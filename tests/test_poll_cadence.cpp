#include <assert.h>
#include <stdio.h>
#include "GameDayCadence.h"

int main() {
  using namespace GameDayCadence;
  assert(intervalMs(true, true, false) == 10000UL);
  assert(intervalMs(true, false, false) == 10000UL);
  assert(intervalMs(false, true, false) == 20000UL);
  assert(intervalMs(false, false, true) == 30000UL);
  assert(intervalMs(false, false, false) == 60000UL);
  assert(intervalMs(false, true, true) == 20000UL);
  assert(intervalMs(true, true, true) == 10000UL);
  puts("PASS: live/armed/followed game polling cadence");
}
