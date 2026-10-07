// BLE link to the mobile app: one write characteristic for commands (text lines),
// one notify characteristic for events (text lines). See docs/PROTOCOL.md.
#pragma once
#include <Arduino.h>

namespace ble {

constexpr size_t LINE_LEN = 64;

void begin(const char* deviceName);
// Pops one pending command line written by the app. Call from the main loop.
bool popCommand(char* out, size_t cap);
// Queues an event line; sent to the app (if connected) from pump().
void emit(const char* line);
// Drains the event queue at a BLE-friendly rate. Call from the main loop.
void pump();
bool connected();

}  // namespace ble
