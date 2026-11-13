#pragma once
#include <stdint.h>
#include "CelebrationEngine.h"

// Five independent NHL game IDs. Rank 0 is the highest priority and is
// mirrored to legacy selectedGameId for old APIs and preview controls.
namespace MultiGameArming {
constexpr uint8_t kMaxArmed=5;
constexpr uint8_t kQueueCapacity=10;
struct Armed {
  uint32_t ids[kMaxArmed]={};
  uint8_t count() const {
    uint8_t n=0;for(uint8_t i=0;i<kMaxArmed;++i)if(ids[i])++n;return n;
  }
  int8_t rank(uint32_t id) const {
    if(!id)return -1;
    for(uint8_t i=0;i<kMaxArmed;++i)if(ids[i]==id)return int8_t(i);
    return -1;
  }
  bool contains(uint32_t id) const { return rank(id)>=0; }
  uint32_t primary() const { return ids[0]; }
  bool add(uint32_t id) {
    if(!id)return false;
    if(contains(id))return true;
    for(uint8_t i=0;i<kMaxArmed;++i)
      if(!ids[i]){ids[i]=id;return true;}
    return false;
  }
  bool remove(uint32_t id) {
    const int8_t p=rank(id);
    if(p<0)return false;
    for(uint8_t i=uint8_t(p);i+1<kMaxArmed;++i)ids[i]=ids[i+1];
    ids[kMaxArmed-1]=0;
    return true;
  }
  bool promote(uint32_t id) {
    if(!id)return false;
    if(!contains(id) && !add(id))return false;
    const uint8_t p=uint8_t(rank(id));
    for(uint8_t i=p;i>0;--i)ids[i]=ids[i-1];
    ids[0]=id;
    return true;
  }
  bool move(uint32_t id,int direction) {
    const int8_t i=rank(id),j=i+(direction<0?-1:1);
    if(i<0||j<0||j>=kMaxArmed||!ids[j])return false;
    const uint32_t tmp=ids[i];ids[i]=ids[j];ids[j]=tmp;return true;
  }
  void clear(){for(uint8_t i=0;i<kMaxArmed;++i)ids[i]=0;}
};
// GOAL > FINAL > GAME_START > POWER_PLAY; priority amongst equal events
// uses the user-assigned 1..5 rank, then first-observed ordering.
inline uint8_t priority(CelebrationEngine::Event event) {
  switch(event){
    case CelebrationEngine::Event::GOAL:
    case CelebrationEngine::Event::SHOOTOUT_GOAL:return 4;
    case CelebrationEngine::Event::FINAL_WIN:
    case CelebrationEngine::Event::FINAL_LOSS:return 3;
    case CelebrationEngine::Event::GAME_START:
    case CelebrationEngine::Event::SHOOTOUT_START:
    case CelebrationEngine::Event::SHOOTOUT_SAVE:
    case CelebrationEngine::Event::SHOOTOUT_MISS:return 2;
    case CelebrationEngine::Event::POWER_PLAY:return 1;
    default:return 0;
  }
}
struct Request {
  CelebrationEngine::Event event=CelebrationEngine::Event::NONE;
  uint32_t gameId=0,color=0,sequence=0,arrived=0;
  uint8_t side=0;
};
struct Queue {
  Request jobs[kQueueCapacity]={};
  uint8_t count=0;
  uint32_t ticket=0;
  void clear(){count=0;ticket=0;}
  void removeGame(uint32_t id) {
    for(uint8_t i=0;i<count;)
      if(jobs[i].gameId==id){for(uint8_t j=i+1;j<count;++j)jobs[j-1]=jobs[j];--count;}
      else ++i;
  }
  bool add(Request event,const Armed &armed){
    if(!priority(event.event))return false;
    if(event.gameId && !armed.contains(event.gameId))return false;
    for(uint8_t i=0;i<count;++i)
      if(jobs[i].gameId==event.gameId &&
         jobs[i].event==event.event && jobs[i].sequence==event.sequence)
        return true; // No duplicate celebrations for the same NHL observation.
    event.arrived=++ticket;
    if(count<kQueueCapacity){jobs[count++]=event;return true;}
    // Capacity is bounded. Drop the lowest ranked request ONLY when the new
    // event outranks it, so a PP/start never displaces a waiting goal.
    uint8_t worst=0;
    for(uint8_t i=1;i<count;++i)
      if(better(jobs[worst],jobs[i],armed))worst=i;
    if(!better(event,jobs[worst],armed))return false;
    jobs[worst]=event;
    return true;
  }
  static bool better(const Request &a,const Request &b,const Armed &armed){
    const uint8_t pa=priority(a.event),pb=priority(b.event);
    if(pa!=pb)return pa>pb;
    const int8_t ra=armed.rank(a.gameId),rb=armed.rank(b.gameId);
    if(ra!=rb)return (ra<0?99:ra)<(rb<0?99:rb);
    return a.arrived<b.arrived;
  }
  bool pop(Request &out,const Armed &armed){
    if(!count)return false;
    uint8_t best=0;
    for(uint8_t i=1;i<count;++i)
      if(better(jobs[i],jobs[best],armed))best=i;
    out=jobs[best];
    for(uint8_t i=best+1;i<count;++i)jobs[i-1]=jobs[i];
    --count;return true;
  }
};
} // namespace MultiGameArming
