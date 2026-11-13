#pragma once
#include <stdint.h>
#include <string.h>
// Fixed-memory, NHL game-ID scoped shootout tracker. Never derive SO attempts
// from the scoreboard: NHL adds only ONE winner goal to the final game score.
namespace NhlShootoutPolicy {
enum class Attempt:uint8_t {NONE, GOAL, SAVE, MISS};
inline Attempt classify(const char* period,uint16_t type){
  if(!period||strcmp(period,"SO"))return Attempt::NONE;
  if(type==505)return Attempt::GOAL;
  if(type==506)return Attempt::SAVE;
  if(type==507||type==537)return Attempt::MISS;
  return Attempt::NONE;
}
struct Watch {
  uint32_t gameId=0,lastSort=0;
  uint8_t homeAttempts=0,awayAttempts=0,homeGoals=0,awayGoals=0;
  bool periodKnown=false,active=false,entered=false,transitionSeen=false,playsKnown=false;
  void reset(uint32_t id=0){*this=Watch{};gameId=id;}
  bool observe(uint32_t id,bool live,const char*period){
    if(gameId!=id)reset(id);
    const bool now=live&&period&&!strcmp(period,"SO");
    const bool start=periodKnown&&!entered&&now;
    if(now&&!entered){
      entered=true;
      transitionSeen=start;playsKnown=false;lastSort=0;
      homeAttempts=awayAttempts=homeGoals=awayGoals=0;
    }
    periodKnown=true;active=now;
    return start;
  }
  void record(uint8_t shooter,Attempt outcome){
    if(outcome==Attempt::NONE)return;
    if(shooter==1){if(homeAttempts<255)++homeAttempts;
      if(outcome==Attempt::GOAL&&homeGoals<255)++homeGoals;}
    if(shooter==2){if(awayAttempts<255)++awayAttempts;
      if(outcome==Attempt::GOAL&&awayGoals<255)++awayGoals;}
  }
};
}
