#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include "WledDiscoveryPolicy.h"
#include "GameEventLog.h"
using namespace WledDiscoveryPolicy;
int main(){
  static_assert(kMaxDevices==8,"Eight active WLED slots");
  static_assert(GameEventLog::kDevices==kMaxDevices,"Log results all eight");
  static_assert(kMaxCandidates>=kMaxDevices,"Scan fits saved devices");
  static_assert(kScanCooldownMs>=10000,"Rate limit scans");
  for(int i=0;i<8;++i)assert(validSlot(i));
  assert(!validSlot(-1)&&!validSlot(8));
  assert(validEvents(0)&&validEvents(15)&&!validEvents(16));
  bool taken[8]={true,true,true,true,false,false,false,false};
  assert(freeSlot(taken,8)==4);
  taken[4]=true;assert(freeSlot(taken,8)==5);
  for(int i=0;i<8;++i)taken[i]=true;
  assert(freeSlot(taken,8)==-1);
  const char *ips[8]={"192.168.1.71","192.168.1.72","","",
                       nullptr,nullptr,nullptr,nullptr};
  assert(duplicateIPv4("192.168.1.71",ips,8));
  assert(!duplicateIPv4("192.168.1.73",ips,8));
  assert(!duplicateIPv4("",ips,8));
  const uint32_t local=0xC0A80118,mask=0xFFFFFF00;
  assert(sameSubnet(0xC0A80147,local,mask));
  assert(!sameSubnet(0xC0A80247,local,mask));
  assert(!sameSubnet(0,local,mask));
  assert(validInfo("WLED","0.15.0",120));
  assert(validInfo("", "0.13.1",64));
  assert(!validInfo("notWLED","0.15.0",120));
  assert(!validInfo("WLED","",120));
  assert(!validInfo("WLED","0.14",0));
  GameEventLog::Buffer log;
  auto &e=log.add(GameEventLog::Type::GOAL,0,0,77,"BOS","UTA","UTA",1,2,3);
  for(uint8_t i=0;i<8;++i)e.wled[i]=GameEventLog::Wled::ACCEPTED;
  e.wledAcknowledgedMask=0xFF;
  for(uint8_t i=0;i<8;++i)
    assert(e.wled[i]==GameEventLog::Wled::ACCEPTED &&
           (e.wledAcknowledgedMask & (1U<<i)));
  std::cout<<"PASS WLED 8A: eight slots, migrated indices, IP dedupe, subnet "
              "validation, verified info and 8-device audit\n";
}
