#pragma once
#include <stdint.h>
#include <string.h>

// One visible game's compact NHL landing-summary snapshot. No heap ownership,
// display dependencies or HTTP calls. The existing PP evidence fetch fills it.
// Game IDs keep scorer/penalty details from bleeding across carousel pages.
namespace NhlLiveTicker {
constexpr uint8_t kMaxGoals = 16;
constexpr uint8_t kMaxPenalties = 20;
constexpr uint32_t kStepMs = 180;
constexpr uint8_t kSeparatorChars = 5;

inline void copy(char *out, size_t cap, const char *source) {
  if (!cap) return;
  if (!source) source = "";
  strncpy(out, source, cap - 1);
  out[cap - 1] = '\0';
}
inline bool isPowerPlayGoal(const char *strength) {
  return strength && (!strcmp(strength,"pp") ||
    !strcmp(strength,"PP") || !strcmp(strength,"PPG") ||
    !strcmp(strength,"ppg"));
}
inline bool boxType(const char *type,uint8_t minutes) {
  if (!type || minutes == 0 || minutes > 10) return false;
  return !strcmp(type,"MIN") || !strcmp(type,"BEN") ||
         !strcmp(type,"DBL") || !strcmp(type,"MAJ");
}
inline uint16_t absolute(uint8_t period,uint16_t elapsed) {
  return uint16_t(uint16_t(period - 1U) * 1200U + elapsed);
}
// NHL clock is remaining time. Regular-season period 4 is a five-minute OT;
// playoff OT uses 20 minutes. The feed's goal/penalty event times are elapsed.
inline uint16_t elapsedAt(uint8_t period,uint16_t remaining,
                          bool regularOvertime) {
  const uint16_t length=(period == 4 && regularOvertime) ? 300 : 1200;
  return absolute(period,remaining <= length ? uint16_t(length-remaining) : 0);
}
inline bool inBox(uint16_t current,uint16_t start,uint16_t end) {
  return current >= start && current < end;
}
inline uint16_t scrollNext(uint16_t current,uint16_t length) {
  return length ? uint16_t((uint32_t(current) + 1U) % length) : 0U;
}

struct Goal {
  char player[36]={};
  char team[5]={};
  char time[8]={};
  uint8_t period=0, side=0;
  uint16_t at=0;
  bool powerPlay=false;
};
struct Penalty {
  char player[36]={};
  char team[5]={};
  char reason[26]={};
  uint16_t at=0, end=0;
  uint8_t minutes=0, side=0;
  bool releasable=false;
};
struct Feed {
  uint32_t gameId=0, fetchedAt=0;
  uint8_t goalCount=0, penaltyCount=0;
  Goal goals[kMaxGoals]={};
  Penalty penalties[kMaxPenalties]={};

  void reset(uint32_t id=0) {
    // Reset only populated rows rather than zeroing a large array in loop().
    for(uint8_t i=0;i<goalCount;++i)goals[i]=Goal();
    for(uint8_t i=0;i<penaltyCount;++i)penalties[i]=Penalty();
    goalCount=0; penaltyCount=0; gameId=id; fetchedAt=0;
  }
  bool addPenalty(uint8_t period,uint16_t elapsed,uint8_t side,
                  const char *team,const char *player,const char *reason,
                  const char *type,uint8_t minutes) {
    if(period<1 || period>5 || elapsed>1200 || !side ||
       penaltyCount>=kMaxPenalties || !boxType(type,minutes)) return false;
    Penalty &p=penalties[penaltyCount++];
    p.at=absolute(period,elapsed);
    p.end=uint16_t(p.at+uint16_t(minutes)*60U);
    p.minutes=minutes; p.side=side;
    p.releasable=strcmp(type,"MAJ")!=0;
    copy(p.team,sizeof(p.team),team);
    copy(p.player,sizeof(p.player),player);
    copy(p.reason,sizeof(p.reason),reason);
    return true;
  }
  bool addGoal(uint8_t period,uint16_t elapsed,uint8_t side,
               const char *team,const char *player,const char *time,
               const char *strength) {
    if(period<1 || period>5 || elapsed>1200 || !side ||
       goalCount>=kMaxGoals) return false;
    Goal &g=goals[goalCount++];
    g.period=period;g.at=absolute(period,elapsed);g.side=side;
    g.powerPlay=isPowerPlayGoal(strength);
    copy(g.team,sizeof(g.team),team);
    copy(g.player,sizeof(g.player),player);
    copy(g.time,sizeof(g.time),time);
    return true;
  }
  // Cancel only the oldest eligible opposing MIN/BEN/DBL for each PP goal;
  // offsetting majors and misconducts do not get marked as released.
  // Invoked after ALL penalties/goals have been collected from landing.
  void applyPowerPlayGoals() {
    // Idempotent if a live snapshot is re-evaluated as the scoreboard clock
    // advances. Never release a second penalty on the same PP goal.
    for(uint8_t i=0;i<penaltyCount;++i)
      penalties[i].end=uint16_t(penalties[i].at+
                                 uint16_t(penalties[i].minutes)*60U);
    for(uint8_t g=0;g<goalCount;++g) {
      const Goal &goal=goals[g];
      if(!goal.powerPlay)continue;
      uint8_t selected=kMaxPenalties;
      for(uint8_t i=0;i<penaltyCount;++i) {
        const Penalty &p=penalties[i];
        if(!p.releasable || p.side==goal.side ||
           p.at>goal.at || goal.at>=p.end)continue;
        if(selected==kMaxPenalties || p.at<penalties[selected].at)
          selected=i;
      }
      if(selected==kMaxPenalties)continue;
      Penalty &p=penalties[selected];
      if(p.minutes==4 && p.end>goal.at+120)
        p.end=uint16_t(goal.at+120);
      else
        p.end=goal.at;
    }
  }
};
} // namespace NhlLiveTicker
