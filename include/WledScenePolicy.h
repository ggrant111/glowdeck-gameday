#pragma once
#include <stddef.h>
#include <stdint.h>
#include "CelebrationEngine.h"
#include "WledZonePolicy.h"

// Phase 8C: one opt-in native-JSON scene per (event, zone), per WLED device.
// Everything here is C++11, heap-free and shared by CYD / Waveshare S3.
// No extra event sources, WLED transport, NVS, or GPIO dependencies.
namespace WledScenePolicy {
constexpr uint8_t kEvents=5;   // GOAL, PP, START, WIN, LOSS
constexpr uint8_t kZones=3;    // HOME, AWAY, NEUTRAL
constexpr uint8_t kSceneCount=kEvents*kZones;
constexpr uint8_t kInherit=255; // exact Phase 8B behavior if not customized
constexpr uint8_t kAuto=0,kTeam=1,kWhite=2,kCustom=3;
constexpr uint8_t kVersion=1;
constexpr size_t kSceneBytes=9;
constexpr size_t kBlobBytes=3+kSceneCount*kSceneBytes+4;
constexpr uint8_t kAllowedFx[]={0,2,3,12,17,23,28};
constexpr uint8_t kAllowedFxCount=sizeof(kAllowedFx)/sizeof(kAllowedFx[0]);

struct Scene {
  uint8_t fx,speed,intensity,brightness,colorMode,r,g,b,duration;
  Scene():fx(kInherit),speed(128),intensity(128),brightness(190),
    colorMode(kAuto),r(0),g(0),b(0),duration(0){}
};
static_assert(sizeof(Scene)==kSceneBytes,"Scene record must stay compact");
inline bool allowedFx(uint8_t fx) {
  if(fx==kInherit)return true;
  for(uint8_t i=0;i<kAllowedFxCount;++i)
    if(kAllowedFx[i]==fx)return true;
  return false;
}
inline bool isTemporary(uint8_t event) {
  return event!=uint8_t(CelebrationEngine::Event::POWER_PLAY);
}
inline bool validDuration(uint8_t event,uint8_t duration) {
  return duration==0 ||
    (isTemporary(event) && duration>=2 && duration<=30);
}
inline bool valid(const Scene &scene,uint8_t event) {
  return allowedFx(scene.fx) &&
    scene.brightness>=1 && scene.colorMode<=kCustom &&
    validDuration(event,scene.duration);
}
inline int index(uint8_t event,uint8_t role) {
  if(event<1||event>kEvents||role<1||role>kZones)return -1;
  return int(event-1)*kZones+int(role-1);
}
inline uint32_t rgb(const Scene &scene,uint32_t fallback,
                    uint32_t teamRgb) {
  if(scene.fx==kInherit||scene.colorMode==kAuto)return fallback;
  if(scene.colorMode==kTeam)return teamRgb;
  if(scene.colorMode==kWhite)return 0xFFFFFFUL;
  return (uint32_t(scene.r)<<16)|(uint32_t(scene.g)<<8)|scene.b;
}
inline CelebrationEngine::Effect effect(const Scene &scene,
                                        CelebrationEngine::Effect fallback) {
  if(scene.fx==kInherit)return fallback;
  return {scene.fx,scene.speed,scene.intensity};
}
inline uint8_t brightness(const Scene &scene,uint8_t fallback) {
  return scene.fx==kInherit?fallback:scene.brightness;
}
inline uint32_t durationMs(const Scene &scene,uint8_t event,
                           uint32_t fallback) {
  return scene.fx!=kInherit && validDuration(event,scene.duration) &&
      scene.duration?uint32_t(scene.duration)*1000UL:fallback;
}
inline uint32_t checksum(const uint8_t *bytes,size_t count) {
  uint32_t hash=2166136261UL;
  for(size_t i=0;i<count;++i)hash=(hash^bytes[i])*16777619UL;
  return hash;
}
inline void encode(const Scene (&scenes)[kSceneCount],
                   uint8_t (&blob)[kBlobBytes]) {
  blob[0]='G';blob[1]='S';blob[2]=kVersion;
  for(uint8_t i=0;i<kSceneCount;++i){
    const Scene &s=scenes[i];
    const size_t offset=3+size_t(i)*kSceneBytes;
    blob[offset]=s.fx;blob[offset+1]=s.speed;blob[offset+2]=s.intensity;
    blob[offset+3]=s.brightness;blob[offset+4]=s.colorMode;
    blob[offset+5]=s.r;blob[offset+6]=s.g;blob[offset+7]=s.b;
    blob[offset+8]=s.duration;
  }
  const uint32_t crc=checksum(blob,kBlobBytes-4);
  for(uint8_t i=0;i<4;++i)blob[kBlobBytes-4+i]=uint8_t(crc>>(i*8));
}
inline bool decode(const uint8_t *blob,size_t bytes,
                   Scene (&scenes)[kSceneCount]) {
  if(!blob||bytes!=kBlobBytes||blob[0]!='G'||blob[1]!='S'||
     blob[2]!=kVersion)return false;
  const uint32_t want=checksum(blob,kBlobBytes-4);
  uint32_t stored=0;
  for(uint8_t i=0;i<4;++i)stored|=uint32_t(blob[kBlobBytes-4+i])<<(i*8);
  if(want!=stored)return false;
  Scene staged[kSceneCount];
  for(uint8_t i=0;i<kSceneCount;++i){
    const size_t o=3+size_t(i)*kSceneBytes;
    Scene &s=staged[i];
    s.fx=blob[o];s.speed=blob[o+1];s.intensity=blob[o+2];
    s.brightness=blob[o+3];s.colorMode=blob[o+4];
    s.r=blob[o+5];s.g=blob[o+6];s.b=blob[o+7];s.duration=blob[o+8];
    const uint8_t event=uint8_t(i/kZones+1);
    if(!valid(s,event))return false;
  }
  for(uint8_t i=0;i<kSceneCount;++i)scenes[i]=staged[i];
  return true;
}
} // namespace WledScenePolicy
