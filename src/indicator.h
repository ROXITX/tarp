// Non-blocking LED + buzzer patterns.
#pragma once
#include <Arduino.h>
#include "pins.h"

struct Step {
  bool green, red, buzz;
  uint16_t ms;
};

namespace pattern {
// Item crossed INTO the compartment: one green flash.
static const Step IN[] = {{1, 0, 1, 80}, {0, 0, 0, 1}};
// Item crossed OUT: two green flashes.
static const Step OUT[] = {{1, 0, 1, 80}, {0, 0, 0, 100}, {1, 0, 1, 80}, {0, 0, 0, 1}};
// Closed and every item present.
static const Step VERIFIED[] = {{1, 0, 1, 150}, {1, 0, 0, 1350}, {0, 0, 0, 1}};
// Closed with items missing.
static const Step MISSING[] = {{0, 1, 1, 200}, {0, 1, 0, 100}, {0, 1, 1, 200}, {0, 1, 0, 100},
                               {0, 1, 1, 200}, {0, 1, 0, 2000}, {0, 0, 0, 1}};
// Tag scanned that is not assigned to this compartment.
static const Step UNKNOWN[] = {{0, 1, 1, 60}, {0, 1, 0, 100}, {0, 0, 0, 1}};
// Enrol captured a tag.
static const Step ENROLLED[] = {{1, 0, 1, 70}, {0, 0, 0, 70}, {1, 0, 1, 70}, {0, 0, 0, 70},
                                {1, 0, 1, 70}, {0, 0, 0, 1}};
// Reader hardware not responding.
static const Step FAULT[] = {{0, 1, 1, 500}, {0, 0, 0, 200}, {0, 1, 1, 500}, {0, 0, 0, 1}};
}  // namespace pattern

class Indicator {
 public:
  void begin() {
    pinMode(GREEN_LED_PIN, OUTPUT);
    pinMode(RED_LED_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    apply(false, false, false);
  }

  template <size_t N>
  void play(const Step (&steps)[N]) {
    steps_ = steps;
    n_ = N;
    i_ = 0;
    t0_ = millis();
    active_ = true;
    applyStep();
  }

  void update() {
    if (!active_) return;
    if (millis() - t0_ < steps_[i_].ms) return;
    if (++i_ >= n_) {
      active_ = false;
      apply(false, false, false);
      return;
    }
    t0_ = millis();
    applyStep();
  }

 private:
  const Step* steps_ = nullptr;
  uint8_t n_ = 0, i_ = 0;
  uint32_t t0_ = 0;
  bool active_ = false;

  void applyStep() { apply(steps_[i_].green, steps_[i_].red, steps_[i_].buzz); }
  static void apply(bool g, bool r, bool b) {
    digitalWrite(GREEN_LED_PIN, g);
    digitalWrite(RED_LED_PIN, r);
    digitalWrite(BUZZER_PIN, b);
  }
};
