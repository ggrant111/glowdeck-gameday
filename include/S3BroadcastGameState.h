#pragma once
#include <stdint.h>
#include <string.h>

// Waveshare S3 Phase 6B.2B broadcast state policy.
// Pure C++11 so NHL lifecycle behavior is covered by host tests without
// pulling in Arduino, display drivers, Wi-Fi, WLED or the CYD renderer.
namespace S3Broadcast {
enum class Phase : uint8_t {
  UPCOMING, LIVE, INTERMISSION, FINAL, OTHER
};
struct View {
  Phase phase;
  bool showScore;
  bool showShots;
  bool showSeasonRecord;
  bool showSpecialTeams;
  // 0 = no winner/unknown, 1 = home winner, 2 = away winner.
  uint8_t winnerSide;
};
inline bool is(const char *s, const char *value) {
  return s && strcmp(s,value) == 0;
}
inline Phase classify(const char *gameState, bool inIntermission) {
  if (is(gameState,"FUT") || is(gameState,"PRE")) return Phase::UPCOMING;
  if (is(gameState,"LIVE") || is(gameState,"CRIT"))
    return inIntermission ? Phase::INTERMISSION : Phase::LIVE;
  if (is(gameState,"OFF") || is(gameState,"FINAL") ||
      is(gameState,"OVER")) return Phase::FINAL;
  return Phase::OTHER;
}
inline View resolve(const char *gameState,bool inIntermission,
                    int awayScore,int homeScore) {
  const Phase p=classify(gameState,inIntermission);
  View result={p,p==Phase::LIVE || p==Phase::INTERMISSION ||
                   p==Phase::FINAL,
                 p==Phase::LIVE || p==Phase::INTERMISSION ||
                   p==Phase::FINAL,
                 p==Phase::UPCOMING,p==Phase::LIVE,0};
  // A winner is meaningful only for a completed game with unequal scores.
  if (p==Phase::FINAL && awayScore>=0 && homeScore>=0) {
    if (homeScore>awayScore) result.winnerSide=1;
    else if (awayScore>homeScore) result.winnerSide=2;
  }
  return result;
}
inline const char *finalStatus(const char *periodType,
                               const char *gameState="OFF") {
  if (is(gameState,"OVER")) return "GAME OVER";
  if (is(periodType,"SO")) return "FINAL / SO";
  if (is(periodType,"OT")) return "FINAL / OT";
  return "FINAL";
}
inline const char *periodName(uint8_t period,const char *periodType) {
  if (is(periodType,"SO")) return "SHOOTOUT";
  if (is(periodType,"OT") || period>3) return "OVERTIME";
  if(period==1) return "1ST";
  if(period==2) return "2ND";
  if(period==3) return "3RD";
  return "PERIOD";
}
} // namespace S3Broadcast
