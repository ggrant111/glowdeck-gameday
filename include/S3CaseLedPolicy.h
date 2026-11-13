#pragma once
#include <stdint.h>

// Phase 7C – pure, host-tested home/away LED effects. No GPIO, I2C,
// networking, filesystem, audio or heap use in this module.
//
// LEFT strip = HOME; RIGHT strip = AWAY. Each channel has its own goal
// animation and power-play pulse. A goal overrides only its OWN channel;
// the opposite side keeps running independently.
namespace S3CaseLedPolicy {
enum class Side : uint8_t { HOME=0, AWAY=1 };
enum class Event : uint8_t { IDLE=0, GOAL=1, POWER_PLAY=2,
                              GAME_START=3, FINAL=4, TEST=5, TEST_PP=6, SHOOTOUT=7 };
constexpr uint8_t kDefaultPixels=8;
constexpr uint8_t kMaxPixels=16;
constexpr uint8_t kDefaultBrightness=96;
constexpr uint8_t kMaxBrightness=128; // 50% channel cap; power supply still sized worst case
constexpr uint16_t kFramePeriodMs=50; // max 20Hz updates; RMT completes in ~1ms
constexpr uint32_t kStartMs=3500UL;
constexpr uint32_t kFinalMs=7000UL;
constexpr uint32_t kTestMs=3000UL;
constexpr uint32_t kDefaultGoalMs=6000UL;
constexpr uint32_t kPulsePeriodMs=1200UL;

inline bool validSide(uint8_t side){return side<2;}
inline bool validPixels(uint8_t n){return n>=1 && n<=kMaxPixels;}
inline bool validBrightness(uint8_t n){return n>=1 && n<=kMaxBrightness;}
inline uint32_t scale(uint32_t rgb,uint8_t level){
  const uint32_t r=((rgb>>16)&0xFFU)*uint32_t(level)/255UL;
  const uint32_t g=((rgb>>8)&0xFFU)*uint32_t(level)/255UL;
  const uint32_t b=(rgb&0xFFU)*uint32_t(level)/255UL;
  return (r<<16)|(g<<8)|b;
}
inline uint8_t pulse(uint32_t elapsed){
  const uint32_t p=elapsed%kPulsePeriodMs;
  const uint32_t rise=p<kPulsePeriodMs/2?p:kPulsePeriodMs-p;
  return uint8_t(45UL+rise*175UL/(kPulsePeriodMs/2));
}
struct Channel {
  Event event=Event::IDLE;
  uint32_t color=0,started=0,duration=0;
  bool pp=false;
  uint32_t ppColor=0;
};
class Controller {
 public:
  void stop(){for(uint8_t i=0;i<2;++i)ch_[i]=Channel{};}
  void setPowerPlay(Side side,bool active,uint32_t color){
    if(!validSide(static_cast<uint8_t>(side)))return;
    Channel &c=ch_[static_cast<uint8_t>(side)];
    c.pp=active;
    c.ppColor=active?color:0;
  }
  void trigger(Side side,Event event,uint32_t rgb,uint32_t now,
               uint32_t duration=0){
    const uint8_t n=static_cast<uint8_t>(side);
    if(!validSide(n)||event==Event::IDLE||event==Event::POWER_PLAY)return;
    Channel &c=ch_[n];
    c.event=event;
    c.color=rgb&0xFFFFFFUL;
    c.started=now;
    c.duration=duration?duration:
        event==Event::GOAL?kDefaultGoalMs:
        event==Event::GAME_START?kStartMs:
        event==Event::FINAL?kFinalMs:event==Event::SHOOTOUT?3200UL:kTestMs;
  }
  Event eventAt(Side side,uint32_t now)const{
    if(!validSide(static_cast<uint8_t>(side)))return Event::IDLE;
    const Channel &c=ch_[static_cast<uint8_t>(side)];
    if(c.event!=Event::IDLE &&
       uint32_t(now-c.started)<c.duration)return c.event;
    return c.pp?Event::POWER_PLAY:Event::IDLE;
  }
  uint32_t pixel(Side side,uint8_t index,uint8_t count,
                 uint32_t now,uint8_t brightness)const{
    if(!validSide(static_cast<uint8_t>(side)) ||
       !validPixels(count)||index>=count||!validBrightness(brightness))
      return 0;
    const Channel &c=ch_[static_cast<uint8_t>(side)];
    const Event event=eventAt(side,now);
    if(event==Event::IDLE)return 0;
    uint8_t level=0;
    const uint32_t elapsed=uint32_t(now-c.started);
    switch(event){
      case Event::GOAL:
        // Bright synchronized strobe for first second, then rotating chase.
        if(elapsed<1000UL)
          level=(elapsed/110UL)%2U?20:255;
        else{
          const uint8_t head=uint8_t(((elapsed-1000UL)/95UL)%count);
          const uint8_t back=uint8_t((head+count-1U)%count);
          level=index==head?255:index==back?110:22;
        }
        break;
      case Event::POWER_PLAY:level=pulse(now);break;
      case Event::TEST_PP:level=pulse(elapsed);break;
      case Event::GAME_START:
        level=(elapsed/190UL)%2U?80:210;
        break;
      case Event::FINAL:
        level=(uint8_t((index*7U)+(elapsed/180UL))%6U)==0U?255:32;
        break;
      case Event::TEST:level=255;break;
      case Event::SHOOTOUT:
        level=((elapsed/140UL+index)%4U)==0U?245:20;break;
      default:break;
    }
    const uint8_t scaled=uint8_t(uint16_t(level)*brightness/255U);
    return scale(event==Event::POWER_PLAY?c.ppColor:c.color,scaled);
  }
 private:
  Channel ch_[2];
};
} // namespace S3CaseLedPolicy
