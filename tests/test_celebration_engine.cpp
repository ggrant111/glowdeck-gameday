#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "CelebrationEngine.h"
using namespace CelebrationEngine;
int main() {
  assert(eligible(true, GOAL_FLAG, Event::GOAL));
  assert(!eligible(false, ALL_EVENTS, Event::GOAL));
  assert(!eligible(true, PP_FLAG, Event::GOAL));
  assert(!eligible(true, GOAL_FLAG, Event::FINAL_WIN));
  assert(eligible(true, FINAL_FLAG, Event::FINAL_LOSS));
  assert(duration(Event::GOAL, 8) == 8000UL);
  assert(duration(Event::POWER_PLAY, 8) == 0);
  assert(duration(Event::GAME_START, 8) == 4500UL);
  assert(phase(Event::GOAL, 2000, 8000, 0) == 0);
  assert(phase(Event::GOAL, 3000, 8000, 0) == 1);
  assert(effect(Event::GOAL, 0, 0).fx == 23);
  assert(effect(Event::GOAL, 1, 0).fx == 28);
  assert(effect(Event::GOAL, 1, 1).fx == 23);
  assert(effect(Event::GOAL, 1, 2).fx == 28);
  assert(effect(Event::GOAL, 1, 3).fx == 0);
  assert(effect(Event::POWER_PLAY, 0, 0).fx == 2);
  assert(effect(Event::GAME_START, 0, 0).fx == 3);
  assert(effect(Event::FINAL_WIN, 0, 0).fx == 17);
  assert(effect(Event::FINAL_LOSS, 0, 0).fx == 12);
  assert(transition(false, "PRE", "LIVE", true) == Event::NONE);
  assert(transition(true, "PRE", "LIVE", true) == Event::GAME_START);
  assert(transition(true, "LIVE", "OFF", true) == Event::FINAL_WIN);
  assert(transition(true, "LIVE", "OFF", false) == Event::FINAL_LOSS);
  assert(transition(true, "OFF", "OFF", true) == Event::NONE);
  assert(transition(true, "LIVE", "LIVE", true) == Event::NONE);
  puts("PASS WLED selected-device eligibility, stage timing and NHL events");
}
