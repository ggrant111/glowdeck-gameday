#include <assert.h>
#include <stdio.h>
#include "NhlPowerPlayTracker2.h"
using namespace NhlPowerPlayTracker2;

static Sample active(uint32_t game,uint32_t now,uint8_t side=1,
                     uint32_t key=101) {
  Sample s;
  s.gameId=game;s.nowMs=now;s.live=true;
  s.ledgerFresh=true;s.clockKnown=true;
  s.ledgerSide=side;s.labelKnown=true;s.labelSide=side;
  s.penaltyKey=key;s.penaltyAt=200;s.penaltyEndAt=320;
  s.gameAt=205;
  return s;
}
int main(){
  Watch w;
  const uint32_t id=2026020246UL;
  Sample s=active(id,1000);
  Delta d=w.observe(s);
  assert(d.started==1 && !d.ended && w.session==1);
  assert(w.phase==Phase::ACTIVE && w.outputSide(1000)==1);
  assert(w.takePending().started==1);
  assert(w.takePending().started==0);

  // TV timeout / temporary situation field omission: penalty still in NHL
  // summary, so PP remains physically active with NO repeat start.
  s.nowMs=11000;s.labelSide=0;s.labelKnown=false;s.gameAt=210;
  d=w.observe(s);
  assert(!d.started&&!d.ended&&w.side==1);
  assert(w.phase==Phase::HELD && w.outputSide(s.nowMs)==1);
  assert(w.reason==Reason::LABEL_MISSING);
  assert(!w.takePending().started);

  // Official intermission ends the pulse but NOT the underlying PP session.
  s.nowMs=15000;s.intermission=true;
  d=w.observe(s);
  assert(w.phase==Phase::INTERMISSION);
  assert(w.session==1&&w.side==1&&w.outputSide(s.nowMs,true)==0);
  assert(!d.ended);
  w.takePending();
  s.nowMs=55000;s.intermission=false;s.gameAt=215;
  d=w.observe(s);
  assert(w.session==1&&w.outputSide(s.nowMs)==1);
  assert(!d.started && d.resumed);
  assert(w.takePending().resumed && !w.takePending().started);

  // Simulated single false 5v5: never stop output or log PP_END on one
  // empty/no-PP sample. Two independent observations >=10s can end it.
  s.nowMs=66000;s.ledgerSide=0;s.labelSide=0;s.gameAt=250;
  d=w.observe(s);
  assert(!d.ended&&w.phase==Phase::HELD&&w.outputSide(s.nowMs)==1);
  w.takePending();
  s.nowMs=75000;d=w.observe(s);
  assert(!d.ended);w.takePending();
  s.nowMs=77100;d=w.observe(s);
  assert(d.ended==1 && w.side==0);
  assert(w.takePending().ended==1 && !w.takePending().ended);

  // A genuinely new opportunity for the SAME team gets a new session ID.
  s=active(id,80000,1,202);s.penaltyAt=800;s.penaltyEndAt=920;s.gameAt=805;
  d=w.observe(s);
  assert(d.started==1 && w.session==2 && w.anchorKey==202);
  w.takePending();
  s.nowMs=81500;s.gameAt=815;s.ledgerSide=0;
  s.verifiedPpGoalEnd=true;d=w.observe(s);
  assert(d.ended==1 && w.reason==Reason::PP_GOAL);
  assert(w.takePending().ended==1);

  // An active major can survive a PP goal because the ledger still says PP.
  s=active(id,90000,2,303);s.penaltyAt=900;s.penaltyEndAt=1200;
  d=w.observe(s);assert(d.started==2);w.takePending();
  s.nowMs=95000;s.gameAt=950;s.verifiedPpGoalEnd=true;
  d=w.observe(s);
  assert(w.side==2 && !d.ended);w.takePending();

  // Network outage: expire physical output after grace, but never invent a
  // PP_END. On reconnect, the same penalty resumes with no extra start.
  s.nowMs=96000;s.ledgerFresh=false;s.clockKnown=false;
  d=w.observe(s);
  assert(!d.ended && w.outputSide(s.nowMs)==2);w.takePending();
  assert(w.tick(95000UL+kUnverifiedGraceMs+1, false));
  assert(w.phase==Phase::UNKNOWN);
  assert(w.outputSide(95000UL+kUnverifiedGraceMs+1)==0);
  assert(!w.takePending().ended);
  s=active(id,95000UL+kUnverifiedGraceMs+2500UL,2,303);
  d=w.observe(s);
  assert(w.session==3 && w.side==2 && !d.started && d.resumed);
  assert(w.outputSide(s.nowMs)==2);
  w.takePending();

  // Complete minor expiry ends immediately if game clock passed known end.
  s.nowMs+=10000;s.ledgerSide=0;s.labelSide=0;s.gameAt=1200;
  d=w.observe(s);
  assert(d.ended==2 && w.reason==Reason::EXPIRED);
  w.takePending();

  // Goalie pull cannot start a session: no ledger evidence = no PP.
  Watch noLedger;
  s=active(2026020247UL,1000);
  s.ledgerSide=0;s.labelSide=1;
  d=noLedger.observe(s);
  assert(!d.started && !noLedger.side);
  assert(noLedger.outputSide(s.nowMs)==0);

  // Different games never inherit each other's PP, even in a re-ordered
  // scoreboard. Independent sessions carry their original IDs.
  Watch other;Sample x=active(2026020248UL,5000,2);
  assert(other.observe(x).started==2);other.takePending();
  assert(other.gameId!=w.gameId && other.side==2 && w.side==0);
  // Reset on a real NHL game ID change, not on slideshow index.
  x.gameId=2026020249UL;x.nowMs=10000;
  d=other.observe(x);
  assert(d.started==2 && other.session==1 && other.gameId==x.gameId);

  puts("PASS PP2: TV break, intermission/resume, debounce, new same-team PP, goal, major, outage, expiry, multi-game");
}
