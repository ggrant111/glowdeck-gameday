#pragma once
#include <stdint.h>
#include <string.h>

// Compact official NHL gamecenter/landing summary evidence. An NHL "PP"
// situation label only becomes an actual event while an eligible penalty is
// still active by game clock. Never mistake a goalie pull (e.g. 1560) for PP.
// This is deliberately conservative: no verified penalty => no PP alert.
namespace NhlPenaltyLedger {
constexpr uint8_t kMaxPenalties=40;
constexpr uint8_t kMaxGoals=24;
struct Penalty {
  uint16_t at=0; // absolute seconds elapsed, period1=0..1199
  uint16_t duration=0; // NHL penalty minutes * 60
  uint8_t penalized=0; // 1=home, 2=away
  bool goalReleasable=false; // MIN/BEN or double minor
};
struct Goal {
  uint16_t at=0;
  uint8_t scoringSide=0; // 1=home 2=away
};
struct Book {
  Penalty penalties[kMaxPenalties]={};
  Goal goals[kMaxGoals]={};
  uint8_t penaltyCount=0, goalCount=0;
  bool loaded=false;
  uint32_t gameId=0, checkedAt=0;
  void reset(uint32_t id=0) {
    penaltyCount=0;goalCount=0;loaded=false;gameId=id;checkedAt=0;
  }
  static bool parseTime(const char *s,uint16_t &seconds) {
    if(!s || s[0]<'0' || s[0]>'9')return false;
    unsigned mins=0,secs=0;
    const char *p=s;
    while(*p>='0'&&*p<='9'){mins=mins*10+(*p++-'0');if(mins>25)return false;}
    if(*p++!=':')return false;
    if(p[0]<'0'||p[0]>'9'||p[1]<'0'||p[1]>'9'||p[2])return false;
    secs=(p[0]-'0')*10+(p[1]-'0');
    if(secs>59)return false;
    seconds=(uint16_t)(mins*60+secs);
    return true;
  }
  static bool absolute(uint8_t period,const char*elapsed,uint16_t&at) {
    if(!period || period>5)return false;
    uint16_t sec=0;
    if(!parseTime(elapsed,sec))return false;
    // Periods 1–3 are 20m; overtime begins at elapsed 60m.
    at=(uint16_t)((period-1)*1200+sec);
    return true;
  }
  static bool fromRemaining(uint8_t period, const char *remaining,
                            uint16_t &at) {
    uint16_t sec=0;
    if(!parseTime(remaining,sec)||sec>1200||!period)return false;
    at=(uint16_t)((period-1)*1200+1200-sec);
    return true;
  }
  bool addPenalty(uint8_t period,const char*time,uint8_t penalizedSide,
                  const char*type,uint8_t minutes,const char *description) {
    if(!penalizedSide || penaltyCount>=kMaxPenalties ||
       !type || !minutes) return false;
    const bool minor=!strcmp(type,"MIN") || !strcmp(type,"BEN") ||
                    !strcmp(type,"DBL");
    const bool otherMajor=!strcmp(type,"MAJ") &&
        (!description || strcmp(description,"fighting"));
    if(!minor&&!otherMajor) return false; // No misconduct or fighting-only PP.
    uint16_t at=0;
    if(!absolute(period,time,at))return false;
    Penalty &p=penalties[penaltyCount++];
    p.at=at; p.duration=(uint16_t)(minutes*60);
    p.penalized=penalizedSide; p.goalReleasable=minor;
    return true;
  }
  bool addPpGoal(uint8_t period,const char*time,uint8_t scoringSide) {
    if(!scoringSide || goalCount>=kMaxGoals)return false;
    uint16_t at=0;
    if(!absolute(period,time,at))return false;
    Goal &goal=goals[goalCount++];
    goal.at=at; goal.scoringSide=scoringSide;
    return true;
  }
  // Calculate active minor/major penalty differential. A PP goal cancels
  // the oldest active releasable two-minute penalty segment for the other
  // team. A double-minor advances its active segment; fighting majors never
  // create a PP. Simultaneous offsetting penalties cancel each other.
  uint8_t sideAt(uint16_t now) const {
    if(!loaded)return 0;
    uint16_t endAt[kMaxPenalties]={};
    for(uint8_t i=0;i<penaltyCount;++i)
      endAt[i]=(uint16_t)(penalties[i].at+penalties[i].duration);
    for(uint8_t g=0;g<goalCount;++g) {
      const Goal &goal=goals[g];
      if(goal.at>now)continue;
      uint8_t chosen=255;
      for(uint8_t i=0;i<penaltyCount;++i) {
        const Penalty &p=penalties[i];
        if(!p.goalReleasable || p.penalized==goal.scoringSide ||
           p.at>goal.at || goal.at>=endAt[i])continue;
        if(chosen==255 || p.at<penalties[chosen].at)chosen=i;
      }
      if(chosen!=255) {
        const Penalty &p=penalties[chosen];
        if(p.duration>120 && endAt[chosen]>goal.at+120)
          endAt[chosen]-=120; // one half of a double minor
        else endAt[chosen]=goal.at; // normal minor ends on PP goal
      }
    }
    uint8_t homePen=0,awayPen=0;
    for(uint8_t i=0;i<penaltyCount;++i) {
      const Penalty &p=penalties[i];
      if(now<p.at || now>=endAt[i])continue;
      if(p.penalized==1)++homePen;
      else if(p.penalized==2)++awayPen;
    }
    if(homePen>awayPen)return 2; // away PP
    if(awayPen>homePen)return 1; // home PP
    return 0;
  }
};
} // namespace NhlPenaltyLedger
