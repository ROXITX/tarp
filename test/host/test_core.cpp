// Host-side tests for smartpack_core.h:  g++ -std=c++17 -Iinclude test/host/test_core.cpp -o test/host/test_core && test/host/test_core
#include <assert.h>
#include <stdio.h>
#include "smartpack_core.h"
using namespace sp;

static Uid U(const char* h) { Uid u; bool ok = Uid::fromHex(h, u); assert(ok); (void)ok; return u; }

int main() {
  // hex round trip
  char hex[21];
  U("04a1B2c3").toHex(hex);
  assert(!strcmp(hex, "04A1B2C3"));
  Uid bad;
  assert(!Uid::fromHex("0", bad) && !Uid::fromHex("ZZ", bad) && !Uid::fromHex("0102030405060708090A0B", bad));

  Bag bag;
  Uid wallet = U("04A1B2C3"), keys = U("DEADBEEF");
  assert(bag.addItem(0, wallet) && bag.addItem(0, keys));
  assert(!bag.addItem(0, wallet));          // duplicate
  assert(bag.addItem(1, wallet));           // same tag may live in the other compartment

  Uid missing[MAX_ITEMS]; int n;
  assert(bag.verify(1, missing, MAX_ITEMS, n) == Verify::Missing && n == 1);  // not scanned in yet
  Bag empty;
  assert(empty.verify(0, missing, MAX_ITEMS, n) == Verify::NoItems);

  // scan toggles IN / OUT
  assert(bag.scan(0, wallet) == ScanResult::NowInside);
  assert(bag.verify(0, missing, MAX_ITEMS, n) == Verify::Missing && n == 1 && missing[0] == keys);
  assert(bag.scan(0, keys) == ScanResult::NowInside);
  assert(bag.verify(0, missing, MAX_ITEMS, n) == Verify::AllPresent && n == 0);
  assert(bag.scan(0, keys) == ScanResult::NowOutside);   // taken back out
  assert(bag.verify(0, missing, MAX_ITEMS, n) == Verify::Missing);
  assert(bag.scan(0, U("01020304")) == ScanResult::Unknown);
  assert(bag.countInside(0) == 1 && bag.count(0) == 2);

  // set / remove / clear / capacity
  assert(bag.setInside(0, keys, true) && !bag.setInside(0, U("01020304"), true));
  assert(bag.removeItem(0, keys) && !bag.removeItem(0, keys));
  bag.clear(0);
  assert(bag.count(0) == 0);
  for (int i = 0; i < MAX_ITEMS; i++) { Uid u; u.len = 4; u.b[0] = (uint8_t)i; assert(bag.addItem(0, u)); }
  assert(!bag.addItem(0, U("FFFFFFFF")));

  // parser
  Command c;
  assert(parseCommand("add 2 04A1B2C3", c) && c.type == CmdType::Add && c.comp == 1 && c.uid == wallet);
  assert(parseCommand("SET 1 04A1B2C3 1", c) && c.type == CmdType::Set && c.value);
  assert(parseCommand("ENROLL 1\n", c) && c.type == CmdType::Enroll && c.comp == 0);
  assert(parseCommand("status", c) && c.type == CmdType::Status);
  assert(!parseCommand("ADD 3 04A1B2C3", c));   // no compartment 3
  assert(!parseCommand("ADD 1", c));
  assert(!parseCommand("SET 1 04A1B2C3 2", c));
  assert(!parseCommand("", c) && !parseCommand("HELLO", c));

  puts("all core tests passed");
  return 0;
}
