#pragma once
#include <stdint.h>
#include "S3NativeLayout.h"

// Phase 6C: visual-only native broadcast theme and placement contracts.
// Pure C++11 constants keep touch targets and page geometry deterministic.
namespace S3BroadcastPages {
constexpr uint16_t kBackground=0x0842;
constexpr uint16_t kSurface=0x1083;
constexpr uint16_t kSurfaceAlt=0x18A4;
constexpr uint16_t kRail=0x2987;
constexpr uint16_t kMuted=0xB5D5;
constexpr uint16_t kAccent=0x36DD;
constexpr uint16_t kWhite=0xFFFF;
constexpr S3NativeLayout::Rect kHeader={0,0,480,48};
constexpr S3NativeLayout::Rect kFooter={0,271,480,49};
constexpr S3NativeLayout::Rect kStatsHero={12,54,456,76};
constexpr S3NativeLayout::Rect kStatsRows={12,134,456,131};
constexpr S3NativeLayout::Rect kStandingsRows={12,106,456,160};
constexpr uint8_t kStatsRowCount=5;
constexpr uint8_t kStandingsRowCount=8;
inline S3NativeLayout::Rect statsRow(uint8_t row) {
  return {12,static_cast<int16_t>(134+27*row),456,25};
}
inline S3NativeLayout::Rect standingsRow(uint8_t row) {
  return {12,static_cast<int16_t>(106+20*row),456,19};
}
inline bool inPanel(const S3NativeLayout::Rect &r){
  return r.x>=0 && r.y>=0 && r.w>0 && r.h>0 &&
         r.x+r.w<=S3NativeLayout::kWidth &&
         r.y+r.h<=S3NativeLayout::kHeight;
}
inline bool overlaps(const S3NativeLayout::Rect &a,
                     const S3NativeLayout::Rect &b){
  return a.x<b.x+b.w && b.x<a.x+a.w &&
         a.y<b.y+b.h && b.y<a.y+a.h;
}
inline bool validPageGeometry(){
  if(!inPanel(kHeader)||!inPanel(kFooter)||!inPanel(kStatsHero)||
     !inPanel(kStatsRows)||!inPanel(kStandingsRows))return false;
  if(overlaps(kHeader,kStatsHero)||overlaps(kFooter,kStatsRows)||
     overlaps(kFooter,kStandingsRows))return false;
  for(uint8_t i=0;i<S3NativeLayout::kSummaryRows;++i){
    const auto r=S3NativeLayout::summaryRow(i);
    if(!inPanel(r)||overlaps(kHeader,r)||overlaps(kFooter,r))return false;
  }
  for(uint8_t i=0;i<kStatsRowCount;++i){
    const auto r=statsRow(i);
    if(!inPanel(r)||overlaps(kFooter,r))return false;
  }
  for(uint8_t i=0;i<kStandingsRowCount;++i){
    const auto r=standingsRow(i);
    if(!inPanel(r)||overlaps(kFooter,r))return false;
  }
  return true;
}
} // namespace S3BroadcastPages
