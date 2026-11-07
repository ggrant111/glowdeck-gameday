#include <cassert>
#include <iostream>
#include "S3BroadcastDataPolicy.h"
using namespace S3Broadcast;

int main(){
  const uint32_t now=110000U;
  const auto live=Phase::LIVE;
  // An unverified strength advantage cannot be displayed as a power play.
  auto status=specialTeams(live,false,1,now-1000,true,0,now-1000,now);
  assert(!status.ppSide && !status.emptyNetSides && !status.signature());
  status=specialTeams(live,true,2,now-1000,false,2,now-1000,now);
  assert(status.ppSide==2 && !status.emptyNetSides);
  assert(hasAway(status.ppSide) && !hasHome(status.ppSide));
  // Home and away are intentionally different side encodings.
  status=specialTeams(live,true,1,now-1000,true,2,now-1000,now);
  assert(status.ppSide==1 && status.emptyNetSides==2);
  assert(hasHome(status.ppSide) && !hasAway(status.ppSide));
  assert(hasAway(status.emptyNetSides) && !hasHome(status.emptyNetSides));
  assert(status.signature()==(1U|(2U<<2)));
  // Both goalies pulled is a bitmask, independent of verified PP side.
  status=specialTeams(live,true,2,now-1000,true,3,now-1000,now);
  assert(status.emptyNetSides==3);
  assert(hasHome(status.emptyNetSides) && hasAway(status.emptyNetSides));
  // Do not infer PP just from pulled goalies, or vice versa.
  status=specialTeams(live,true,0,now-1000,true,1,now-1000,now);
  assert(!status.ppSide && status.emptyNetSides==1);
  // Known-but-expired records never leave a stale on-screen celebration.
  status=specialTeams(live,true,1,now-45000,true,2,now-44999,now);
  assert(!status.ppSide && status.emptyNetSides==2);
  status=specialTeams(live,true,1,now-45001,true,2,now-45000,now);
  assert(!status.ppSide && !status.emptyNetSides);
  status=specialTeams(live,true,1,0,true,2,0,now);
  assert(!status.signature());
  status=specialTeams(live,true,1,now-1000,true,2,now-1000,now);
  for(const auto phase:{Phase::UPCOMING,Phase::INTERMISSION,
                        Phase::FINAL,Phase::OTHER}){
    auto state=specialTeams(phase,true,1,now-1000,true,2,now-1000,now);
    assert(state.signature()==0);
  }
  // uint32_t millis rollover still expires correctly.
  status=specialTeams(live,true,1,UINT32_MAX-10000U,
                      true,2,UINT32_MAX-10000U,5000U);
  assert(status.ppSide==1 && status.emptyNetSides==2);
  status=specialTeams(live,true,1,UINT32_MAX-50000U,
                      true,2,UINT32_MAX-50000U,5000U);
  assert(status.signature()==0);

  assert(needsStatusRefresh(123,1,123,0));
  assert(needsStatusRefresh(123,0,123,1));
  assert(!needsStatusRefresh(123,1,123,1));
  assert(!needsStatusRefresh(123,1,456,0));
  assert(!needsStatusRefresh(0,1,0,0));
  std::cout<<"PASS S3 Phase 6B.3: verified PP, pulled-goalie, both teams, "
              "TTL/rollover, state cleanup and redraw changes\n";
}
