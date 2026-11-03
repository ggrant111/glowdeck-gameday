#include <assert.h>
#include <stdio.h>
#include "RearLedPattern.h"

int main() {
  using namespace RearLedPattern;
  assert(flash(0, 6000) == 255);
  assert(flash(200, 6000) == 0);
  assert(flash(260, 6000) == 255);
  assert(flash(6000, 6000) == 0);
  assert(flash(5900, 6000) <= 52);
  assert(pulse(0) == 0);
  assert(pulse(900) == 210);
  assert(pulse(1800) == 0);
  assert(invertedDuty(255, 255, 255) == 0);
  assert(invertedDuty(0, 255, 255) == 255);
  assert(invertedDuty(255, 0, 255) == 255);
  puts("PASS rear RGB active-low flash/pulse timing");
}
