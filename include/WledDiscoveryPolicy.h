#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Pure, host-testable contract shared by CYD and Waveshare S3.
// WLED 8A increases the existing four saved controller slots to eight
// without changing indices or keys 0..3. Discovery never auto-enables lights.
namespace WledDiscoveryPolicy {
constexpr uint8_t kMaxDevices=8;
constexpr uint8_t kMaxCandidates=12;
constexpr uint8_t kDefaultEvents=15;
constexpr uint32_t kScanCooldownMs=15000UL;
constexpr uint32_t kManualProbeTimeoutMs=800UL;
constexpr uint32_t kScanProbeTimeoutMs=650UL;

inline bool validSlot(int slot){return slot>=0 && slot<kMaxDevices;}
inline bool validEvents(int events){return events>=0 && events<=15;}
// Only attempt automatic HTTP validation on same-subnet IPv4 addresses;
// never probe arbitrary external addresses as a result of mDNS data.
inline bool sameSubnet(uint32_t target,uint32_t device,uint32_t mask){
  return target!=0 && target!=0xFFFFFFFFUL && mask!=0 &&
         ((target&mask)==(device&mask));
}
inline bool validInfo(const char *brand,const char *version,
                      uint32_t leds){
  if(!version || !version[0] || leds==0 || leds>65535UL)return false;
  if(!brand || !brand[0])return true; // older WLED did not expose "brand"
  return strcmp(brand,"WLED")==0;
}
inline int freeSlot(const bool *occupied,size_t n){
  for(size_t i=0;i<n && i<kMaxDevices;++i)if(!occupied[i])return int(i);
  return -1;
}
inline bool duplicateIPv4(const char *candidate,const char* const *known,size_t n){
  if(!candidate||!candidate[0])return false;
  for(size_t i=0;i<n;++i){
    const char *x=known[i];
    if(!x)continue;
    size_t j=0;
    while(candidate[j]&&x[j]&&candidate[j]==x[j])++j;
    if(candidate[j]=='\0' && x[j]=='\0')return true;
  }
  return false;
}
} // WledDiscoveryPolicy
