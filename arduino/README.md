# SmartPack – Arduino IDE sketch

Same firmware as the PlatformIO project (`../src`, `../include`), laid out as an Arduino sketch.
This folder is a copy; if you change the firmware, change `src/`/`include/` and re-copy (see bottom).

## Setup (once)
1. Install **Arduino IDE 2.x**.
2. **File → Preferences → Additional boards manager URLs**, add:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
3. **Tools → Board → Boards Manager**, install **esp32 by Espressif Systems** (2.0.17 or 3.x).
4. **Tools → Manage Libraries**, install **MFRC522v2** (OSSLibraries; do not install the older MFRC522 as well).

## Open and upload
1. Open `arduino/SmartPack/SmartPack.ino` (keep the folder name `SmartPack`).
2. **Tools → Board → ESP32 Arduino → ESP32 Dev Module**
3. **Tools → Partition Scheme → Huge APP (3MB No OTA/1MB SPIFFS)** (BLE + Wi-Fi stack is large)
4. Pick the COM port, click Upload. If it hangs on "Connecting...", hold the BOOT button.
5. **Tools → Serial Monitor**, 115200 baud, "Newline" line ending.

## Bench test
Type `ENROLL 1`, tap a tag on reader 1 -> `ENROLLED 1 <UID>`.
Tap it again -> `IN 1 <UID>`, again -> `OUT 1 <UID>`. Short the reed pin to GND (closed) -> `CLOSED 1` and a verify result.
Commands: see `../docs/PROTOCOL.md`.

## Re-copy after changing the firmware
```bash
cp src/main.cpp arduino/SmartPack/SmartPack.ino
cp src/ble_link.* src/indicator.h src/storage.h include/pins.h include/smartpack_core.h arduino/SmartPack/
```
