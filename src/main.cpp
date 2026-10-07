// SmartPack firmware (ESP32 + 2x MFRC522 + 2x reed switch + LEDs + buzzer).
//
// Concept:
//  * Each compartment has its own RFID reader at its opening.
//  * An assigned tag scanned once  = item crossed INTO the compartment.
//    Scanned again                 = item crossed OUT. (toggle)
//  * When the compartment's reed switch reports CLOSED, every assigned item
//    must be marked inside, otherwise the red LED/buzzer fire and the app is
//    told exactly which items are missing.
#include <Arduino.h>
#include <MFRC522.h>
#include <SPI.h>
#include <stdarg.h>
#include "ble_link.h"
#include "indicator.h"
#include "pins.h"
#include "smartpack_core.h"
#include "storage.h"

// Scans only count while the compartment is open (that is when items cross it).
static constexpr bool SCAN_ONLY_WHEN_OPEN = true;
static constexpr uint32_t REED_DEBOUNCE_MS = 60;
static constexpr uint32_t SAME_TAG_COOLDOWN_MS = 1500;
static constexpr uint32_t READER_POLL_MS = 60;
static constexpr uint32_t ENROLL_TIMEOUT_MS = 30000;

static MFRC522 readers[sp::NUM_COMPARTMENTS] = {MFRC522(RFID1_SS, RFID1_RST),
                                                MFRC522(RFID2_SS, RFID2_RST)};
static const uint8_t reedPins[sp::NUM_COMPARTMENTS] = {REED1_PIN, REED2_PIN};

static sp::Bag bag;
static Storage storage;
static Indicator indicator;

struct ReedState {
  bool closed = false;     // debounced
  bool raw = false;        // last raw reading
  uint32_t rawSince = 0;   // when raw last changed
};
static ReedState reed[sp::NUM_COMPARTMENTS];

static sp::Uid lastUid[sp::NUM_COMPARTMENTS];
static uint32_t lastUidMs[sp::NUM_COMPARTMENTS];
static int enrollComp = -1;
static uint32_t enrollStart = 0;
static uint8_t readerPollIdx = 0;
static uint32_t lastPoll = 0;

// ---- output: USB serial + BLE ----
static void emit(const char* fmt, ...) {
  char line[ble::LINE_MAX];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(line, sizeof line, fmt, ap);
  va_end(ap);
  Serial.println(line);
  ble::emit(line);
}

static void emitUid(const char* tag, int c, const sp::Uid& u) {
  char hex[2 * sp::MAX_UID + 1];
  u.toHex(hex);
  emit("%s %d %s", tag, c + 1, hex);
}

// ---- reed switches ----
static bool readReedClosed(int c) { return digitalRead(reedPins[c]) == LOW; }

static void verifyCompartment(int c) {
  sp::Uid missing[sp::MAX_ITEMS];
  int n = 0;
  sp::Verify v = bag.verify(c, missing, sp::MAX_ITEMS, n);
  switch (v) {
    case sp::Verify::NoItems:
      emit("VERIFY %d EMPTY 0", c + 1);
      break;
    case sp::Verify::AllPresent:
      emit("VERIFY %d OK 0", c + 1);
      indicator.play(pattern::VERIFIED);
      break;
    case sp::Verify::Missing:
      for (int i = 0; i < n && i < sp::MAX_ITEMS; i++) emitUid("MISS", c, missing[i]);
      emit("VERIFY %d MISSING %d", c + 1, n);
      indicator.play(pattern::MISSING);
      break;
  }
}

static void updateReeds(uint32_t now) {
  for (int c = 0; c < sp::NUM_COMPARTMENTS; c++) {
    bool r = readReedClosed(c);
    if (r != reed[c].raw) {
      reed[c].raw = r;
      reed[c].rawSince = now;
    }
    if (reed[c].raw != reed[c].closed && now - reed[c].rawSince >= REED_DEBOUNCE_MS) {
      reed[c].closed = reed[c].raw;
      if (reed[c].closed) {
        emit("CLOSED %d", c + 1);
        verifyCompartment(c);
      } else {
        emit("OPEN %d", c + 1);
      }
    }
  }
}

// ---- RFID ----
static bool readUid(MFRC522& r, sp::Uid& u) {
  if (!r.PICC_IsNewCardPresent() || !r.PICC_ReadCardSerial()) return false;
  u = sp::Uid();
  u.len = min<uint8_t>(r.uid.size, sp::MAX_UID);
  memcpy(u.b, r.uid.uidByte, u.len);
  r.PICC_HaltA();  // tag must leave the field before it can be read again
  r.PCD_StopCrypto1();
  return true;
}

static void handleScan(int c, const sp::Uid& u) {
  if (enrollComp >= 0) {
    // Enrol mode: whichever reader sees the tag, it is assigned to the requested compartment.
    int target = enrollComp;
    enrollComp = -1;
    bool added = bag.addItem(target, u);
    if (added) storage.save(bag);
    emitUid(added ? "ENROLLED" : "ENROLL_DUP", target, u);
    indicator.play(pattern::ENROLLED);
    return;
  }

  if (SCAN_ONLY_WHEN_OPEN && reed[c].closed) {
    emitUid("IGNORED_CLOSED", c, u);
    return;
  }

  switch (bag.scan(c, u)) {
    case sp::ScanResult::NowInside:
      storage.save(bag);
      emitUid("IN", c, u);
      indicator.play(pattern::IN);
      break;
    case sp::ScanResult::NowOutside:
      storage.save(bag);
      emitUid("OUT", c, u);
      indicator.play(pattern::OUT);
      break;
    case sp::ScanResult::Unknown:
      emitUid("UNKNOWN", c, u);
      indicator.play(pattern::UNKNOWN);
      break;
  }
}

static void pollReaders(uint32_t now) {
  if (now - lastPoll < READER_POLL_MS) return;
  lastPoll = now;
  int c = readerPollIdx;
  readerPollIdx = (readerPollIdx + 1) % sp::NUM_COMPARTMENTS;  // alternate readers

  sp::Uid u;
  if (!readUid(readers[c], u)) return;
  if (u == lastUid[c] && now - lastUidMs[c] < SAME_TAG_COOLDOWN_MS) return;
  lastUid[c] = u;
  lastUidMs[c] = now;
  handleScan(c, u);
}

// ---- commands ----
static void listCompartment(int c) {
  for (int i = 0; i < sp::MAX_ITEMS; i++) {
    const sp::Item& it = bag.items[c][i];
    if (!it.used) continue;
    char hex[2 * sp::MAX_UID + 1];
    it.uid.toHex(hex);
    emit("ITEM %d %s %d", c + 1, hex, it.inside ? 1 : 0);
  }
}

static void handleCommand(const char* line) {
  sp::Command cmd;
  if (!sp::parseCommand(line, cmd)) {
    emit("ERR BAD_COMMAND");
    return;
  }
  int c = cmd.comp;
  switch (cmd.type) {
    case sp::CmdType::Add:
      if (bag.addItem(c, cmd.uid)) {
        storage.save(bag);
        emit("OK ADD");
      } else {
        emit("ERR ADD_FAILED");  // full or already assigned
      }
      break;
    case sp::CmdType::Del:
      if (bag.removeItem(c, cmd.uid)) {
        storage.save(bag);
        emit("OK DEL");
      } else {
        emit("ERR NOT_FOUND");
      }
      break;
    case sp::CmdType::Clear:
      bag.clear(c);
      storage.save(bag);
      emit("OK CLEAR");
      break;
    case sp::CmdType::Set:
      if (bag.setInside(c, cmd.uid, cmd.value)) {
        storage.save(bag);
        emit("OK SET");
      } else {
        emit("ERR NOT_FOUND");
      }
      break;
    case sp::CmdType::List:
      listCompartment(c);
      emit("OK LIST");
      break;
    case sp::CmdType::Enroll:
      enrollComp = c;
      enrollStart = millis();
      emit("OK ENROLL %d", c + 1);
      break;
    case sp::CmdType::Cancel:
      enrollComp = -1;
      emit("OK CANCEL");
      break;
    case sp::CmdType::Status:
      for (int i = 0; i < sp::NUM_COMPARTMENTS; i++)
        emit("STATE %d %s %d %d", i + 1, reed[i].closed ? "CLOSED" : "OPEN", bag.count(i),
             bag.countInside(i));
      emit("OK STATUS");
      break;
    default:
      emit("ERR BAD_COMMAND");
  }
}

static void pollSerialCommands() {
  static char buf[ble::LINE_MAX];
  static size_t n = 0;
  while (Serial.available()) {
    char ch = (char)Serial.read();
    if (ch == '\n' || ch == '\r') {
      if (n) {
        buf[n] = 0;
        handleCommand(buf);
        n = 0;
      }
    } else if (n < sizeof buf - 1) {
      buf[n++] = ch;
    }
  }
}

void setup() {
  Serial.begin(115200);
  indicator.begin();
  for (int c = 0; c < sp::NUM_COMPARTMENTS; c++) pinMode(reedPins[c], INPUT_PULLUP);

  if (!storage.load(bag)) Serial.println("No saved config, starting empty");

  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  bool fault = false;
  for (int c = 0; c < sp::NUM_COMPARTMENTS; c++) {
    readers[c].PCD_Init();
    byte ver = readers[c].PCD_ReadRegister(MFRC522::VersionReg);
    Serial.printf("RFID %d firmware version 0x%02X\n", c + 1, ver);
    if (ver == 0x00 || ver == 0xFF) fault = true;
  }
  if (fault) indicator.play(pattern::FAULT);

  // Adopt the current reed state silently (no verify on boot).
  uint32_t now = millis();
  for (int c = 0; c < sp::NUM_COMPARTMENTS; c++) {
    reed[c].closed = reed[c].raw = readReedClosed(c);
    reed[c].rawSince = now;
  }

  ble::begin("SmartPack");
  emit("READY");
}

void loop() {
  uint32_t now = millis();

  char line[ble::LINE_MAX];
  while (ble::popCommand(line, sizeof line)) handleCommand(line);
  pollSerialCommands();

  updateReeds(now);
  pollReaders(now);

  if (enrollComp >= 0 && now - enrollStart > ENROLL_TIMEOUT_MS) {
    enrollComp = -1;
    emit("ENROLL_TIMEOUT");
  }

  indicator.update();
  ble::pump();
}
