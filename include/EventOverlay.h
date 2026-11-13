#pragma once
#include <stdint.h>
#include <string.h>

// Non-blocking, fixed-memory screen overlays for the 320x240 CYD.
// Only state and timing: no TFT, network, audio or heap allocation in here.
namespace EventOverlay {
enum class Kind : uint8_t {
  NONE=0, GOAL, POWER_PLAY_START, POWER_PLAY_END, GAME_START,
  FINAL_WIN, FINAL_LOSS, SHOOTOUT_START, SHOOTOUT_GOAL,
  SHOOTOUT_SAVE, SHOOTOUT_MISS
};
inline uint8_t priority(Kind k) {
  switch (k) {
    case Kind::GOAL: case Kind::SHOOTOUT_GOAL:return 4;
    case Kind::FINAL_WIN: case Kind::FINAL_LOSS: return 3;
    case Kind::GAME_START: case Kind::SHOOTOUT_START:
    case Kind::SHOOTOUT_SAVE:return 2;
    case Kind::SHOOTOUT_MISS:return 1;
    case Kind::POWER_PLAY_START: case Kind::POWER_PLAY_END: return 1;
    default: return 0;
  }
}
inline uint32_t durationMs(Kind k, uint8_t goalSeconds, uint8_t otherSeconds) {
  switch (k) {
    case Kind::GOAL: return uint32_t(goalSeconds)*1000UL;
    case Kind::POWER_PLAY_START: return 4500UL;
    case Kind::POWER_PLAY_END: return 2500UL;
    case Kind::GAME_START: return 4500UL;
    case Kind::SHOOTOUT_START:return 3000UL;
    case Kind::SHOOTOUT_GOAL:return 4500UL;
    case Kind::SHOOTOUT_SAVE:case Kind::SHOOTOUT_MISS:return 2300UL;
    case Kind::FINAL_WIN: case Kind::FINAL_LOSS:
      return uint32_t(otherSeconds)*1000UL;
    default: return 0;
  }
}
inline const char* label(Kind k) {
  switch (k) {
    case Kind::GOAL: return "GOAL!";
    case Kind::POWER_PLAY_START: return "POWER PLAY";
    case Kind::POWER_PLAY_END: return "FULL STRENGTH";
    case Kind::GAME_START: return "PUCK DROP";
    case Kind::SHOOTOUT_START:return "SHOOTOUT!";
    case Kind::SHOOTOUT_GOAL:return "SO GOAL!";
    case Kind::SHOOTOUT_SAVE:return "SO SAVE!";
    case Kind::SHOOTOUT_MISS:return "SO MISSED";
    case Kind::FINAL_WIN: return "FINAL - WIN";
    case Kind::FINAL_LOSS: return "FINAL - LOSS";
    default: return "";
  }
}
inline bool isPulse(Kind k) {
  return k==Kind::POWER_PLAY_START || k==Kind::POWER_PLAY_END;
}
inline bool isFlash(Kind k) {
  return k==Kind::GOAL || k==Kind::SHOOTOUT_GOAL;
}
struct Item {
  Kind kind=Kind::NONE;
  uint32_t gameId=0;
  uint32_t color=0;
  uint32_t eventSequence=0;
  uint32_t duration=0;
  uint32_t enqueuedAt=0;
  uint8_t tier=1; // 0=other live, 1=followed, 2=favorite
  char team[4]={};
  char away[4]={};
  char home[4]={};
  char clock[13]={};
  uint8_t period=0;
  uint8_t awayScore=0,homeScore=0;
};
inline void copy(char* dst, const char* src, uint8_t n) {
  uint8_t i=0;
  if(src) for(;i+1<n && src[i];++i) dst[i]=src[i];
  dst[i]=0;
}
constexpr uint8_t kQueueSize=4;
inline uint8_t rank(const Item &item) {
  return uint8_t(item.tier*8U + priority(item.kind));
}
class Controller {
 public:
  bool active() const { return current_.kind!=Kind::NONE; }
  const Item &current() const { return current_; }
  uint8_t pending() const {return count_;}
  uint32_t startedAt() const {return startedAt_;}
  uint32_t remaining(uint32_t now) const {
    if(!active()) return 0;
    const uint32_t elapsed=now-startedAt_;
    return elapsed>=current_.duration?0:current_.duration-elapsed;
  }
  // A new goal preempts lower-priority overlays immediately; finals queue
  // behind an ongoing goal so a last-second goal shows before the result.
  // Events of equal priority queue instead of endlessly resetting animation.
  bool push(const Item &item,uint32_t now) {
    if(item.kind==Kind::NONE || item.duration==0) return false;
    if(active() && same(current_,item)) return false;
    for(uint8_t i=0;i<count_;++i)
      if(same(queue_[i],item)) return false;
    Item pending=item;
    pending.enqueuedAt=now;
    if(!active() || rank(item)>rank(current_)) {
      // Don't replay low-priority background notices after an interruption.
      if(active() && current_.tier>0 && count_<kQueueSize &&
         remaining(now)>350) {
        Item paused=current_;
        paused.duration=remaining(now);
        paused.enqueuedAt=now;
        insert(paused);
      }
      current_=pending;
      startedAt_=now;
      dirty_=true;
      return true;
    }
    return insert(pending);
  }
  // Return true if the visible overlay changed. The caller should repaint
  // the current item or return to the exact underlying screen.
  bool tick(uint32_t now) {
    if(!active() || remaining(now)) return false;
    next(now);
    return true;
  }
  void dismiss(uint32_t now) { if(active()) next(now); }
  void clear() {
    current_=Item(); count_=0;dirty_=true;startedAt_=0;
  }
  bool takeDirty() { const bool v=dirty_;dirty_=false;return v; }
 private:
  static bool same(const Item&a,const Item&b) {
    return a.kind==b.kind && a.gameId==b.gameId &&
      a.eventSequence && b.eventSequence && a.eventSequence==b.eventSequence;
  }
  bool insert(const Item& item) {
    if(count_>=kQueueSize) {
      // Never evict a goal or final for a lower priority PP event.
      if(rank(item)<=rank(queue_[count_-1]))
        return false;
      --count_;
    }
    uint8_t at=count_;
    while(at && rank(item)>rank(queue_[at-1])) {
      queue_[at]=queue_[at-1];--at;
    }
    queue_[at]=item;
    ++count_;
    return true;
  }
  void next(uint32_t now) {
    current_=Item();
    // Never show an unrelated game notification more than 15s after
    // it was observed; favor timely scores over replaying a long backlog.
    while(count_) {
      Item candidate=queue_[0];
      for(uint8_t i=1;i<count_;++i) queue_[i-1]=queue_[i];
      --count_;
      if(candidate.tier==0 && uint32_t(now-candidate.enqueuedAt)>15000UL)
        continue;
      current_=candidate;
      startedAt_=now;
      break;
    }
    dirty_=true;
  }
  Item current_={};
  Item queue_[kQueueSize]={};
  uint8_t count_=0;
  uint32_t startedAt_=0;
  bool dirty_=false;
};
} // namespace EventOverlay
