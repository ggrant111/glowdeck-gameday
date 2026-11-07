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
// 480 RGB pixels fit into a 1450-byte DDP UDP datagram (no IP fragmentation).
// The 4096-pixel controller limit is an explicit embedded throughput guard,
// not a DDP protocol limit. Only one small packet buffer is allocated.
constexpr uint16_t kMaxPixels=4096;
constexpr uint16_t kChunkPixels=480;
constexpr uint16_t kHeaderBytes=10;
constexpr uint16_t kPacketBytes=kHeaderBytes+kChunkPixels*3;
constexpr uint16_t kMaxPackets=(kMaxPixels+kChunkPixels-1U)/kChunkPixels;
inline uint16_t packetCount(uint16_t pixels) {
  return pixels ? uint16_t((uint32_t(pixels)+kChunkPixels-1U)/kChunkPixels):0;
}
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
  uint16_t start,stop;
  uint8_t id,role;
  Range(uint16_t a=0,uint16_t b=0,uint8_t c=0,uint8_t d=0):
      start(a),stop(b),id(c),role(d){}
};
struct Layout {
  Range ranges[kMaxRanges];
  uint8_t count=0;
  uint16_t pixels=0;
};
struct Style {
  uint32_t color;
  uint8_t fx,speed,intensity,brightness;
  Style(uint32_t c=0,uint8_t f=0,uint8_t s=128,uint8_t i=128,
        uint8_t b=128):color(c),fx(f),speed(s),intensity(i),brightness(b){}
};
inline bool validFps(int fps) {
  return fps>=kMinFps && fps<=kMaxFps;
}
inline Fail validate(const Layout &layout,uint8_t event,uint8_t scorerSide) {
  if(layout.pixels==0)return NO_PIXELS;
  if(layout.pixels>kMaxPixels)return TOO_MANY_PIXELS;
  if(layout.count==0||layout.count>kMaxRanges)return TOO_MANY_RANGES;
  uint16_t ids=0;
  uint32_t covered=0;
  // Check exact full-strip ownership without a pixel-sized stack bitmap.
  // Each of at most 16 ranges is checked for overlap with earlier ranges;
  // no overlaps plus summed length == pixels proves there are no gaps.
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
    for(uint8_t j=0;j<i;++j){
      const Range &prior=layout.ranges[j];
      if(r.start<prior.stop && prior.start<r.stop)return GAP_OR_OVERLAP;
    }
    covered+=uint32_t(r.stop-r.start);
  }
  return covered==layout.pixels?OK:GAP_OR_OVERLAP;
}
inline const char* reason(Fail why){
  switch(why){
    case OK:return "DDP ready";
    case NO_PIXELS:return "No WLED pixels";
    case TOO_MANY_PIXELS:return "Over 4096 LEDs: native JSON fallback";
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
// Render only one MTU-safe chunk. Offsets in DDP are BYTE offsets, not
// LED indices. All packets of a frame must be sent in order, with PUSH on
// ONLY the final chunk, so WLED displays the completed frame.
inline bool frameChunk(const Layout &layout,const Style *styles,
                       uint32_t elapsed,uint8_t sequence,
                       uint16_t firstPixel,uint8_t *out,
                       size_t capacity,size_t &used) {
  used=0;
  if(!styles||!out||layout.pixels==0||layout.pixels>kMaxPixels||
     layout.count==0||layout.count>kMaxRanges||
     firstPixel>=layout.pixels||firstPixel%kChunkPixels!=0)
    return false;
  const uint16_t remaining=uint16_t(layout.pixels-firstPixel);
  const uint16_t count=remaining<kChunkPixels?remaining:kChunkPixels;
  const uint16_t bytes=uint16_t(count*3U);
  if(capacity<kHeaderBytes+size_t(bytes))return false;
  const uint32_t offset=uint32_t(firstPixel)*3U;
  out[0]=(firstPixel+count==layout.pixels)?0x41:0x40; // PUSH only last
  out[1]=uint8_t(sequence%15U+1U); // 1..15: zero disables DDP sequencing
  out[2]=0x0B; // RGB24
  out[3]=0x01; // display #1
  out[4]=uint8_t(offset>>24);out[5]=uint8_t(offset>>16);
  out[6]=uint8_t(offset>>8);out[7]=uint8_t(offset); // big endian
  out[8]=uint8_t(bytes>>8);out[9]=uint8_t(bytes);
  const uint16_t lastPixel=uint16_t(firstPixel+count);
  uint16_t painted=0;
  for(uint8_t i=0;i<layout.count;++i){
    const Range &r=layout.ranges[i];
    if(r.start>=r.stop||r.stop>layout.pixels)return false;
    const uint16_t from=r.start>firstPixel?r.start:firstPixel;
    const uint16_t to=r.stop<lastPixel?r.stop:lastPixel;
    for(uint16_t n=from;n<to;++n){
      const uint32_t rgb=pixelColor(styles[i],uint16_t(n-r.start),
                                     uint16_t(r.stop-r.start),elapsed);
      const size_t p=kHeaderBytes+size_t(n-firstPixel)*3U;
      out[p]=uint8_t(rgb>>16);
      out[p+1]=uint8_t(rgb>>8);
      out[p+2]=uint8_t(rgb);
      ++painted;
    }
  }
  if(painted!=count)return false; // fail closed on uncovered pixel ranges
  used=kHeaderBytes+bytes;
  return true;
}
} // namespace WledDdpPolicy
