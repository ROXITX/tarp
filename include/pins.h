// Pin map for SmartPack. Mirrors PIN_CONFIG.md ("Final Code Definitions") -- keep in sync.
#pragma once

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

// Reed switches (INPUT_PULLUP: CLOSED = LOW, OPEN = HIGH)
#define REED1_PIN  27
#define REED2_PIN  26

// LEDs
#define GREEN_LED_PIN 25
#define RED_LED_PIN   33

// Buzzer
#define BUZZER_PIN 32
