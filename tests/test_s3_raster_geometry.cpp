#include <cassert>
#include <cstdint>
#include <iostream>
#include "S3RasterGeometry.h"

int main(){
  using namespace S3Raster;
  assert(kWidth==480 && kHeight==320);
  assert(roundedRadius(100,45,10)==10);
  assert(roundedRadius(12,8,99)==4);
  assert(roundedRadius(0,16,6)==0);
  assert(roundedRadius(20,20,-5)==0);
  for(int r=1;r<=20;++r){
    int32_t previous=r;
    for(int i=0;i<r;++i){
      const int32_t inset=cornerInset(r,i);
      assert(inset>=0 && inset<=r);
      assert(inset<=previous);
      previous=inset;
    }
  }
  Rect a=cropToPanel({-10,-20,25,45});
  assert(a.x==0 && a.y==0 && a.w==15 && a.h==25);
  a=cropToPanel({460,300,50,50});
  assert(a.x==460 && a.y==300 && a.w==20 && a.h==20);
  a=cropToPanel({480,50,4,4});
  assert(area(a)==0);
  a=cropToPanel({20,20,0,50});
  assert(area(a)==0);

  // Only the clock/score and shots plates are sent during a partial live
  // update. Those transfers exclude hero logos and the next/prev footer.
  const Rect score=cropToPanel(kClockScore),shots=cropToPanel(kShots);
  assert(score.x==154 && score.y==61 && score.w==172 && score.h==129);
  assert(shots.x==14 && shots.y==220 && shots.w==452 && shots.h==45);
  assert(score.y+score.h<shots.y);
  assert(score.x>0 && score.x+score.w<480);
  assert(shots.y+shots.h<275);
  assert(area(score)+area(shots)<(480*320)/3);

  // A full framebuffer clear must replace EVERY previous page pixel before
  // redrawing a stats page, regardless of the prior standings layout.
  uint16_t framebuffer[320][480];
  for(auto &row:framebuffer)for(auto &pixel:row)pixel=0x6AAA;
  for(auto &row:framebuffer)for(auto &pixel:row)pixel=0x0842;
  for(auto &row:framebuffer)for(auto &pixel:row)assert(pixel==0x0842);
  std::cout<<"S3 Phase 5 sprite geometry, clipping and full-scene reset passed\n";
  return 0;
}
