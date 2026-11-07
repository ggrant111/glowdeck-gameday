#include <cassert>
#include <cstdint>
#include <iostream>
#include "WledZonePolicy.h"
#include "CelebrationEngine.h"
using namespace WledZonePolicy;
int main(){
  static_assert(kMaxSegments==16,"Controller zone cap");
  static_assert(CelebrationEngine::kMaxSegments==kMaxSegments,
                "Native WLED segment cap matches policy");
  uint32_t map=0;
  for(int i=0;i<16;++i)assert(role(map,i)==UNASSIGNED);
  assert(!validSegment(-1)&&validSegment(15)&&!validSegment(16));
  assert(!validRole(-1)&&validRole(0)&&validRole(3)&&!validRole(4));
  map=assign(map,0,HOME);
  map=assign(map,1,AWAY);
  map=assign(map,2,NEUTRAL);
  map=assign(map,15,HOME);
  assert(role(map,0)==HOME&&role(map,1)==AWAY);
  assert(role(map,2)==NEUTRAL&&role(map,3)==UNASSIGNED);
  assert(role(map,15)==HOME);
  uint32_t before=map;
  assert(assign(map,16,AWAY)==before);
  assert(assign(map,0,static_cast<Role>(7))==before);
  map=assign(map,1,UNASSIGNED);
  assert(role(map,1)==UNASSIGNED);
  map=assign(map,1,AWAY);
  // Legacy devices must still illuminate every valid segment.
  for(uint8_t i=0;i<16;++i)
    assert(target(false,0,i,1,kHomeSide));
  // Real HOME goal: home+neutral, no away/unassigned; vice versa away.
  assert(target(true,map,0,1,kHomeSide));
  assert(!target(true,map,0,1,kAwaySide));
  assert(target(true,map,1,1,kAwaySide));
  assert(!target(true,map,1,1,kHomeSide));
  assert(target(true,map,2,1,kHomeSide));
  assert(target(true,map,2,1,kAwaySide));
  assert(!target(true,map,3,1,kHomeSide));
  assert(!target(true,map,2,1,kUnknownSide));
  // PP must follow same side restrictions; start/final include all assigned.
  assert(target(true,map,0,2,kHomeSide));
  assert(!target(true,map,1,2,kHomeSide));
  for(uint8_t ev=3;ev<=5;++ev){
    assert(target(true,map,0,ev,kUnknownSide));
    assert(target(true,map,1,ev,kUnknownSide));
    assert(target(true,map,2,ev,kUnknownSide));
    assert(!target(true,map,3,ev,kUnknownSide));
  }
  const uint32_t home=0x135724,away=0xA05912;
  assert(zoneColor(HOME,1,home,home,away)==home);
  assert(zoneColor(AWAY,1,away,home,away)==away);
  assert(zoneColor(NEUTRAL,1,home,home,away)==0xFFFFFF);
  assert(zoneColor(NEUTRAL,2,home,home,away)==0xAABBC8);
  assert(zoneColor(HOME,3,away,home,away)==home);
  assert(zoneColor(AWAY,3,home,home,away)==away);
  assert(zoneColor(HOME,4,away,home,away)==home);
  assert(zoneColor(AWAY,5,home,home,away)==away);
  // No bit spill into adjacent role when overwriting every segment.
  uint32_t all=0;
  for(uint8_t i=0;i<16;++i)all=assign(all,i,NEUTRAL);
  assert(all==UINT32_MAX);
  for(uint8_t i=0;i<16;++i)assert(role(all,i)==NEUTRAL);
  all=assign(all,7,UNASSIGNED);
  assert(role(all,7)==UNASSIGNED&&role(all,6)==NEUTRAL&&role(all,8)==NEUTRAL);
  std::cout<<"PASS WLED 8B: 16-segment packing, legacy preservation, "
              "home/away/neutral GOAL/PP eligibility and color policy\n";
}
