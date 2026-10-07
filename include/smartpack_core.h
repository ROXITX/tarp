// Hardware-independent SmartPack logic: tag IDs, per-compartment item list,
// IN/OUT toggling, closure verification and the text command parser.
// No Arduino dependencies so it can be unit-tested on a PC (test/host).
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

namespace sp {

constexpr int NUM_COMPARTMENTS = 2;
constexpr int MAX_ITEMS = 16;   // tracked items per compartment
constexpr int MAX_UID = 10;     // MFRC522 UIDs are 4, 7 or 10 bytes

struct Uid {
  uint8_t b[MAX_UID];
  uint8_t len;

  Uid() : len(0) { memset(b, 0, sizeof b); }

  bool operator==(const Uid& o) const { return len == o.len && memcmp(b, o.b, len) == 0; }

  // out must hold 2*MAX_UID+1 chars
  void toHex(char* out) const {
    for (int i = 0; i < len; i++) sprintf(out + 2 * i, "%02X", b[i]);
    out[2 * len] = 0;
  }

  static bool fromHex(const char* s, Uid& out) {
    size_t n = strlen(s);
    if (n < 2 || n % 2 || n / 2 > MAX_UID) return false;
    Uid u;
    for (size_t i = 0; i < n / 2; i++) {
      int hi = hexVal(s[2 * i]), lo = hexVal(s[2 * i + 1]);
      if (hi < 0 || lo < 0) return false;
      u.b[i] = (uint8_t)(hi << 4 | lo);
    }
    u.len = (uint8_t)(n / 2);
    out = u;
    return true;
  }

 private:
  static int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  }
};

struct Item {
  Uid uid;
  bool used;
  bool inside;  // toggled by each scan: first scan = IN, next = OUT, ...
  Item() : used(false), inside(false) {}
};

enum class ScanResult : uint8_t { Unknown, NowInside, NowOutside };
enum class Verify : uint8_t { NoItems, AllPresent, Missing };

// Plain data (safe to memcpy into flash/NVS).
struct Bag {
  Item items[NUM_COMPARTMENTS][MAX_ITEMS];

  int find(int c, const Uid& u) const {
    for (int i = 0; i < MAX_ITEMS; i++)
      if (items[c][i].used && items[c][i].uid == u) return i;
    return -1;
  }

  // Newly added items start OUTSIDE: the first scan puts them IN.
  // Returns false if the compartment is full or the tag is already assigned there.
  bool addItem(int c, const Uid& u) {
    if (find(c, u) >= 0) return false;
    for (int i = 0; i < MAX_ITEMS; i++) {
      if (!items[c][i].used) {
        items[c][i].uid = u;
        items[c][i].used = true;
        items[c][i].inside = false;
        return true;
      }
    }
    return false;
  }

  bool removeItem(int c, const Uid& u) {
    int i = find(c, u);
    if (i < 0) return false;
    items[c][i] = Item();
    return true;
  }

  void clear(int c) {
    for (int i = 0; i < MAX_ITEMS; i++) items[c][i] = Item();
  }

  bool setInside(int c, const Uid& u, bool inside) {
    int i = find(c, u);
    if (i < 0) return false;
    items[c][i].inside = inside;
    return true;
  }

  // One RFID scan = one crossing of the compartment opening.
  ScanResult scan(int c, const Uid& u) {
    int i = find(c, u);
    if (i < 0) return ScanResult::Unknown;
    items[c][i].inside = !items[c][i].inside;
    return items[c][i].inside ? ScanResult::NowInside : ScanResult::NowOutside;
  }

  int count(int c) const {
    int n = 0;
    for (int i = 0; i < MAX_ITEMS; i++) n += items[c][i].used;
    return n;
  }

  int countInside(int c) const {
    int n = 0;
    for (int i = 0; i < MAX_ITEMS; i++) n += items[c][i].used && items[c][i].inside;
    return n;
  }

  // Called when a compartment is closed: every assigned item must be inside.
  Verify verify(int c, Uid* missing, int cap, int& nMissing) const {
    nMissing = 0;
    if (count(c) == 0) return Verify::NoItems;
    for (int i = 0; i < MAX_ITEMS; i++) {
      if (items[c][i].used && !items[c][i].inside) {
        if (nMissing < cap) missing[nMissing] = items[c][i].uid;
        nMissing++;
      }
    }
    return nMissing ? Verify::Missing : Verify::AllPresent;
  }
};

// ---- text commands (BLE write characteristic and USB serial) ----
//   ADD <c> <UIDHEX>      assign tag to compartment c (1..2)
//   DEL <c> <UIDHEX>      unassign tag
//   CLEAR <c>             remove all items of compartment
//   SET <c> <UIDHEX> <0|1> force an item's state (0 = outside, 1 = inside)
//   LIST <c>              dump items of a compartment
//   ENROLL <c>            next scanned tag is reported (and added) to compartment c
//   CANCEL                leave enroll mode
//   STATUS                dump state of all compartments
enum class CmdType : uint8_t { Invalid, Add, Del, Clear, Set, List, Enroll, Cancel, Status };

struct Command {
  CmdType type;
  int comp;  // 0-based
  Uid uid;
  bool value;
  Command() : type(CmdType::Invalid), comp(0), value(false) {}
};

inline bool parseComp(const char* s, int& c) {
  if (!s) return false;
  char* end;
  long v = strtol(s, &end, 10);
  if (*end || v < 1 || v > NUM_COMPARTMENTS) return false;
  c = (int)v - 1;
  return true;
}

inline bool parseCommand(const char* line, Command& out) {
  char buf[64];
  if (strlen(line) >= sizeof buf) return false;
  strcpy(buf, line);
  char* save = nullptr;
  char* tok[4] = {nullptr, nullptr, nullptr, nullptr};
  int n = 0;
  for (char* t = strtok_r(buf, " \t\r\n", &save); t; t = strtok_r(nullptr, " \t\r\n", &save)) {
    if (n == 4) return false;
    tok[n++] = t;
  }
  if (n == 0) return false;

  Command c;
  auto is = [&](const char* w) { return strcasecmp(tok[0], w) == 0; };

  if (is("ADD") || is("DEL")) {
    if (n != 3 || !parseComp(tok[1], c.comp) || !Uid::fromHex(tok[2], c.uid)) return false;
    c.type = is("ADD") ? CmdType::Add : CmdType::Del;
  } else if (is("SET")) {
    if (n != 4 || !parseComp(tok[1], c.comp) || !Uid::fromHex(tok[2], c.uid)) return false;
    if (strcmp(tok[3], "0") && strcmp(tok[3], "1")) return false;
    c.value = tok[3][0] == '1';
    c.type = CmdType::Set;
  } else if (is("CLEAR") || is("LIST") || is("ENROLL")) {
    if (n != 2 || !parseComp(tok[1], c.comp)) return false;
    c.type = is("CLEAR") ? CmdType::Clear : is("LIST") ? CmdType::List : CmdType::Enroll;
  } else if (is("CANCEL") || is("STATUS")) {
    if (n != 1) return false;
    c.type = is("CANCEL") ? CmdType::Cancel : CmdType::Status;
  } else {
    return false;
  }
  out = c;
  return true;
}

}  // namespace sp
