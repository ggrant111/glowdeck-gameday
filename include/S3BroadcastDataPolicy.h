#pragma once
#include <stdint.h>
#include "S3BroadcastGameState.h"

// Waveshare 3.5" Arena Broadcast presentation ONLY. Does not modify NHL
// power-play evidence, empty-net detection, rear LED, WLED or event policy.
// NHL side encoding: 1 = home, 2 = away, 3 = BOTH pulled goalies.
namespace S3Broadcast {
constexpr uint32_t kStatusFreshMs=45000UL;

inline bool freshEvidence(bool known,uint32_t lastUpdate,uint32_t now) {
  return known && lastUpdate != 0 &&
      uint32_t(now - lastUpdate) < kStatusFreshMs;
}

struct SpecialTeams {
  uint8_t ppSide;      // Verified via the existing penalty ledger only.
  uint8_t emptyNetSides; // Existing NHL goalie-pulled bit mask.
  uint8_t signature() const {return uint8_t((ppSide&3U)|((emptyNetSides&3U)<<2));}
};
inline SpecialTeams specialTeams(Phase phase,
  bool ppKnown,uint8_t ppSide,uint32_t ppUpdated,
  bool enKnown,uint8_t enSides,uint32_t enUpdated,uint32_t now) {
  SpecialTeams status={0,0};
  // Intermissions, future/final games must never retain a live PP/EN badge.
  if(phase!=Phase::LIVE)return status;
  if((ppSide==1 || ppSide==2) &&
     freshEvidence(ppKnown,ppUpdated,now))
    status.ppSide=ppSide;
  if((enSides&3U) && freshEvidence(enKnown,enUpdated,now))
    status.emptyNetSides=enSides&3U;
  return status;
}
inline bool hasAway(uint8_t sides) {return (sides&2U)!=0;}
inline bool hasHome(uint8_t sides) {return (sides&1U)!=0;}
inline bool needsStatusRefresh(uint32_t renderedGameId,uint8_t renderedMask,
                               uint32_t visibleGameId,uint8_t currentMask) {
  return visibleGameId!=0 && renderedGameId==visibleGameId &&
         renderedMask!=currentMask;
}
} // namespace S3Broadcast
