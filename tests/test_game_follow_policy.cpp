#include <assert.h>
#include <stdio.h>
#include "GameFollowPolicy.h"

int main() {
  using namespace GameFollowPolicy;
  static_assert(kMaxDailyGames == 20, "Follow capacity must cover entire day");
  uint32_t followed[kMaxDailyGames] = {};
  uint8_t slot = 42;
  assert(change(followed,kMaxDailyGames,0,false,0,true,slot)==Result::INVALID);
  assert(change(followed,kMaxDailyGames,999,false,0,true,slot)==Result::INVALID);
  assert(change(followed,kMaxDailyGames,50,true,50,true,slot)==Result::ARMED);
  assert(slot==255);
  for(uint32_t i=0; i<kMaxDailyGames; ++i) {
    const uint32_t id=100+i;
    assert(change(followed,kMaxDailyGames,id,true,999,true,slot)==Result::ADDED);
    assert(slot==i && followed[slot]==id);
  }
  assert(change(followed,kMaxDailyGames,105,true,999,true,slot)==Result::UNCHANGED);
  assert(slot==255);
  assert(change(followed,kMaxDailyGames,500,true,999,true,slot)==Result::FULL);
  assert(slot==255);
  assert(change(followed,kMaxDailyGames,105,true,999,false,slot)==Result::REMOVED);
  assert(slot==5 && followed[5]==0);
  assert(change(followed,kMaxDailyGames,500,true,999,true,slot)==Result::ADDED);
  assert(slot==5 && followed[5]==500);
  assert(change(followed,kMaxDailyGames,500,true,999,false,slot)==Result::REMOVED);
  assert(change(followed,kMaxDailyGames,500,true,999,false,slot)==Result::UNCHANGED);
  assert(headerStarTap(148,14,151,18));
  assert(headerStarTap(122,31,123,31));
  assert(!headerStarTap(148,14,245,14));
  assert(!headerStarTap(148,14,148,89));
  assert(!headerStarTap(310,15,310,15)); // Menu is not the star.
  assert(listStarTap(280) && !listStarTap(255));
  puts("PASS full-day 20-game follows, dedupe, NVS slot and CYD star hit targets");
}
