# SmartPack

ESP32 smart bag: two compartments, each with an MFRC522 RFID reader and a reed switch.

* RFID scan = item crossing the compartment opening. 1st scan = **IN**, next scan = **OUT**, and so on.
* When a compartment is closed (reed switch), every item assigned to it must be IN, otherwise red LED + buzzer + BLE alert naming the missing items.
* Wiring: see [`PIN_CONFIG.md`](PIN_CONFIG.md) (unchanged). Pins in code: `include/pins.h`.
* App link: [`docs/PROTOCOL.md`](docs/PROTOCOL.md).

## Layout

| Path | What |
|---|---|
| `include/smartpack_core.h` | Hardware-independent logic (toggle, verify, command parser) |
| `src/main.cpp` | Firmware main loop |
| `src/ble_link.*`, `src/indicator.h`, `src/storage.h` | BLE, LED/buzzer patterns, NVS persistence |
| `test/host/test_core.cpp` | PC unit tests for the core |

## Build / test

```bash
pio run -t upload && pio device monitor          # firmware (PlatformIO)
g++ -std=c++17 -Iinclude test/host/test_core.cpp -o test/host/test_core && test/host/test_core
```

Bench test without the app: open the serial monitor and type e.g. `ENROLL 1`, tap a tag, then tap it again to see `IN` / `OUT`.
