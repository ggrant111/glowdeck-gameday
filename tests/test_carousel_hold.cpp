#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "CarouselHoldPolicy.h"

struct Game { uint32_t id; };
int main() {
  using namespace CarouselHoldPolicy;
  State hold;
  Game oldOrder[] = {{100}, {200}, {300}};
  Game newOrder[] = {{300}, {100}, {200}};
  assert(!hold.paused && hold.index(oldOrder, 3) == 255);
  hold.toggle(200);
  assert(hold.paused && hold.gameId == 200);
  assert(hold.index(oldOrder, 3) == 1);
  assert(hold.index(newOrder, 3) == 2); // NHL reorders without losing focus.
  hold.navigatedTo(300);
  assert(hold.paused && hold.index(newOrder, 3) == 0);
  hold.navigatedTo(0); // Swiped to summary/standings.
  assert(!hold.paused && hold.gameId == 0);
  hold.toggle(100);
  hold.toggle(100); // Pause button now displays play and resumes cycle.
  assert(!hold.paused && hold.index(oldOrder, 3) == 255);
  hold.toggle(0); // No game cannot be pinned.
  assert(!hold.paused);
  hold.toggle(100);
  assert(hold.index(newOrder, 0) == 255); // Missing game resets in caller.
  hold.reset(); // New local day clears any stale lock.
  assert(!hold.paused);
  puts("PASS display-only hold: toggle, navigation, reordering, reset");
}
