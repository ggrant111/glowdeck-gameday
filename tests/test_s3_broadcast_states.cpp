#include <cassert>
#include <cstring>
#include <iostream>
#include "S3BroadcastGameState.h"
#include "S3NativeLayout.h"

using namespace S3Broadcast;

int main() {
  // The real NHL score feed uses FUT/PRE -> LIVE/CRIT -> OFF/FINAL,
  // with an intermission flag on LIVE and periodType OT/SO at final.
  const View fut=resolve("FUT",false,0,0);
  const View pre=resolve("PRE",false,0,0);
  assert(fut.phase==Phase::UPCOMING && pre.phase==Phase::UPCOMING);
  assert(!fut.showScore && !fut.showShots && fut.showSeasonRecord);
  assert(!pre.showSpecialTeams && !fut.winnerSide);

  const View live=resolve("LIVE",false,2,1);
  const View critical=resolve("CRIT",false,4,3);
  assert(live.phase==Phase::LIVE && critical.phase==Phase::LIVE);
  assert(live.showScore && live.showShots && live.showSpecialTeams);
  assert(!live.showSeasonRecord && !live.winnerSide);
  assert(!critical.winnerSide); // Never declare a live-game winner.

  const View intermission=resolve("LIVE",true,2,1);
  const View criticalIntermission=resolve("CRIT",true,3,3);
  assert(intermission.phase==Phase::INTERMISSION);
  assert(criticalIntermission.phase==Phase::INTERMISSION);
  assert(intermission.showScore && intermission.showShots);
  assert(!intermission.showSpecialTeams && !intermission.winnerSide);

  const View finalHome=resolve("OFF",false,2,3);
  const View finalAway=resolve("FINAL",false,5,1);
  const View finalTie=resolve("FINAL",false,1,1);
  const View gameOver=resolve("OVER",false,1,2);
  assert(finalHome.phase==Phase::FINAL && finalHome.winnerSide==1);
  assert(finalAway.phase==Phase::FINAL && finalAway.winnerSide==2);
  assert(finalTie.phase==Phase::FINAL && !finalTie.winnerSide);
  assert(gameOver.phase==Phase::FINAL && gameOver.winnerSide==1);
  assert(finalHome.showScore && finalHome.showShots);
  assert(!finalAway.showSpecialTeams && !finalAway.showSeasonRecord);
  assert(!std::strcmp(finalStatus("REG"),"FINAL"));
  assert(!std::strcmp(finalStatus("OT"),"FINAL / OT"));
  assert(!std::strcmp(finalStatus("SO"),"FINAL / SO"));
  assert(!std::strcmp(finalStatus("SO","OVER"),"GAME OVER"));

  const View unknown=resolve("",false,0,0);
  const View disconnected=resolve(nullptr,false,0,0);
  assert(unknown.phase==Phase::OTHER && disconnected.phase==Phase::OTHER);
  assert(!unknown.showScore && !unknown.showShots);
  assert(!unknown.showSpecialTeams && !unknown.showSeasonRecord);
  assert(!std::strcmp(periodName(1,"REG"),"1ST"));
  assert(!std::strcmp(periodName(2,"REG"),"2ND"));
  assert(!std::strcmp(periodName(3,"REG"),"3RD"));
  assert(!std::strcmp(periodName(4,"OT"),"OVERTIME"));
  assert(!std::strcmp(periodName(5,"SO"),"SHOOTOUT"));
  assert(!std::strcmp(periodName(0,""),"PERIOD"));

  // Dirty readouts contain every state-changing UI pixel and exclude
  // logo plinths, footer navigation and follow-star controls.
  const auto top=S3NativeLayout::kBroadcastReadoutTop;
  const auto bottom=S3NativeLayout::kBroadcastReadoutBottom;
  assert(top.x==144 && top.y==43 && top.w==192 && top.h==160);
  assert(bottom.x==48 && bottom.y==215 &&
         bottom.w==384 && bottom.h==53);
  assert(top.x>134 && top.x+top.w<346); // 96px official logos
  assert(bottom.y+bottom.h<=S3NativeLayout::kBroadcastGames.y);
  assert(bottom.y+bottom.h<=S3NativeLayout::kBroadcastSettings.y);
  assert(top.y+top.h < bottom.y);

  std::cout<<"S3 broadcast: FUT/PRE, LIVE/CRIT, INT, FINAL/OT/SO, "
              "winner, unknown status and dirty bounds OK\n";
}
