#include <assert.h>
#include <stdio.h>
#include "DisplayEventPolicy.h"

int main() {
 using namespace DisplayEventPolicy;
 assert(teamIndex("UTA")>=0 && teamIndex("BOS")>=0);
 assert(teamIndex("ZZZ")==-1);
 const uint32_t favorites=(1UL<<teamIndex("UTA"))|(1UL<<teamIndex("BOS"));
 assert(isFavorite(favorites,"UTA"));
 assert(isFavorite(favorites,"BOS"));
 assert(!isFavorite(favorites,"DAL"));
 assert(favoriteMatch(favorites,"DAL","BOS"));
 assert(tier(favorites,"UTA","DAL",false)==2);
 assert(tier(favorites,"DAL","ANA",true)==1);
 assert(tier(favorites,"DAL","ANA",false)==0);
 assert(shouldShow(Mode::ALL_LIVE,false,"DAL","ANA",favorites));
 assert(!shouldShow(Mode::FOLLOWED_ONLY,false,"DAL","ANA",favorites));
 assert(shouldShow(Mode::FOLLOWED_ONLY,false,"UTA","DAL",favorites));
 assert(duration(EventOverlay::Kind::GOAL,0,8,7)==3000);
 assert(duration(EventOverlay::Kind::GOAL,1,8,7)==8000);
 assert(duration(EventOverlay::Kind::GOAL,2,8,7)==5500);
 assert(backgroundAllowed(EventOverlay::Kind::GOAL,0,100,99,true));
 assert(!backgroundAllowed(EventOverlay::Kind::GAME_START,0,10000,5000,true));
 assert(backgroundAllowed(EventOverlay::Kind::GAME_START,0,17000,5000,true));
 assert(backgroundAllowed(EventOverlay::Kind::POWER_PLAY_START,2,100,99,true));
 puts("PASS display all-live scope, favorite ranking, durations and throttling");
}
