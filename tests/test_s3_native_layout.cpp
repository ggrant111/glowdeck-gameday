#include <cassert>
#include <iostream>
#include "S3NativeLayout.h"

using namespace S3NativeLayout;
int main() {
  assert(kWidth == 480 && kHeight == 320);
  assert(hitTest(398,24,false).action == Action::FOLLOW);
  assert(hitTest(453,24,false).action == Action::MENU);
  assert(hitTest(195,294,false).action == Action::GAMES);
  assert(hitTest(55,296,false).action == Action::PREVIOUS);
  assert(hitTest(403,296,false).action == Action::NEXT);
  assert(hitTest(241,148,false).action == Action::DETAILS);
  assert(hitTest(-1,10,false).action == Action::NONE);
  assert(hitTest(480,10,false).action == Action::NONE);
  assert(hitTest(453,24,true).action == Action::MENU);
  assert(hitTest(398,24,true).action == Action::NONE);
  for (uint8_t row = 0; row < kSummaryRows; ++row) {
    const Rect r = summaryRow(row);
    assert(r.x == 12 && r.w == 456);
    assert(r.y + r.h <= kPrev.y);
    const Hit body = hitTest(175,r.y+23,true);
    assert(body.action == Action::SUMMARY_DETAILS && body.row == row);
    const Hit star = hitTest(444,r.y+23,true);
    assert(star.action == Action::SUMMARY_FOLLOW && star.row == row);
  }
  assert(hitTest(170,267,true).action == Action::NONE);
  // Phase 3 native game-detail and standings navigation are deliberately
  // separated from game/summary hit targets: no accidental arm on standings.
  assert(statsHitTest(398,24).action == Action::FOLLOW);
  assert(statsHitTest(452,24).action == Action::MENU);
  assert(statsHitTest(62,297).action == Action::BACK);
  assert(statsHitTest(230,297).action == Action::ARM);
  assert(statsHitTest(406,297).action == Action::SETUP);
  assert(statsHitTest(210,160).action == Action::NONE);
  assert(statsHitTest(-1,100).action == Action::NONE);
  assert(standingsHitTest(452,24).action == Action::GAMES);
  assert(standingsHitTest(230,297).action == Action::GAMES);
  assert(standingsHitTest(62,297).action == Action::PREVIOUS);
  assert(standingsHitTest(406,297).action == Action::NEXT);
  assert(standingsHitTest(398,24).action == Action::NONE);
  assert(standingsHitTest(110,156).action == Action::NONE);
  assert(standingsHitTest(480,0).action == Action::NONE);
  // Phase 4: 1:1 pixel GIF viewport, full native panel bounds.
  assert(kGifViewport.w == 320 && kGifViewport.h == 240);
  assert(kGifViewport.x == (kWidth - kGifViewport.w)/2);
  assert(kGifViewport.y == (kHeight - kGifViewport.h)/2);
  assert(kGifViewport.x >= 0 && kGifViewport.y >= 0);
  assert(kGifViewport.x+kGifViewport.w <= kWidth);
  assert(kGifViewport.y+kGifViewport.h <= kHeight);
  assert(overlayCloseHit(452,20));
  assert(!overlayCloseHit(239,160));
  assert(!overlayCloseHit(480,20));
  assert(setupHitTest(80,233).action == Action::TEST);
  assert(setupHitTest(240,233).action == Action::TOUCH_CHECK);
  assert(setupHitTest(400,233).action == Action::COLOR_MODE);
  assert(setupHitTest(60,297).action == Action::PREVIOUS);
  assert(setupHitTest(240,297).action == Action::GAMES);
  assert(setupHitTest(405,297).action == Action::BACK);
  assert(setupHitTest(452,22).action == Action::BACK);
  assert(setupHitTest(400,170).action == Action::NONE);
  assert(touchCheckHitTest(48,96).action == Action::TOUCH_CHECK);
  assert(touchCheckHitTest(48,96).row == 0);
  assert(touchCheckHitTest(427,96).row == 1);
  assert(touchCheckHitTest(48,211).row == 2);
  assert(touchCheckHitTest(427,211).row == 3);
  assert(touchCheckHitTest(60,296).action == Action::BACK);
  assert(touchCheckHitTest(240,296).action == Action::RESET_CHECK);
  assert(touchCheckHitTest(405,296).action == Action::GAMES);
  assert(touchCheckHitTest(240,170).action == Action::NONE);
  assert(touchCheckHitTest(-1,20).action == Action::NONE);
  // Phase 6B clean broadcast navigation: swipe carousel, score -> stats,
  // follow star and tiny corner controls; no 3-button footer.
  assert(broadcastHitTest(401,23).action==Action::FOLLOW);
  assert(broadcastHitTest(24,301).action==Action::GAMES);
  assert(broadcastHitTest(120,301).action==Action::PREVIOUS);
  assert(broadcastHitTest(240,301).action==Action::PAUSE);
  assert(broadcastHitTest(360,301).action==Action::NEXT);
  assert(broadcastHitTest(453,301).action==Action::MENU);
  assert(broadcastHitTest(240,148).action==Action::DETAILS);
  assert(broadcastHitTest(80,144).action==Action::DETAILS);  // Away logo
  assert(broadcastHitTest(393,144).action==Action::DETAILS); // Home logo
  assert(broadcastHitTest(110,240).action==Action::DETAILS); // Away SOG
  assert(broadcastHitTest(365,240).action==Action::DETAILS); // Home SOG
  assert(broadcastHitTest(240,258).action==Action::DETAILS); // PP/EN
  assert(broadcastHitTest(20,155).action==Action::NONE);
  assert(broadcastHitTest(240,270).action==Action::NONE);
  assert(broadcastHitTest(401,23).action==Action::FOLLOW);
  assert(broadcastHitTest(24,301).action==Action::GAMES);
  assert(broadcastHitTest(453,301).action==Action::MENU);
  assert(broadcastTap(0,0));
  assert(broadcastTap(23,-24));
  assert(!broadcastTap(25,0));
  assert(!broadcastTap(0,25));
  assert(!broadcastTap(-55,33)); // diagonal drag, not a tap
  assert(!broadcastTap(50,100)); // not a swipe or a tap
  assert(broadcastHitTest(479,319).action==Action::MENU);
  assert(broadcastHitTest(480,300).action==Action::NONE);
  assert(broadcastHitTest(-1,150).action==Action::NONE);
  assert(kBroadcastReadoutTop.x>=130 &&
         kBroadcastReadoutTop.x+kBroadcastReadoutTop.w<=350);
  assert(kBroadcastReadoutTop.y+kBroadcastReadoutTop.h<
         kBroadcastReadoutBottom.y);
  assert(kBroadcastReadoutBottom.y+kBroadcastReadoutBottom.h<=272);
  assert(carouselSwipe(-66,0));
  assert(carouselSwipe(70,20));
  assert(!carouselSwipe(20,3));
  assert(!carouselSwipe(75,110));
  std::cout << "S3 native phases 1-4 UI geometry, GIF viewport and touch tests passed\n";
  return 0;
}
