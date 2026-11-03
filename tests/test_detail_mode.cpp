#include <assert.h>
#include <stdio.h>
#include "GameDetailMode.h"

int main() {
  using namespace GameDetailMode;
  assert(isPregame("FUT"));
  assert(isPregame("PRE"));
  assert(!isPregame("LIVE"));
  assert(!isPregame("CRIT"));
  assert(!isPregame("OFF"));
  assert(!needsModeSwitch(true, true, "PRE"));
  assert(needsModeSwitch(true, true, "LIVE")); // puck drop
  assert(needsModeSwitch(true, false, "FUT")); // game reset to pregame
  assert(!needsModeSwitch(true, false, "OFF")); // completed remains game stats
  assert(!needsModeSwitch(false, true, "LIVE")); // no cache to invalidate
  puts("PASS: pregame-to-live details switch");
}
