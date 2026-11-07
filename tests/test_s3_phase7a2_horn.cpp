#include <cassert>
#include <cstdint>
#include <iostream>
#include "S3HornPolicy.h"
using namespace S3HornPolicy;

int main(){
  static_assert(kVolumePercent==70,"Default follows Waveshare ES8311 example");
  static_assert(kMinVolumePercent==30 && kMaxVolumePercent==80,
                "S3 gain is bounded, speaker-safe and configurable");
  static_assert(codecVolumeRegister(50)==0x7F,"Old 50% remains selectable");
  static_assert(codecVolumeRegister(70)==0xB2,"70% matches official formula");
  static_assert(codecVolumeRegister(80)==0xCB,"Max 80% gain register");
  assert(validVolume(30) && validVolume(50) &&
         validVolume(70) && validVolume(80));
  assert(!validVolume(0) && !validVolume(29) && !validVolume(81));
  for(uint8_t p=kMinVolumePercent;p<=kMaxVolumePercent;++p){
    assert(codecVolumeRegister(p)>=codecVolumeRegister(kMinVolumePercent));
    assert(codecVolumeRegister(p)<=codecVolumeRegister(kMaxVolumePercent));
  }
  assert(sample(0)==0 && sample(kFrames)==0);
  int32_t maxAbs=0;
  int64_t sumSquares=0;
  for(uint32_t frame=0;frame<kFrames;++frame){
    const int32_t v=sample(frame);
    const int32_t mag=v<0?-v:v;
    assert(mag<=30000 && mag<32767);
    if(mag>maxAbs)maxAbs=mag;
    sumSquares+=int64_t(v)*v;
  }
  // 30 kPCM with 440/660/880 Hz upper harmonics is audible on small
  // enclosure speakers while preserving 220 Hz body and leaving headroom.
  assert(maxAbs>22000);
  assert(sumSquares/kFrames>30000000);
  assert(sample(kFrames-1)==0);
  assert(diagnosticSample(0)==0 && diagnosticSample(kTestFrames)==0);
  std::cout<<"PASS S3 7A.2: harmonic-rich horn, 30k PCM ceiling, "
              "user-selectable 30–80% codec gain and safe legacy 50%\n";
}
