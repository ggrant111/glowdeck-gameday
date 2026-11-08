#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "WledRestoreFetchPolicy.h"
#include "NhlPowerPlayTracker2.h"
#include "GameEventLog.h"

using namespace WledRestoreFetchPolicy;
int main() {
  const uint32_t now=1234567UL;
  const uint32_t budget=12500UL;
  // Root-cause regression: active PP holds a WLED lease indefinitely,
  // without scheduling a restore. This must NOT block the NHL scoreboard,
  // official penalty ledger or game stats for the duration of that PP.
  assert(!dueSoon(true,UINT32_MAX,now,budget));
  assert(!dueSoon(true,UINT32_MAX,now,0));
  assert(!dueSoon(false,now,now,budget));
  assert(!dueSoon(false,UINT32_MAX,now,budget));
  assert(!dueSoon(true,now+budget+1,now,budget));
  // An actual pending restore retains priority over new HTTPS requests.
  assert(dueSoon(true,now+budget,now,budget));
  assert(dueSoon(true,now+200,now,budget));
  assert(dueSoon(true,now,now,budget));
  assert(dueSoon(true,now-1000,now,budget));
  // millis() wrap around a finite restore timer is handled correctly.
  const uint32_t wrap=UINT32_MAX-1000;
  assert(dueSoon(true,999UL,wrap,2500UL));
  assert(!dueSoon(true,2999UL,wrap,2500UL));
  assert(!dueSoon(true,UINT32_MAX,wrap,2500UL));

  // PP2 end transition remains available to the Activity event log
  // even while the WLED device is actively pulsing (not restoring).
  NhlPowerPlayTracker2::Watch watch;
  NhlPowerPlayTracker2::Sample sample;
  sample.gameId=2026020246UL;
  sample.nowMs=5000UL;
  sample.live=true;
  sample.clockKnown=true;
  sample.ledgerFresh=true;
  sample.gameAt=150;
  sample.penaltyAt=120;
  sample.penaltyEndAt=240;
  sample.penaltyKey=111;
  sample.labelSide=1;
  sample.ledgerSide=1;
  assert(watch.observe(sample).started==1);
  assert(watch.takePending().started==1);
  assert(!dueSoon(true,UINT32_MAX,sample.nowMs,budget));
  sample.nowMs=18000UL;sample.gameAt=245;
  sample.labelSide=0;sample.ledgerSide=0;
  const NhlPowerPlayTracker2::Delta change=watch.observe(sample);
  assert(change.ended==1);
  assert(watch.takePending().ended==1);
  GameEventLog::Buffer history;
  GameEventLog::Entry &event=history.add(
      GameEventLog::Type::POWER_PLAY_END,0,18,sample.gameId,
      "BOS","UTA","UTA",1,2,1);
  assert(event.type==GameEventLog::Type::POWER_PLAY_END);
  assert(GameEventLog::matchesCategory(event.type,"all"));
  assert(GameEventLog::matchesCategory(event.type,"powerplays"));
  assert(history.size()==1);
  puts("PASS: WLED PP lease never blocks NHL polls; real restore priority; verified PP_END is loggable");
}
