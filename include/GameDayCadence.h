#pragma once
#include <stdint.h>

namespace GameDayCadence {
  constexpr uint32_t kPriorityLiveMs = 10000UL;
  constexpr uint32_t kOtherLiveMs = 20000UL;
  constexpr uint32_t kArmedPregameMs = 30000UL;
  constexpr uint32_t kIdleMs = 60000UL;

  // One date-scoped NHL scoreboard request updates all today's matchups.
  // A live followed or WLED-armed game gets top priority; otherwise reduce
  // API/network load during quiet periods.
  inline uint32_t intervalMs(bool followedOrArmedLive, bool anyLive,
                             bool armedPregame) {
    return followedOrArmedLive ? kPriorityLiveMs :
           anyLive ? kOtherLiveMs :
           armedPregame ? kArmedPregameMs : kIdleMs;
  }
}
