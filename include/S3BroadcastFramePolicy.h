#pragma once
#include <stdint.h>

// Phase 6B.4: pure, allocation-free change tracking for the S3 Arena screen.
// A full scene is still required on game/slide changes and overlay recovery.
namespace S3BroadcastFrame {
constexpr uint8_t NONE=0, TOP=1, BOTTOM=2, BOTH=TOP|BOTTOM;
constexpr uint32_t kFNVOffset=2166136261UL;
inline void mixByte(uint32_t &h,uint8_t byte){
  h^=byte;
  h*=16777619UL;
}
inline void mixNumber(uint32_t &h,uint32_t value){
  for(uint8_t i=0;i<4;++i)mixByte(h,uint8_t(value>>(i*8)));
}
inline void mixText(uint32_t &h,const char *text){
  if(text)while(*text)mixByte(h,uint8_t(*text++));
  mixByte(h,0xFF); // delimit adjacent fields
}
struct Snapshot {
  uint32_t gameId=0, top=0, bottom=0;
  bool valid=false;
};
inline uint8_t changed(const Snapshot &previous,const Snapshot &next){
  if(!previous.valid || !next.valid || previous.gameId!=next.gameId)
    return BOTH;
  return (previous.top!=next.top?TOP:NONE)|
         (previous.bottom!=next.bottom?BOTTOM:NONE);
}
inline bool needsFullScene(const Snapshot &previous,const Snapshot &next){
  return !previous.valid || !next.valid || previous.gameId!=next.gameId;
}
// Software-only estimate for RGB565 SPI transfer; actual panel timing is
// measured by the firmware. No extra framebuffer or per-frame allocations.
constexpr uint32_t kFullBytes=480UL*320UL*2UL;
constexpr uint32_t kTopBytes=192UL*160UL*2UL;
constexpr uint32_t kBottomBytes=384UL*53UL*2UL;
inline uint32_t transferBytes(uint8_t mask){
  return ((mask&TOP)?kTopBytes:0UL)+
         ((mask&BOTTOM)?kBottomBytes:0UL);
}
} // namespace S3BroadcastFrame
