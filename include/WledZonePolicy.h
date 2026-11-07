#pragma once
#include <stdint.h>

// WLED Phase 8B. Each of 16 existing WLED segment IDs has a two-bit role.
// ZERO = unassigned, 1 = home, 2 = away, 3 = neutral.
// Mapping is opt-in PER controller. Legacy disabled-mapping controllers
// continue receiving the original full-controller effects unchanged.
namespace WledZonePolicy {
constexpr uint8_t kMaxSegments=16;
enum Role:uint8_t { UNASSIGNED=0, HOME=1, AWAY=2, NEUTRAL=3 };
constexpr uint8_t kUnknownSide=0,kHomeSide=1,kAwaySide=2;
inline bool validRole(int role){return role>=0&&role<=3;}
inline bool validSegment(int id){return id>=0&&id<kMaxSegments;}
inline Role role(uint32_t packed,uint8_t id){
  if(!validSegment(id))return UNASSIGNED;
  return Role((packed>>(uint32_t(id)*2U))&3UL);
}
inline uint32_t assign(uint32_t packed,uint8_t id,Role zone){
  if(!validSegment(id)||!validRole(zone))return packed;
  const uint32_t shift=uint32_t(id)*2UL;
  return (packed&~(3UL<<shift))|(uint32_t(zone)<<shift);
}
inline bool target(bool mappingEnabled,uint32_t packed,uint8_t id,
                   uint8_t eventType,uint8_t scorerSide){
  if(!validSegment(id))return false;
  if(!mappingEnabled)return true; // existing four devices unchanged
  const Role z=role(packed,id);
  if(z==UNASSIGNED)return false;
  // Type 1 goal, 2 power play, 3 start, 4 final_win, 5 final_loss.
  if(eventType==1 || eventType==2){
    if(scorerSide==kHomeSide)return z==HOME||z==NEUTRAL;
    if(scorerSide==kAwaySide)return z==AWAY||z==NEUTRAL;
    return false; // unknown side must never touch a mapped zone
  }
  return z==HOME||z==AWAY||z==NEUTRAL;
}
inline uint32_t zoneColor(Role zone,uint8_t eventType,
                          uint32_t triggeringColor,uint32_t homeRgb,
                          uint32_t awayRgb){
  // A Neutral zone participates for either team. By default it must use
  // the triggering team's NHL color, not a forced white/gray accent.
  // WledScenePolicy::kWhite remains an explicit per-scene override.
  if(zone==NEUTRAL)return triggeringColor;
  if(eventType>=3 && eventType<=5){
    if(zone==HOME)return homeRgb;
    if(zone==AWAY)return awayRgb;
  }
  return triggeringColor;
}
} // namespace WledZonePolicy
