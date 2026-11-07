#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include "WledScenePolicy.h"
#include "WledZonePolicy.h"
#include "CelebrationEngine.h"
using namespace WledScenePolicy;
int main(){
  static_assert(kSceneCount==15,"5 events across 3 zones");
  static_assert(kSceneBytes==9,"Nine bytes per scene");
  static_assert(kBlobBytes==142,"Versioned 142-byte NVS blob");
  static_assert(kInherit==255,"Legacy 8B defaults");
  Scene library[kSceneCount];
  for(uint8_t i=0;i<kSceneCount;++i){
    const Scene &s=library[i];
    assert(s.fx==kInherit && s.colorMode==kAuto);
    assert(s.duration==0);
    assert(valid(s,uint8_t(i/kZones+1)));
  }
  for(uint8_t e=1;e<=5;++e)
    for(uint8_t z=1;z<=3;++z)
      assert(index(e,z)==(e-1)*3+(z-1));
  assert(index(0,1)==-1 && index(6,1)==-1);
  assert(index(1,0)==-1 && index(1,4)==-1);
  assert(!allowedFx(1) && allowedFx(0) && allowedFx(2));
  assert(allowedFx(3) && allowedFx(12) && allowedFx(17));
  assert(allowedFx(23) && allowedFx(28) && allowedFx(kInherit));
  Scene &goalHome=library[index(1,1)];
  goalHome.fx=23;
  goalHome.speed=220;
  goalHome.intensity=175;
  goalHome.brightness=80;
  goalHome.colorMode=kCustom;
  goalHome.r=0x12;goalHome.g=0x34;goalHome.b=0x56;
  goalHome.duration=3;
  assert(valid(goalHome,1));
  assert(rgb(goalHome,0xFFFFFF,0xAABBDD)==0x123456);
  assert(brightness(goalHome,190)==80);
  assert(durationMs(goalHome,1,8000)==3000);
  const auto desired=effect(goalHome,{0,128,128});
  assert(desired.fx==23 && desired.speed==220 && desired.intensity==175);
  Scene &goalAway=library[index(1,2)];
  goalAway.fx=28;goalAway.colorMode=kTeam;
  goalAway.duration=12;
  assert(rgb(goalAway,0x001122,0xCC55DD)==0xCC55DD);
  assert(durationMs(goalAway,1,8000)==12000);
  Scene &neutral=library[index(1,3)];
  neutral.fx=17;neutral.colorMode=kWhite;
  assert(rgb(neutral,0x22AA33,0xBBAA11)==0xFFFFFF);
  assert(durationMs(neutral,1,8000)==8000);
  const auto inherited=effect(library[index(2,1)],{2,115,160});
  assert(inherited.fx==2 && inherited.speed==115 &&
         inherited.intensity==160);
  assert(rgb(library[index(2,1)],0x444455,0xFFFFFF)==0x444455);
  assert(brightness(library[index(2,1)],187)==187);
  assert(durationMs(library[index(2,1)],2,0)==0);
  // PP lifetime is tied to official NHL man-advantage evidence; any
  // manually bounded PP timeout is invalid.
  Scene &pp=library[index(2,2)];
  pp.fx=2;pp.duration=4;
  assert(!valid(pp,2));
  pp.duration=0;assert(valid(pp,2));
  Scene bad=goalHome;
  bad.fx=1;assert(!valid(bad,1));
  bad=goalHome;bad.brightness=0;assert(!valid(bad,1));
  bad=goalHome;bad.colorMode=4;assert(!valid(bad,1));
  bad=goalHome;bad.duration=1;assert(!valid(bad,1));
  bad=goalHome;bad.duration=31;assert(!valid(bad,1));
  // Persist across reboot as a single versioned, checksummed NVS blob.
  uint8_t blob[kBlobBytes]={};
  encode(library,blob);
  assert(blob[0]=='G' && blob[1]=='S' && blob[2]==kVersion);
  Scene roundTrip[kSceneCount];
  assert(decode(blob,sizeof(blob),roundTrip));
  for(uint8_t i=0;i<kSceneCount;++i){
    assert(roundTrip[i].fx==library[i].fx);
    assert(roundTrip[i].colorMode==library[i].colorMode);
    assert(roundTrip[i].duration==library[i].duration);
  }
  assert(roundTrip[index(1,1)].r==0x12);
  assert(roundTrip[index(1,1)].g==0x34);
  assert(roundTrip[index(1,1)].b==0x56);
  Scene intact[kSceneCount];
  uint8_t corrupt[kBlobBytes];
  memcpy(corrupt,blob,sizeof(blob));
  corrupt[12]^=1;
  assert(!decode(corrupt,sizeof(corrupt),intact));
  assert(!decode(blob,sizeof(blob)-1,intact));
  corrupt[12]^=1;
  corrupt[0]='X';assert(!decode(corrupt,sizeof(corrupt),intact));
  memcpy(corrupt,blob,sizeof(blob));
  corrupt[2]=99;assert(!decode(corrupt,sizeof(corrupt),intact));
  // Corruption never overwrites initialized, INHERIT-only defaults.
  for(uint8_t i=0;i<kSceneCount;++i)assert(intact[i].fx==kInherit);
  // Independent event/zone effect changes must not change routing.
  uint32_t roles=0;
  roles=WledZonePolicy::assign(roles,0,WledZonePolicy::HOME);
  roles=WledZonePolicy::assign(roles,1,WledZonePolicy::AWAY);
  roles=WledZonePolicy::assign(roles,2,WledZonePolicy::NEUTRAL);
  assert(WledZonePolicy::target(true,roles,0,1,1));
  assert(!WledZonePolicy::target(true,roles,1,1,1));
  assert(WledZonePolicy::target(true,roles,2,1,2));
  std::cout<<"PASS WLED 8C: 15 scenes, inherited legacy defaults, "
              "validated FX/timing/colors, CRC reboot persistence and "
              "unchanged Home/Away/Neutral eligibility\n";
}
