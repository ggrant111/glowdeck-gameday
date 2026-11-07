#include <cassert>
#include <cstdint>
#include <iostream>
#include "S3CaseLedPolicy.h"
#include "TeamPalette.h"
using namespace S3CaseLedPolicy;

static uint8_t red(uint32_t c){return uint8_t(c>>16);}
static uint8_t green(uint32_t c){return uint8_t((c>>8)&255);}
static uint8_t blue(uint32_t c){return uint8_t(c&255);}
int main(){
  static_assert(kDefaultPixels==8,"Default case design should use 8 LEDs/side");
  static_assert(kMaxPixels==16,"Limit up to 16 LEDs/side");
  static_assert(kDefaultBrightness<=kMaxBrightness,"No over-bright defaults");
  static_assert(kFramePeriodMs>=40,"Rate limit RMT refreshes");
  assert(validSide(0)&&validSide(1)&&!validSide(2));
  assert(validPixels(1)&&validPixels(8)&&validPixels(16));
  assert(!validPixels(0)&&!validPixels(17));
  assert(validBrightness(1)&&validBrightness(128));
  assert(!validBrightness(0)&&!validBrightness(129));
  uint32_t uta=0,bos=0;
  assert(TeamPalette::find("UTA",uta));
  assert(TeamPalette::find("BOS",bos));
  assert(uta!=bos);
  Controller c;
  assert(c.pixel(Side::HOME,0,8,0,96)==0);
  assert(c.pixel(Side::AWAY,0,8,0,96)==0);
  assert(c.pixel(static_cast<Side>(4),0,8,0,96)==0);
  assert(c.pixel(Side::HOME,8,8,0,96)==0);
  assert(c.pixel(Side::HOME,0,0,0,96)==0);

  // A home GOAL is HOME/LEFT only, primary color unchanged.
  c.trigger(Side::HOME,Event::GOAL,uta,1000,6000);
  assert(c.eventAt(Side::HOME,1000)==Event::GOAL);
  assert(c.eventAt(Side::AWAY,1000)==Event::IDLE);
  const uint32_t home=c.pixel(Side::HOME,0,8,1000,128);
  assert(home!=0 && red(home)>blue(home)/2);
  assert(c.pixel(Side::AWAY,0,8,1000,128)==0);

  // Verified PP on the other side continues during the home goal.
  c.setPowerPlay(Side::AWAY,true,bos);
  assert(c.eventAt(Side::AWAY,1300)==Event::POWER_PLAY);
  assert(c.pixel(Side::AWAY,0,8,1300,96)!=0);
  c.setPowerPlay(Side::HOME,true,uta);
  assert(c.eventAt(Side::HOME,1500)==Event::GOAL);
  assert(c.eventAt(Side::HOME,7000)==Event::POWER_PLAY);
  assert(c.pixel(Side::HOME,0,8,7050,96)!=0);
  c.setPowerPlay(Side::HOME,false,0);
  assert(c.eventAt(Side::HOME,7050)==Event::IDLE);
  assert(c.pixel(Side::HOME,0,8,7050,96)==0);

  // Simultaneous goals independent: latest home event never removes away.
  c.stop();
  c.trigger(Side::HOME,Event::GOAL,uta,10000,6000);
  c.trigger(Side::AWAY,Event::GOAL,bos,10200,6000);
  assert(c.eventAt(Side::HOME,12000)==Event::GOAL);
  assert(c.eventAt(Side::AWAY,12000)==Event::GOAL);
  const uint32_t awayBefore=c.pixel(Side::AWAY,1,8,12000,100);
  c.trigger(Side::HOME,Event::GOAL,uta,12000,6000);
  assert(c.pixel(Side::AWAY,1,8,12000,100)==awayBefore);
  assert(c.eventAt(Side::HOME,16500)==Event::GOAL);
  assert(c.eventAt(Side::AWAY,16500)==Event::IDLE);

  // PP preview is self-expiring; not persistent verified powerplay state.
  c.stop();
  c.trigger(Side::AWAY,Event::TEST_PP,bos,42);
  assert(c.eventAt(Side::AWAY,100)==Event::TEST_PP);
  assert(c.pixel(Side::AWAY,0,8,100,96)!=0);
  assert(c.eventAt(Side::AWAY,42+kTestMs)==Event::IDLE);
  c.trigger(Side::HOME,Event::TEST,0xFF0000,100);
  const auto p=c.pixel(Side::HOME,0,8,120,128);
  assert(red(p)>0 && green(p)==0 && blue(p)==0);
  c.trigger(Side::HOME,Event::TEST,0x00FF00,100);
  const auto g=c.pixel(Side::HOME,0,8,120,128);
  assert(green(g)>0 && red(g)==0 && blue(g)==0);
  c.trigger(Side::HOME,Event::TEST,0x0000FF,100);
  const auto b=c.pixel(Side::HOME,0,8,120,128);
  assert(blue(b)>0 && red(b)==0 && green(b)==0);

  // Start and finals honor durations and never light idle opposite side.
  c.stop();
  c.trigger(Side::HOME,Event::GAME_START,uta,250);
  c.trigger(Side::AWAY,Event::GAME_START,bos,250);
  assert(c.eventAt(Side::HOME,500)==Event::GAME_START);
  assert(c.eventAt(Side::AWAY,250+kStartMs)==Event::IDLE);
  c.stop();
  c.trigger(Side::AWAY,Event::FINAL,bos,8000);
  assert(c.eventAt(Side::HOME,8100)==Event::IDLE);
  assert(c.eventAt(Side::AWAY,8100)==Event::FINAL);
  assert(c.eventAt(Side::AWAY,8000+kFinalMs)==Event::IDLE);

  // Rapid refresh / millis() wrap: no stale goal persists on rollover.
  c.stop();
  c.trigger(Side::HOME,Event::GOAL,uta,0xFFFFFF00UL,3000);
  assert(c.eventAt(Side::HOME,0x00000080UL)==Event::GOAL);
  assert(c.eventAt(Side::HOME,0x00001000UL)==Event::IDLE);

  // Bounded brightness under repeated independent animations.
  c.stop();
  c.setPowerPlay(Side::AWAY,true,bos);
  c.trigger(Side::HOME,Event::GOAL,uta,1000,6000);
  uint64_t frames=0;
  for(uint32_t time=1000;time<46000;time+=50)
    for(uint8_t pixel=0;pixel<16;++pixel)
      for(uint8_t side=0;side<2;++side){
        const uint32_t v=c.pixel(static_cast<Side>(side),pixel,16,time,128);
        assert(red(v)<=128 && green(v)<=128 && blue(v)<=128);
        ++frames;
      }
  assert(frames>=25000);
  c.stop();
  assert(c.eventAt(Side::AWAY,3000)==Event::IDLE);
  assert(c.pixel(Side::AWAY,0,8,3000,128)==0);
  std::cout<<"PASS S3 Phase 7C: 2 independent strips, primary-color GRB-safe RGB, "
              "PP priority, goal chase, visual tests, wrap and failsafe OFF\n";
}
