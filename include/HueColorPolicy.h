#pragma once
#include <stdint.h>
#include <math.h>
#include "CelebrationEngine.h"

// Pure policy shared by CYD, Waveshare S3 and host-side tests.
// Hue CLIP v2 uses CIE 1931 xy, not RGB hex strings.
namespace HuePolicy {
constexpr uint8_t kMaxLights = 4;
struct Xy { float x, y; };
inline float linear(uint8_t channel) {
  const float c = float(channel) / 255.0f;
  return c > 0.04045f ? powf((c+0.055f)/1.055f,2.4f) : c/12.92f;
}
inline Xy rgbToXy(uint32_t color) {
  const float r=linear(uint8_t(color>>16)), g=linear(uint8_t(color>>8)),
              b=linear(uint8_t(color));
  const float X=r*0.664511f+g*0.154324f+b*0.162028f;
  const float Y=r*0.283881f+g*0.668433f+b*0.047685f;
  const float Z=r*0.000088f+g*0.072310f+b*0.986039f;
  const float sum=X+Y+Z;
  if(sum<0.00001f) return {0.3127f,0.3290f};
  return {X/sum,Y/sum};
}
inline bool validResourceId(const char *id) {
  if(!id) return false;
  for(uint8_t i=0;i<36;++i) {
    const char c=id[i];
    if(!c) return false;
    if(i==8||i==13||i==18||i==23) {
      if(c!='-')return false;
    } else if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||
                (c>='A'&&c<='F')))return false;
  }
  return id[36]=='\0';
}
inline bool participates(bool enabled,uint8_t mask,
                         CelebrationEngine::Event event) {
  return CelebrationEngine::eligible(enabled,mask,event);
}
inline uint8_t brightnessFor(CelebrationEngine::Event event,uint8_t limit,
                             bool pulseLow=false) {
  if(event==CelebrationEngine::Event::POWER_PLAY)
    return pulseLow?uint8_t(limit/3>5?limit/3:5):limit;
  return limit;
}
} // namespace HuePolicy
