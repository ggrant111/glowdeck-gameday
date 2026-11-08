#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "NhlPowerPlayPresentation.h"
#include "NhlPowerPlayTracker2.h"
#include "GameEventLog.h"

using namespace NhlPowerPlayPresentation;
static const char *club(uint8_t side,const char *home,const char *away) {
  return side==1?home:side==2?away:"";
}
static void verify(uint8_t ppSide, const char *home,const char *away,
                   const char *expectedPp,const char *expectedRestored) {
  assert(!strcmp(club(ppSide,home,away),expectedPp));
  const uint8_t restoredSide=shortHandedSide(ppSide);
  assert(restoredSide && restoredSide!=ppSide);
  const char *restored=club(restoredSide,home,away);
  assert(!strcmp(restored,expectedRestored));

  // PP start and PP end are opposite teams in the SAME game. Activity
  // must attribute each event to the team shown on the corresponding
  // overlay and the PP_END event must appear under the powerplays filter.
  GameEventLog::Buffer history;
  GameEventLog::Entry &start=history.add(
      GameEventLog::Type::POWER_PLAY_START,0,11,2026020246UL,
      away,home,expectedPp,0,0,2);
  assert(!strcmp(start.team,expectedPp));
  GameEventLog::Entry &ended=history.add(
      GameEventLog::Type::POWER_PLAY_END,0,32,2026020246UL,
      away,home,restored,0,0,2);
  assert(!strcmp(ended.team,expectedRestored));
  assert(GameEventLog::matchesCategory(ended.type,"powerplays"));
  assert(history.size()==2);
}
int main() {
  // HOME PP: the AWAY club was a man down and returns to full strength.
  verify(1,"UTA","BOS","UTA","BOS");
  // AWAY PP: the HOME club was shorthanded and returns to full strength.
  verify(2,"UTA","BOS","BOS","UTA");
  assert(shortHandedSide(0)==0);
  assert(shortHandedSide(3)==0);
  // Real PP2 change.ended is the previous advantage side, not the
  // short-handed side. Confirm the tracker contract in both directions.
  for(uint8_t side=1;side<=2;++side) {
    NhlPowerPlayTracker2::Watch tracker;
    NhlPowerPlayTracker2::Sample in;
    in.gameId=2026020246UL;in.nowMs=1000UL;in.live=true;
    in.ledgerFresh=true;in.clockKnown=true;in.gameAt=100;
    in.ledgerSide=side;in.labelSide=side;
    in.penaltyAt=90;in.penaltyEndAt=210;in.penaltyKey=1;
    assert(tracker.observe(in).started==side);
    assert(tracker.takePending().started==side);
    in.nowMs=15000UL;in.gameAt=215;
    in.ledgerSide=0;in.labelSide=0;
    const NhlPowerPlayTracker2::Delta change=tracker.observe(in);
    assert(change.ended==side);
    assert(shortHandedSide(change.ended)==(side==1?2:1));
    assert(tracker.takePending().ended==side);
  }
  puts("PASS PP_END names the previously short-handed club; PP_START stays with advantage team");
}
