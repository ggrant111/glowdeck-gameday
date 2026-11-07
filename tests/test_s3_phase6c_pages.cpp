#include <cassert>
#include <iostream>
#include "S3BroadcastPagesPolicy.h"
using namespace S3BroadcastPages;
using namespace S3NativeLayout;
int main(){
  assert(validPageGeometry());
  assert(kBackground==0x0842);
  assert(kStatsRowCount==5 && kStandingsRowCount==8);
  assert(!overlaps(kHeader,kFooter));
  assert(inPanel(kHeader) && inPanel(kFooter));
  assert(kStatsHero.y==54 && kStatsHero.h==76);
  for(uint8_t i=0;i<kStatsRowCount;++i){
    auto r=statsRow(i);
    assert(r.y>=134 && r.y+r.h<=271);
    assert(!overlaps(r,kFooter));
  }
  for(uint8_t i=0;i<kStandingsRowCount;++i){
    auto r=standingsRow(i);
    assert(r.y>=106 && r.y+r.h<=271);
    assert(!overlaps(r,kFooter));
  }
  for(uint8_t i=0;i<kSummaryRows;++i){
    auto r=summaryRow(i);
    assert(r.x>=0 && r.x+r.w<=kWidth);
    assert(r.y+r.h<=kFooter.y);
    assert(hitTest(444,r.y+24,true).action==Action::SUMMARY_FOLLOW);
    assert(hitTest(240,r.y+24,true).action==Action::SUMMARY_DETAILS);
  }
  assert(statsHitTest(398,24).action==Action::FOLLOW);
  assert(statsHitTest(240,296).action==Action::ARM);
  assert(statsHitTest(405,296).action==Action::SETUP);
  assert(standingsHitTest(405,296).action==Action::NEXT);
  assert(setupHitTest(240,231).action==Action::TOUCH_CHECK);
  assert(touchCheckHitTest(240,296).action==Action::RESET_CHECK);
  assert(overlayCloseHit(452,20));
  assert(kGifViewport.x==80 && kGifViewport.y==40);
  assert(kGifViewport.w==320 && kGifViewport.h==240);
  std::cout<<"PASS S3 6C: new page geometry, unchanged touch targets, GIF viewport\n";
  return 0;
}
