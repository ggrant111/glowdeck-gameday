#pragma once
#include <stddef.h>
#include <stdint.h>
#include "CelebrationEngine.h"
#include "WledZonePolicy.h"

// Phase 8D DDP v1 RGB24. Pure C++11; no Wi-Fi, heap or Arduino dependencies.
// DDP is whole-controller realtime output, NOT WLED native segment control.
// Only stream when an EXCLUSIVELY DEDICATED controller has a complete,
// non-overlapping, event-eligible pixel map. Otherwise use Phase 8C JSON.
namespace WledDdpPolicy {
constexpr uint16_t kPort=4048;
constexpr uint16_t kMaxPixels=300;
constexpr uint16_t kHeaderBytes=10;
constexpr uint16_t kPacketBytes=kHeaderBytes+kMaxPixels*3;
constexpr uint8_t kMaxRanges=16;
constexpr uint8_t kMaxConcurrent=2;
constexpr uint8_t kDefaultFps=15;
constexpr uint8_t kMinFps=10,kMaxFps=20;
constexpr uint32_t kPreviewMs=3000UL;
constexpr uint32_t kMaxLiveMs=30000UL; // PP streams yield to JSON (not indefinite)
constexpr uint32_t kHeartbeatMaxGapMs=2000UL;
enum Fail : uint8_t {
  OK=0, NO_PIXELS=1, TOO_MANY_PIXELS=2, BAD_RANGE=3,
  GAP_OR_OVERLAP=4, UNASSIGNED=5, NOT_TARGETED=6, TOO_MANY_RANGES=7,
  DUPLICATE_ID=8
};
struct Range {
  uint16_t start=0,stop=0;
  uint8_t id=0,role=0;
};
struct Layout {
  Range ranges[kMaxRanges];
  uint8_t count=0;
  uint16_t pixels=0;
};
struct Style {
  uint32_t color=0;
  uint8_t fx=0,speed=128,intensity=128,brightness=128;
};
inline bool validFps(int fps) {
  return fps>=kMinFps && fps<=kMaxFps;
}
inline Fail validate(const Layout &layout,uint8_t event,uint8_t scorerSide) {
  if(layout.pixels==0)return NO_PIXELS;
  if(layout.pixels>kMaxPixels)return TOO_MANY_PIXELS;
  if(layout.count==0||layout.count>kMaxRanges)return TOO_MANY_RANGES;
  uint8_t occupied[kMaxPixels]={};
  uint16_t ids=0;
  for(uint8_t i=0;i<layout.count;++i){
    const Range &r=layout.ranges[i];
    if(r.id>=kMaxRanges || (ids & (uint16_t(1U)<<r.id)))
      return DUPLICATE_ID;
    ids|=uint16_t(1U)<<r.id;
    if(r.start>=r.stop || r.stop>layout.pixels)return BAD_RANGE;
    if(r.role==WledZonePolicy::UNASSIGNED ||
       !WledZonePolicy::validRole(r.role))return UNASSIGNED;
    if(!WledZonePolicy::target(true,
         uint32_t(r.role)<<(uint32_t(r.id)*2),r.id,event,scorerSide))
      return NOT_TARGETED;
    for(uint16_t pixel=r.start;pixel<r.stop;++pixel){
      if(occupied[pixel])return GAP_OR_OVERLAP;
      occupied[pixel]=1;
    }
  }
  for(uint16_t pixel=0;pixel<layout.pixels;++pixel)
    if(!occupied[pixel])return GAP_OR_OVERLAP;
  return OK;
}
inline const char* reason(Fail why){
  switch(why){
    case OK:return "DDP ready";
    case NO_PIXELS:return "No WLED pixels";
    case TOO_MANY_PIXELS:return "Over 300 LEDs: native JSON fallback";
    case BAD_RANGE:return "Invalid WLED LED range";
    case GAP_OR_OVERLAP:return "Pixel gaps/overlaps: native JSON fallback";
    case UNASSIGNED:return "Unassigned LEDs: native JSON fallback";
    case NOT_TARGETED:return "Opposite team zone: native JSON fallback";
    case TOO_MANY_RANGES:return "Too many WLED segments";
    case DUPLICATE_ID:return "Duplicate WLED segment IDs";
  }
  return "DDP mapping invalid";
}
inline uint8_t scale(uint8_t value,uint8_t brightness){
  return uint8_t((uint16_t(value)*brightness+127U)/255U);
}
inline uint8_t wave(uint32_t t,uint16_t period) {
  const uint16_t p=uint16_t(t%period),half=period/2;
  return uint8_t(p<half?(uint32_t(p)*255U)/half:
      (uint32_t(period-p)*255U)/half);
}
inline uint8_t effectLevel(const Style &s,uint16_t index,uint16_t length,
                           uint32_t elapsed) {
  const uint32_t speed=uint32_t(s.speed)+32UL;
  const uint16_t period=uint16_t(1800UL-uint32_t(s.speed)*5UL);
  const uint16_t p=period<200?200:period;
  switch(s.fx){
    case 2: return uint8_t(40U+uint16_t(wave(elapsed,p))*215U/255U); // breath
    case 3: return (index*255UL/(length?length:1))<=
       (uint32_t(elapsed)*speed/70UL)%256UL?255:24; // wipe
    case 12:return uint8_t(255-wave(elapsed,p)/2); // fade
    case 17: {
      const uint32_t seed=(uint32_t(index)*2654435761UL)^
        (uint32_t(elapsed/120UL)*2246822519UL);
      return ((seed^(seed>>13))%256U < s.intensity/3U)?255:28;
    }
    case 23:return uint8_t((elapsed%(160U+uint32_t(255-s.speed)*3U))<
                (30U+uint32_t(s.intensity)/3U)?255:0); // strobe
    case 28: {
      const uint16_t cursor=uint16_t((uint32_t(elapsed)*speed/550UL)%
                                     (length?length:1));
      const uint16_t distance=index>=cursor?index-cursor:
                              cursor-index;
      return distance<=((s.intensity/64U)+1U)?255:12;
    }
    default:return 255; // solid and unsupported IDs (already validated)
  }
}
inline uint32_t pixelColor(const Style &s,uint16_t index,uint16_t len,
                           uint32_t elapsed){
  const uint8_t level=effectLevel(s,index,len,elapsed);
  const uint8_t finalBrightness=scale(level,s.brightness);
  const uint8_t r=scale(uint8_t(s.color>>16),finalBrightness);
  const uint8_t g=scale(uint8_t(s.color>>8),finalBrightness);
  const uint8_t b=scale(uint8_t(s.color),finalBrightness);
  return (uint32_t(r)<<16)|(uint32_t(g)<<8)|b;
}
inline bool frame(const Layout &layout,const Style *styles,
                  uint32_t elapsed,uint8_t sequence,
                  uint8_t *out,size_t capacity,size_t &used){
  used=0;
  if(!styles||!out||layout.pixels==0||
     layout.pixels>kMaxPixels||
     capacity<kHeaderBytes+size_t(layout.pixels)*3U)return false;
  const uint16_t channels=uint16_t(layout.pixels*3U);
  out[0]=0x41; // DDP v1 + PUSH
  out[1]=sequence&15U;
  out[2]=0x0B; // RGB24, not legacy 0x01
  out[3]=0x01; // display #1
  out[4]=out[5]=out[6]=out[7]=0; // big-endian channel offset 0
  out[8]=uint8_t(channels>>8);out[9]=uint8_t(channels);
  for(uint8_t i=0;i<layout.count;++i){
    const Range &r=layout.ranges[i];
    if(r.start>=r.stop || r.stop>layout.pixels)return false;
    for(uint16_t n=r.start;n<r.stop;++n){
      const uint32_t rgb=pixelColor(styles[i],n-r.start,r.stop-r.start,
                                    elapsed);
      const size_t offset=kHeaderBytes+size_t(n)*3U;
      out[offset]=uint8_t(rgb>>16);
      out[offset+1]=uint8_t(rgb>>8);
      out[offset+2]=uint8_t(rgb);
    }
  }
  used=kHeaderBytes+channels;
  return true;
}
} // namespace WledDdpPolicy
