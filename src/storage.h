// Persists the bag configuration + IN/OUT state in ESP32 NVS flash.
#pragma once
#include <Preferences.h>
#include "smartpack_core.h"

class Storage {
 public:
  bool load(sp::Bag& bag) {
    Preferences p;
    p.begin("smartpack", true);
    bool ok = p.getBytesLength(KEY) == sizeof(Blob);
    if (ok) {
      Blob b;
      p.getBytes(KEY, &b, sizeof b);
      ok = b.magic == MAGIC;
      if (ok) bag = b.bag;
    }
    p.end();
    return ok;
  }

  void save(const sp::Bag& bag) {
    Blob b;
    b.magic = MAGIC;
    b.bag = bag;
    Preferences p;
    p.begin("smartpack", false);
    p.putBytes(KEY, &b, sizeof b);
    p.end();
  }

 private:
  // Bump MAGIC when sp::Bag layout changes (e.g. MAX_ITEMS) to discard old data.
  static constexpr uint32_t MAGIC = 0x53504B01;
  static constexpr const char* KEY = "bag";
  struct Blob {
    uint32_t magic;
    sp::Bag bag;
  };
};
