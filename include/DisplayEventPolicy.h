#pragma once
#include <stdint.h>
#include <string.h>
#include "TeamPalette.h"
#include "EventOverlay.h"

// Pure, allocation-free preferences and admission policy for the CYD display.
// The physical WLED/rear-LED selection is deliberately NOT checked here.
namespace DisplayEventPolicy {
enum class Mode : uint8_t { FOLLOWED_ONLY=0, ALL_LIVE=1 };
inline int8_t teamIndex(const char* abbr) {
  if (!abbr) return -1;
  for (uint8_t i=0; i<TeamPalette::kCount; ++i)
    if (!strcmp(abbr, TeamPalette::kTeams[i].abbr)) return (int8_t)i;
  return -1;
}
inline bool isFavorite(uint32_t mask, const char* abbr) {
  const int8_t index=teamIndex(abbr);
  return index>=0 && (mask & (1UL<<index))!=0;
}
inline bool favoriteMatch(uint32_t mask, const char* away,
                          const char* home) {
  return isFavorite(mask,away)||isFavorite(mask,home);
}
// 2=favorite, 1=armed/followed, 0=other. Favorites outrank even
// ordinary goals; goals outrank power plays within a single tier.
inline uint8_t tier(uint32_t favorites, const char* away, const char* home,
                    bool followed) {
  return favoriteMatch(favorites,away,home)?2:(followed?1:0);
}
inline bool shouldShow(Mode mode, bool followed,
                       const char* away, const char* home,
                       uint32_t favorites) {
  return mode==Mode::ALL_LIVE || followed ||
         favoriteMatch(favorites,away,home);
}
inline uint32_t duration(EventOverlay::Kind kind, uint8_t tier,
                         uint8_t goalSeconds, uint8_t otherSeconds) {
  if (tier==0) {
    if (kind==EventOverlay::Kind::GOAL) return 3000UL;
    if (kind==EventOverlay::Kind::POWER_PLAY_END) return 1600UL;
    return 2400UL;
  }
  if (tier==2) {
    if (kind==EventOverlay::Kind::GOAL) return 5500UL;
    if (kind==EventOverlay::Kind::FINAL_WIN ||
        kind==EventOverlay::Kind::FINAL_LOSS) return 4300UL;
    if (kind==EventOverlay::Kind::POWER_PLAY_START ||
        kind==EventOverlay::Kind::POWER_PLAY_END) return 2800UL;
    return 3000UL;
  }
  return EventOverlay::durationMs(kind, goalSeconds, otherSeconds);
}
// Quiet nonfavorite background PP/start/final chatter during crowded slates.
// All observed goals remain eligible; queue capacity and freshness bound noise.
inline bool backgroundAllowed(EventOverlay::Kind kind, uint8_t tier,
                              uint32_t now, uint32_t lastAt,
                              bool hasPrevious) {
  if (tier>0 || kind==EventOverlay::Kind::GOAL ||
      kind==EventOverlay::Kind::SHOOTOUT_START ||
      kind==EventOverlay::Kind::SHOOTOUT_GOAL ||
      kind==EventOverlay::Kind::SHOOTOUT_SAVE ||
      kind==EventOverlay::Kind::SHOOTOUT_MISS)return true;
  return !hasPrevious || uint32_t(now-lastAt)>=12000UL;
}
} // namespace DisplayEventPolicy
