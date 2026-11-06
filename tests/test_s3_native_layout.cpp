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
  assert(carouselSwipe(-66,0));
  assert(carouselSwipe(70,20));
  assert(!carouselSwipe(20,3));
  assert(!carouselSwipe(75,110));
  std::cout << "S3 480x320 native hit targets passed\n";
  return 0;
}
