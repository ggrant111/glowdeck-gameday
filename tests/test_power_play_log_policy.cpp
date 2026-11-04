#include <assert.h>
#include <stdio.h>
#include "PowerPlayLogPolicy.h"
using namespace PowerPlayLogPolicy;
int main() {
  Watch w;
  // First positive detection generates one "observed" start.
  Change c=observe(w, 2026020253UL, true, 2);
  assert(c.started == 2 && c.firstObservation && !c.ended);
  for(int i=0;i<25;++i) {
    c=observe(w, 2026020253UL, true, 2);
    assert(!c.started && !c.ended); // No duplicate while PP persists.
    c=observe(w, 2026020253UL, false, 0);
    assert(!c.started && !c.ended); // Missing API data is not a PP end.
  }
  c=observe(w, 2026020253UL, true, 0);
  assert(!c.ended); // Short intermittent "no PP" reading.
  c=observe(w, 2026020253UL, true, 2);
  assert(!c.started && !c.ended); // Still same power play.
  c=observe(w, 2026020253UL, true, 0);
  assert(!c.ended);
  c=observe(w, 2026020253UL, true, 0);
  assert(c.ended == 2 && !c.started); // Confirmed end.
  c=observe(w, 2026020253UL, true, 0);
  assert(!c.started && !c.ended);
  c=observe(w, 2026020253UL, true, 2);
  assert(c.started == 2 && !c.ended); // Distinct later PP.
  c=observe(w, 2026020253UL, true, 1);
  assert(c.ended == 2 && c.started == 1); // Rare opposite side switch.
  c=observe(w, 2026020254UL, false, 2);
  assert(!c.started && !c.ended && w.gameId == 2026020253UL);
  c=observe(w, 2026020254UL, true, 2);
  assert(c.firstObservation && c.started == 2);
  assert(w.gameId == 2026020254UL);
  puts("PASS: id-stable PP log, ignored missing situation, debounced ends, no repeated starts");
}
