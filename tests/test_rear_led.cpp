#include <assert.h>
#include <stdio.h>
#include "RearLedPattern.h"

int main() {
  using namespace RearLedPattern;
  // Verify the physical rear LED mapping found on the actual CYD: G/B were
  // swapped in older firmware. Pin map must not change NHL team RGB values.
  assert(kRedPin == 4);
  assert(kGreenPin == 17);
  assert(kBluePin == 16);
  assert(kRedPin != kGreenPin && kGreenPin != kBluePin &&
         kRedPin != kBluePin);
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
  puts("PASS rear RGB pin map and active-low flash/pulse timing");
}
