#include <cassert>
#include <cstring>
#include "NhlLiveTicker.h"
int main() {
  using namespace NhlLiveTicker;
  assert(!validPlayerName(nullptr));
  assert(!validPlayerName(""));
  assert(!validPlayerName("null"));
  assert(!validPlayerName("NULL"));
  assert(!validPlayerName("  null"));
  assert(!validPlayerName("n/a"));
  assert(validPlayerName("Sam Bennett"));
  assert(validPlayerName("Nico Hischier"));
  assert(kStepMs<=40); // smoother than the old 180ms character stepping
  assert(isPowerPlayGoal("pp"));
  assert(isPowerPlayGoal("PPG"));
  assert(!isPowerPlayGoal("EV"));
  assert(boxType("MIN",2) && boxType("DBL",4) && boxType("MAJ",5));
  assert(!boxType("MIS",10) && !boxType("MIN",0));
  assert(absolute(2,65)==1265);
  assert(elapsedAt(3,1130,false)==2470);
  assert(elapsedAt(4,290,true)==3610); // 5-min regular-season OT
  assert(elapsedAt(4,1190,false)==3610); // playoff 20-min OT
  assert(!inBox(90,100,220));
  assert(inBox(100,100,220) && inBox(219,100,220));
  assert(!inBox(220,100,220));
  assert(scrollNext(0,0)==0);
  assert(scrollNext(2,3)==0);
  Feed f;
  f.reset(2026020123UL);
  assert(f.gameId==2026020123UL);
  assert(f.addPenalty(1,100,1,"BOS","First Player","tripping","MIN",2));
  assert(f.addPenalty(1,130,1,"BOS","Second Player","holding","MIN",2));
  assert(f.addPenalty(1,135,2,"NYR","Fighter","fighting","MAJ",5));
  assert(!f.addPenalty(1,140,1,"BOS","Unknown","misconduct","MIS",10));
  assert(f.penaltyCount==3);
  assert(f.addGoal(1,175,2,"NYR","Goal Scorer","02:55","pp"));
  f.applyPowerPlayGoals();
  assert(f.penalties[0].end==175); // oldest minor ends on PP goal
  assert(f.penalties[1].end==250);
  assert(f.penalties[2].end==435); // fighting major not cut short
  assert(!inBox(180,f.penalties[0].at,f.penalties[0].end));
  assert(inBox(180,f.penalties[1].at,f.penalties[1].end));
  assert(f.addGoal(1,200,2,"NYR","Another Player","03:20","EV"));
  f.applyPowerPlayGoals();
  assert(f.penalties[1].end==250); // even-strength goal cannot release
  // A missing/null offender must not leak the literal "null" into a
  // rendered player name, nor discard the penalty type and duration.
  Feed missingName;
  missingName.reset(2026020125UL);
  assert(missingName.addPenalty(1,100,1,"BOS","null","interference","MIN",2));
  assert(missingName.penaltyCount==1);
  assert(!missingName.penalties[0].player[0]);
  assert(missingName.penalties[0].minutes==2);
  assert(!strcmp(missingName.penalties[0].reason,"interference"));
  assert(!strcmp(f.goals[0].player,"Goal Scorer"));
  assert(!strcmp(f.goals[0].time,"02:55"));
  Feed doubleMinor;
  doubleMinor.reset(5);
  assert(doubleMinor.addPenalty(2,0,1,"BOS","Player","high-sticking","DBL",4));
  assert(doubleMinor.addGoal(2,70,2,"NYR","Scorer","01:10","PPG"));
  doubleMinor.applyPowerPlayGoals();
  assert(doubleMinor.penalties[0].end==absolute(2,190));
  // Names are safely truncated; old game details never survive reset.
  char oversized[100];memset(oversized,'X',sizeof(oversized));oversized[99]=0;
  doubleMinor.reset(6);
  assert(doubleMinor.goalCount==0 && doubleMinor.penaltyCount==0);
  assert(doubleMinor.addGoal(1,10,1,"BOS",oversized,"00:10","EV"));
  assert(strlen(doubleMinor.goals[0].player)==35);
  return 0;
}
