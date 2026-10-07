# SmartPack ESP32 – Pin Configuration

## Final GPIO Configuration

| Component | Signal / Pin | ESP32 GPIO |
|---|---|---:|
| RFID 1 | SDA / SS / CS | GPIO 5 |
| RFID 1 | RST | GPIO 21 |
| RFID 1 | SCK | GPIO 18 |
| RFID 1 | MISO | GPIO 19 |
| RFID 1 | MOSI | GPIO 23 |
| RFID 1 | VCC | 3V3 |
| RFID 1 | GND | GND |
| RFID 2 | SDA / SS / CS | GPIO 16 |
| RFID 2 | RST | GPIO 17 |
| RFID 2 | SCK | GPIO 18 |
| RFID 2 | MISO | GPIO 19 |
| RFID 2 | MOSI | GPIO 23 |
| RFID 2 | VCC | 3V3 |
| RFID 2 | GND | GND |
| Reed Switch 1 | Signal | GPIO 27 |
| Reed Switch 1 | Other wire | GND |
| Reed Switch 2 | Signal | GPIO 26 |
| Reed Switch 2 | Other wire | GND |
| Green LED | Anode (+) | GPIO 25 through 220–330Ω |
| Green LED | Cathode (-) | GND |
| Red LED | Anode (+) | GPIO 33 through 220–330Ω |
| Red LED | Cathode (-) | GND |
| Buzzer | Positive (+) | GPIO 32 |
| Buzzer | Negative (-) | GND |
| BLE | Internal ESP32 | No external GPIO |
| Wi-Fi | Internal ESP32 | No external GPIO |

## RFID SPI

Both RFID readers share:

```text
SCK  = GPIO 18
MISO = GPIO 19
MOSI = GPIO 23
```

Unique pins:

```text
RFID 1:
SS  = GPIO 5
RST = GPIO 21

RFID 2:
SS  = GPIO 16
RST = GPIO 17
```

### RFID 1 – Compartment 1

```text
SDA / SS → GPIO 5
SCK      → GPIO 18
MOSI     → GPIO 23
MISO     → GPIO 19
RST      → GPIO 21
3.3V     → ESP32 3V3
GND      → ESP32 GND
IRQ      → Not connected
```

### RFID 2 – Compartment 2

```text
SDA / SS → GPIO 16
SCK      → GPIO 18
MOSI     → GPIO 23
MISO     → GPIO 19
RST      → GPIO 17
3.3V     → ESP32 3V3
GND      → ESP32 GND
IRQ      → Not connected
```

## Reed Switches

### Compartment 1

```text
GPIO27 ───── Reed Switch 1 ───── GND
```

```cpp
#define REED1_PIN 27
pinMode(REED1_PIN, INPUT_PULLUP);
```

Normal 2-wire reed switch: either physical wire can go to GPIO27.

Logic:

```text
CLOSED = LOW
OPEN   = HIGH
```

### Compartment 2

```text
GPIO26 ───── Reed Switch 2 ───── GND
```

```cpp
#define REED2_PIN 26
pinMode(REED2_PIN, INPUT_PULLUP);
```

Logic:

```text
CLOSED = LOW
OPEN   = HIGH
```

## LEDs

### Green LED

Purpose: Correct / Verified

```text
GPIO25 ── 220Ω/330Ω ── LED Long Leg (+)
                         LED Short Leg (-) ── GND
```

```cpp
#define GREEN_LED_PIN 25
pinMode(GREEN_LED_PIN, OUTPUT);
```

### Red LED

Purpose: Wrong / Missing / Error

```text
GPIO33 ── 220Ω/330Ω ── LED Long Leg (+)
                         LED Short Leg (-) ── GND
```

```cpp
#define RED_LED_PIN 33
pinMode(RED_LED_PIN, OUTPUT);
```

## Buzzer

For a small low-current active buzzer:

```text
GPIO32 ───── Buzzer +
GND   ────── Buzzer -
```

```cpp
#define BUZZER_PIN 32
pinMode(BUZZER_PIN, OUTPUT);
```

For a 5V/high-current buzzer, use a transistor/MOSFET driver.

## Power Configuration

### Battery → TP4056

```text
Battery + (red)   → TP4056 B+
Battery - (black) → TP4056 B-
```

### TP4056 → MT3608

```text
TP4056 OUT+ → MT3608 IN+
TP4056 OUT- → MT3608 IN-
```

### MT3608 → ESP32

Set MT3608 output to approximately 5.0V using a multimeter before connecting the ESP32.

```text
MT3608 OUT+ → ESP32 VIN / 5V
MT3608 OUT- → ESP32 GND
```

Then:

```text
ESP32 3V3 → RFID 1 VCC
ESP32 3V3 → RFID 2 VCC
```

## Critical RFID Power Rule

MFRC522 VCC must be connected to **ESP32 3V3**.

Do NOT connect MFRC522 VCC to:

```text
5V
VIN
MT3608 OUT+
Li-Po battery directly
```

## Common Ground

```text
ESP32 GND
   ├── RFID 1 GND
   ├── RFID 2 GND
   ├── Reed 1 GND
   ├── Reed 2 GND
   ├── Green LED GND
   ├── Red LED GND
   ├── Buzzer GND
   └── MT3608 OUT-
```

## Final Code Definitions

```cpp
// RFID 1 - Compartment 1
#define RFID1_SS   5
#define RFID1_RST  21

// RFID 2 - Compartment 2
#define RFID2_SS   16
#define RFID2_RST  17

// Shared SPI
#define SPI_SCK    18
#define SPI_MISO   19
#define SPI_MOSI   23

// Reed switches
#define REED1_PIN  27
#define REED2_PIN  26

// LEDs
#define GREEN_LED_PIN 25
#define RED_LED_PIN   33

// Buzzer
#define BUZZER_PIN 32
```

SPI initialization:

```cpp
SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
```

## Final GPIO List

```text
GPIO 5   → RFID 1 SS
GPIO 16  → RFID 2 SS
GPIO 17  → RFID 2 RST
GPIO 18  → SPI SCK
GPIO 19  → SPI MISO
GPIO 21  → RFID 1 RST
GPIO 23  → SPI MOSI
GPIO 25  → Green LED
GPIO 26  → Reed Switch 2
GPIO 27  → Reed Switch 1
GPIO 32  → Buzzer
GPIO 33  → Red LED
```

## Functional Meaning

| Hardware | Function |
|---|---|
| RFID 1 | Identifies item in Compartment 1 |
| RFID 2 | Identifies item in Compartment 2 |
| Reed 1 | Detects Compartment 1 closure/opening |
| Reed 2 | Detects Compartment 2 closure/opening |
| Green LED | Correct item / successful verification |
| Red LED | Wrong item / missing item / error |
| Buzzer | Audible warning/alert |
| BLE | Sends status to mobile application |
| Battery | Portable power |
| TP4056 | Battery charging/protection |
| MT3608 | Boosts battery voltage to 5V |
| ESP32 3V3 | Supplies RFID readers |

## Safety Rules

1. MFRC522 VCC → ESP32 3V3 only.
2. Never connect MFRC522 VCC to 5V.
3. Never connect MFRC522 VCC directly to the Li-Po battery.
4. Adjust MT3608 to approximately 5V before connecting it to ESP32 VIN/5V.
5. Use a 220Ω or 330Ω resistor with each LED.
6. All grounds must be common.
7. IRQ on both MFRC522 readers is not connected.
8. Both RFID readers share SCK, MISO and MOSI.
9. RFID readers use separate SS/CS pins.
10. RFID readers use separate RST pins.

## Board Note

This configuration is intended for a classic ESP32-WROOM / ESP32 DevKit-style board.

The classic ESP32 Arduino mapping includes:

```text
SS   = GPIO5
MOSI = GPIO23
MISO = GPIO19
SCK  = GPIO18
```

GPIO16 and GPIO17 are assigned here to RFID 2 SS and RST. Verify that the exact ESP32 board being used exposes these pins and does not reserve them for another onboard function before final assembly.

For a different ESP32 family such as ESP32-S3, C3, C5, etc., verify the board-specific pinout before using this configuration.

## Final Architecture

```text
                    SMARTPACK
                       |
                      ESP32
                       |
       +---------------+---------------+
       |               |               |
     RFID 1          RFID 2          Sensors
       |               |               |
   Compartment 1   Compartment 2    Reed 1/2
       |               |               |
     GPIO5          GPIO16         GPIO27/26
     GPIO21         GPIO17
       |               |
       +------- SPI ---+
          18/19/23

       +-------------------------------+
       |                               |
   GPIO25                          GPIO33
 Green LED                         Red LED
       |                               |
       +---------------+---------------+
                       |
                    GPIO32
                    Buzzer

Power:
Li-Po
  ↓
TP4056
  ↓
MT3608 (~5V)
  ↓
ESP32 VIN/5V
  ↓
ESP32 3V3
  ↓
RFID 1 + RFID 2
```
