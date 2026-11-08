#include <cassert>
#include <cmath>
#include "HueColorPolicy.h"
int main(){
  using namespace HuePolicy;
  assert(validResourceId("d1a42d29-d1a4-4ab1-9813-d1a4d1a4d1a4"));
  assert(!validResourceId("8"));
  assert(!validResourceId("00000000-0000-0000-0000-00000000000z"));
  const Xy red=rgbToXy(0xff0000),blue=rgbToXy(0x0000ff),black=rgbToXy(0);
  assert(red.x>blue.x && red.y>blue.y);
  assert(std::isfinite(black.x) && std::isfinite(black.y));
  assert(black.x>0 && black.x<1 && black.y>0 && black.y<1);
  assert(!participates(false,15,CelebrationEngine::Event::GOAL));
  assert(participates(true,1,CelebrationEngine::Event::GOAL));
  assert(!participates(true,1,CelebrationEngine::Event::POWER_PLAY));
  assert(brightnessFor(CelebrationEngine::Event::POWER_PLAY,75,true)==25);
  return 0;
}
