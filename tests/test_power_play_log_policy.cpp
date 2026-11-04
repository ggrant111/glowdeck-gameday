#include <assert.h>
#include <stdio.h>
#include "PowerPlayLogPolicy.h"
using namespace PowerPlayLogPolicy;
int main() {
  Watch w;
  Change c=observe(w, 2026020253UL, true, 2, 1000);
  assert(c.started == 2 && c.firstObservation && !c.ended);
  for(int i=0;i<25;++i) {
    c=observe(w, 2026020253UL, true, 2, 1100+i*15000);
    assert(!c.started && !c.ended); // Same PP, no duplicates.
    c=observe(w, 2026020253UL, false, 0, 1200+i*15000);
    assert(!c.started && !c.ended); // Missing API data is not an end.
  }
  c=observe(w, 2026020253UL, true, 0, 410000);
  assert(!c.ended); // Single temporary "no PP".
  c=observe(w, 2026020253UL, true, 2, 410300);
  assert(!c.started && !c.ended); // PP still in progress.
  c=observe(w, 2026020253UL, true, 0, 420000);
  assert(!c.ended);
  c=observe(w, 2026020253UL, true, 0, 420500);
  assert(!c.ended); // Too soon, repeated fallback sample.
  c=observe(w, 2026020253UL, true, 0, 430100);
  assert(c.ended == 2 && !c.started); // Confirmed after 10s.
  c=observe(w, 2026020253UL, true, 0, 440000);
  assert(!c.started && !c.ended);
  c=observe(w, 2026020253UL, true, 2, 451000);
  assert(c.started == 2 && !c.ended); // Distinct later PP.
  c=observe(w, 2026020253UL, true, 1, 470000);
  assert(c.ended == 2 && c.started == 1); // Team switch.
  c=observe(w, 2026020254UL, false, 2, 480000);
  assert(!c.started && !c.ended && w.gameId == 2026020253UL);
  c=observe(w, 2026020254UL, true, 2, 490000);
  assert(c.firstObservation && c.started == 2);
  assert(w.gameId == 2026020254UL);
  puts("PASS: id-stable PP log, missing data ignored, timed end confirmation, no repeated starts");
}
