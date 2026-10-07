#include "ble_link.h"
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace ble {

static const char* SERVICE_UUID = "6e5a0001-5b1c-4f6e-9a3d-2f7d4c0b5a10";
static const char* CMD_UUID = "6e5a0002-5b1c-4f6e-9a3d-2f7d4c0b5a10";    // app -> bag (write)
static const char* EVENT_UUID = "6e5a0003-5b1c-4f6e-9a3d-2f7d4c0b5a10";  // bag -> app (notify)

struct Line {
  char s[LINE_LEN];
};

static QueueHandle_t cmdQ;
static BLECharacteristic* eventChar;
static volatile bool isConnected = false;

// Event ring buffer, only touched from the main loop.
constexpr int EVQ = 24;
static Line evq[EVQ];
static int evHead = 0, evTail = 0;
static uint32_t lastSend = 0;

class ServerCb : public BLEServerCallbacks {
  void onConnect(BLEServer*) override { isConnected = true; }
  void onDisconnect(BLEServer*) override {
    isConnected = false;
    BLEDevice::startAdvertising();
  }
};

class CmdCb : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* c) override {
    auto v = c->getValue();
    Line l;
    size_t n = v.length() < LINE_LEN - 1 ? v.length() : LINE_LEN - 1;
    memcpy(l.s, v.c_str(), n);
    l.s[n] = 0;
    xQueueSend(cmdQ, &l, 0);  // runs in the BLE task; main loop does the work
  }
};

void begin(const char* deviceName) {
  cmdQ = xQueueCreate(8, sizeof(Line));
  BLEDevice::init(deviceName);
  BLEDevice::setMTU(185);
  BLEServer* server = BLEDevice::createServer();
  server->setCallbacks(new ServerCb());
  BLEService* svc = server->createService(SERVICE_UUID);

  BLECharacteristic* cmd = svc->createCharacteristic(
      CMD_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  cmd->setCallbacks(new CmdCb());

  eventChar = svc->createCharacteristic(EVENT_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  eventChar->addDescriptor(new BLE2902());

  svc->start();
  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->setScanResponse(true);
  BLEDevice::startAdvertising();
}

bool popCommand(char* out, size_t cap) {
  Line l;
  if (xQueueReceive(cmdQ, &l, 0) != pdTRUE) return false;
  strlcpy(out, l.s, cap);
  return true;
}

void emit(const char* line) {
  int next = (evHead + 1) % EVQ;
  if (next == evTail) evTail = (evTail + 1) % EVQ;  // overflow: drop oldest
  strlcpy(evq[evHead].s, line, LINE_LEN);
  evHead = next;
}

void pump() {
  if (evHead == evTail) return;
  if (!isConnected) {  // nobody listening; don't let stale events pile up
    evTail = evHead;
    return;
  }
  if (millis() - lastSend < 25) return;
  lastSend = millis();
  const char* s = evq[evTail].s;
  eventChar->setValue((uint8_t*)s, strlen(s));
  eventChar->notify();
  evTail = (evTail + 1) % EVQ;
}

bool connected() { return isConnected; }

}  // namespace ble
