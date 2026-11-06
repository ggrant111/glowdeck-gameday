#pragma once
#include <stdint.h>

// Host-testable native S3 framebuffer geometry. All coordinates are physical
// 480x320 pixels. These helpers don't depend on Arduino, SPI or TFT_eSPI.
namespace S3Raster {
constexpr int16_t kWidth=480, kHeight=320;
struct Rect { int32_t x,y,w,h; };
inline int32_t roundedRadius(int32_t w,int32_t h,int32_t r) {
  if(w<=0 || h<=0 || r<=0)return 0;
  const int32_t m=w<h?w:h;
  return r>m/2?m/2:r;
}
// Inset of a horizontal scanline in the top/bottom rounded corner.
// Pixel-circle arithmetic avoids sqrt(), float allocations and jagged gaps.
inline int32_t cornerInset(int32_t radius,int32_t row) {
  if(radius<=0 || row<0 || row>=radius)return 0;
  const int32_t dy=radius-row-1;
  int32_t dx=radius;
  while(dx>0 && dx*dx+dy*dy>radius*radius)--dx;
  return radius-dx;
}
inline Rect cropToPanel(Rect r) {
  if(r.w<=0 || r.h<=0)return {0,0,0,0};
  if(r.x<0){r.w+=r.x;r.x=0;}
  if(r.y<0){r.h+=r.y;r.y=0;}
  if(r.x>=kWidth || r.y>=kHeight)return {0,0,0,0};
  if(r.x+r.w>kWidth)r.w=kWidth-r.x;
  if(r.y+r.h>kHeight)r.h=kHeight-r.y;
  if(r.w<=0 || r.h<=0)return {0,0,0,0};
  return r;
}
inline int32_t area(const Rect &r) {
  return r.w>0 && r.h>0?r.w*r.h:0;
}
// Regions covered by drawS3Readout(); a score/status refresh must never
// overwrite the logo cards, game picker, standings or other screens.
constexpr Rect kClockScore = {154,61,172,129};
constexpr Rect kShots = {14,220,452,45};
} // namespace S3Raster
