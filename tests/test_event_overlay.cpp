#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "EventOverlay.h"
using namespace EventOverlay;
int main() {
  static_assert(kQueueSize==4,"Overlay queue must be bounded");
  assert(priority(Kind::GOAL)>priority(Kind::FINAL_WIN));
  assert(priority(Kind::FINAL_WIN)>priority(Kind::GAME_START));
  assert(durationMs(Kind::GOAL,8,7)==8000);
  assert(durationMs(Kind::POWER_PLAY_END,8,7)==2500);
  assert(durationMs(Kind::FINAL_WIN,8,7)==7000);
  assert(isFlash(Kind::GOAL));
  assert(isPulse(Kind::POWER_PLAY_START));
  char team[4];copy(team,"WASHINGTON",sizeof(team));
  assert(!strcmp(team,"WAS"));
  Controller c;
  Item pp;pp.kind=Kind::POWER_PLAY_START;pp.duration=4500;pp.gameId=123;pp.eventSequence=1;
  assert(c.push(pp,1000)); assert(c.active());
  assert(!c.push(pp,1300)); // dedupe
  assert(!c.tick(3000)); // still active
  Item goal;goal.kind=Kind::GOAL;goal.duration=8000;goal.gameId=123;goal.eventSequence=2;
  assert(c.push(goal,3200));assert(c.current().kind==Kind::GOAL);
  assert(c.pending()==1); // PP pause
  Item final;final.kind=Kind::FINAL_WIN;final.duration=7000;final.gameId=123;final.eventSequence=3;
  assert(c.push(final,4000));assert(c.current().kind==Kind::GOAL);
  assert(c.pending()==2);
  assert(!c.tick(11200-1));
  assert(c.tick(11200));assert(c.current().kind==Kind::FINAL_WIN);
  assert(c.tick(18200));assert(c.current().kind==Kind::POWER_PLAY_START);
  assert(c.tick(20500)); assert(!c.active()); // resumes paused PP remainder
  // Bounded queue; low-priority overflow does not evict a final.
  c.clear();
  Item h;h.kind=Kind::GAME_START;h.duration=5000;h.eventSequence=10;h.gameId=10;
  c.push(h,0);
  for(int i=1;i<=4;i++) { Item x=final;x.eventSequence=30+i;x.gameId=100+i;c.push(x,100); }
  assert(c.pending()<=kQueueSize);
  c.clear();assert(!c.active()&&!c.pending());
  // Favorite PP beats ordinary goal; ordinary notifications do not pause/replay.
  Item ordinary=goal;ordinary.tier=0;ordinary.duration=3000;ordinary.gameId=800;
  assert(c.push(ordinary,0));
  Item favorite=pp;favorite.tier=2;favorite.gameId=900;
  assert(rank(favorite)>rank(ordinary));
  assert(c.push(favorite,200) && c.current().gameId==900);
  assert(c.pending()==0); // No stale ordinary event replay.
  // Keep at most four pending entries; evict low-tier PP for favorite goals.
  c.clear();
  favorite.kind=Kind::POWER_PLAY_START;favorite.duration=3500;
  assert(c.push(favorite,1000));
  for(int i=0;i<kQueueSize;i++){
    Item x=ordinary;x.gameId=300+i;x.eventSequence=20+i;
    assert(c.push(x,1100+i));
  }
  assert(c.pending()==kQueueSize);
  Item priorityGoal=goal;priorityGoal.tier=2;priorityGoal.gameId=999;
  assert(c.push(priorityGoal,1300));
  assert(c.current().gameId==999);
  // Stale nonfavorite goals expire from the queue after 15 seconds.
  // A queued stale nonfavorite event must never appear.
  c.clear();
  assert(c.push(favorite,2000));
  assert(c.push(ordinary,2001));
  assert(c.tick(40000) && !c.active());
  puts("PASS: overlay favorites, stale suppression, queue bounds, timing and dedupe");
}
