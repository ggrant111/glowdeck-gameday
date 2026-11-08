#include <assert.h>
#include <stdio.h>
#include "NhlPenaltyLedger.h"
using namespace NhlPenaltyLedger;
int main() {
  Book b;
  // Official ANA @ VAN, 2026-11-03, third period:
  // ANA Warren MIN at 04:37; fighting MAJs/misconducts at same time
  // do not contribute to manpower. VAN DeBrusk PP goal at 05:30;
  // VAN DeBrusk EV goal at 09:34. Score finishes ANA5 VAN4.
  assert(b.addPenalty(3,"04:37",2,"MIN",2,"roughing"));
  // Home is Vancouver (1), ANA penalized away (2).
  assert(b.addPenalty(3,"04:37",2,"MAJ",5,"fighting")==false);
  assert(b.addPenalty(3,"04:37",2,"GAM",10,"game-misconduct")==false);
  uint16_t t=0;
  assert(Book::absolute(3,"04:50",t));
  assert(!b.sideAt(t)); // unavailable ledger never creates alerts.
  b.loaded=true;b.gameId=2026020246;
  assert(b.sideAt(t)==1);
  assert(Book::absolute(3,"05:29",t)&&b.sideAt(t)==1);
  assert(b.addPpGoal(3,"05:30",1));
  assert(Book::absolute(3,"05:30",t)&&b.sideAt(t)==0);
  assert(Book::absolute(3,"09:34",t)&&b.sideAt(t)==0);
  assert(Book::absolute(3,"18:00",t)&&b.sideAt(t)==0);
  uint16_t x;
  assert(Book::fromRemaining(3,"10:26",x));
  assert(Book::absolute(3,"09:34",t)&&x==t);
  // Multiple PP opportunities, two at the same time offset 4-on-4.
  Book pair;pair.reset(77);
  assert(pair.addPenalty(1,"04:00",1,"MIN",2,"tripping"));
  assert(pair.addPenalty(1,"04:00",2,"MIN",2,"roughing"));
  pair.loaded=true;
  assert(Book::absolute(1,"04:30",t)&&pair.sideAt(t)==0);
  assert(Book::absolute(1,"06:00",t)&&pair.sideAt(t)==0);
  // A short-handed team goal doesn't end the other team's PP.
  Book sh;sh.addPenalty(2,"01:56",2,"MIN",2,"holding");
  sh.addPpGoal(2,"02:20",2);sh.loaded=true;
  assert(Book::absolute(2,"02:50",t)&&sh.sideAt(t)==1);
  assert(Book::absolute(2,"03:56",t)&&sh.sideAt(t)==0);
  // Stable penalty identity across NHL polls, verified PPG cancellation.
  Book stable;
  assert(stable.addPenalty(1,"01:23",2,"MIN",2,"interference"));
  stable.loaded=true;
  assert(Book::absolute(1,"01:30",t));
  const Book::Evidence first=stable.evidenceAt(t);
  const Book::Evidence second=stable.evidenceAt(t);
  assert(first.side==1 && first.key!=0 && first.key==second.key);
  assert(first.penaltyAt==83 && first.endAt==203);
  assert(stable.addPpGoal(1,"01:59",1));
  assert(Book::absolute(1,"02:00",t) && stable.sideAt(t)==0);
  assert(stable.hasPpGoalSince(1,83,t));
  assert(!stable.hasPpGoalSince(2,83,t));
  // Five-minute regular-season OT vs 20-minute playoff OT.
  assert(Book::fromRemaining(4,"04:30",t,true)&&t==3630);
  assert(!Book::fromRemaining(4,"19:30",t,true));
  assert(Book::fromRemaining(4,"19:30",t,false)&&t==3630);
  // Penalties crossing regulation/intermission remain active in OT.
  Book carry;
  assert(carry.addPenalty(3,"19:30",2,"MIN",2,"tripping"));
  carry.loaded=true;
  assert(Book::fromRemaining(4,"04:50",t,true));
  assert(carry.evidenceAt(t).side==1);
  // Remaining clock form, invalid unknown clock, no overflow.
  assert(!Book::fromRemaining(3,"",t));
  assert(!Book::absolute(0,"04:00",t));
  puts("PASS official ANA/VAN PP 04:37..05:30, PP goal ends minor, no 5-4 PP");
}
