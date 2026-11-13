#include <assert.h>
#include <stdio.h>
#include "NhlShootoutPolicy.h"
#include "CelebrationEngine.h"
#include "GameEventLog.h"
#include "EventOverlay.h"
#include "WledZonePolicy.h"
using namespace NhlShootoutPolicy;
int main(){
  Watch w;
  assert(!w.observe(2026020307U,true,"SO")); // boot into SO: no replay
  assert(w.active&&!w.transitionSeen);
  assert(w.homeAttempts==0&&w.awayGoals==0);
  w.playsKnown=true;w.lastSort=150;
  assert(classify("REG",505)==Attempt::NONE);
  assert(classify("OT",505)==Attempt::NONE);
  assert(classify("SO",505)==Attempt::GOAL);
  assert(classify("SO",506)==Attempt::SAVE);
  assert(classify("SO",507)==Attempt::MISS);
  assert(classify("SO",537)==Attempt::MISS);
  assert(classify("SO",502)==Attempt::NONE);
  w.record(1,Attempt::GOAL);w.record(2,Attempt::SAVE);
  w.record(1,Attempt::MISS);w.record(0,Attempt::GOAL);
  assert(w.homeAttempts==2&&w.homeGoals==1);
  assert(w.awayAttempts==1&&w.awayGoals==0);
  assert(!w.observe(2026020307U,true,"SO")); // SO may persist
  assert(!w.observe(2026020308U,true,"OT"));
  assert(w.observe(2026020308U,true,"SO")); // observed OT -> SO
  assert(w.transitionSeen&&!w.playsKnown);
  assert(!w.observe(2026020308U,true,"SO")); // no duplicate start
  using namespace CelebrationEngine;
  assert(eligible(true,SHOOTOUT_FLAG,Event::SHOOTOUT_GOAL));
  assert(!eligible(true,GOAL_FLAG,Event::SHOOTOUT_GOAL));
  assert(duration(Event::SHOOTOUT_GOAL,8)==4500UL);
  assert(duration(Event::SHOOTOUT_SAVE,8)==2300UL);
  assert(effect(Event::SHOOTOUT_GOAL,0,0).fx==28);
  assert(effect(Event::SHOOTOUT_SAVE,0,0).fx==17);
  assert(WledZonePolicy::target(true,1U,0,7,1));
  assert(!WledZonePolicy::target(true,1U,0,7,2));
  assert(EventOverlay::priority(EventOverlay::Kind::SHOOTOUT_GOAL)==4);
  assert(GameEventLog::matchesCategory(GameEventLog::Type::SHOOTOUT_SAVE,"shootouts"));
  assert(!GameEventLog::matchesCategory(GameEventLog::Type::SHOOTOUT_SAVE,"goals"));
  printf("PASS NHL shootout state, event masks, overlays and game log\n");
}
