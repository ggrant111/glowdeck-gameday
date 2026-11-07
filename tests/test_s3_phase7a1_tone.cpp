#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include "S3HornPolicy.h"
using namespace S3HornPolicy;
int main(){
  static_assert(kVolumePercent==50,"Hardware codec must remain capped at 50%");
  static_assert(kDiagnosticHz==1000,"Diagnostic test must use 1kHz");
  static_assert(kSampleRate==48000UL,"I2S sample rate unchanged");
  static_assert(kTestDurationMs==1200,"Test tone is short");
  static_assert(kTestFrames==57600UL,"1.2 seconds at 48 kHz");
  assert(diagnosticSample(0)==0);
  assert(diagnosticSample(kTestFrames)==0);
  assert(diagnosticSample(kTestFrames+100)==0);
  int32_t peak=0;
  uint32_t nonzero=0;
  int64_t power=0;
  // Verify strong mid-frequency content and amplitude bounds, even through
  // the first attack and last release frame.
  for(uint32_t f=0;f<kTestFrames;++f){
    int32_t s=diagnosticSample(f);
    int32_t abs=s<0?-s:s;
    if(abs>peak)peak=abs;
    if(abs>0)++nonzero;
    power+=int64_t(s)*s;
    assert(abs<=15000);
  }
  assert(peak>14000);
  assert(nonzero>kTestFrames*9/10);
  assert(power/kTestFrames>10000000);
  // A silent or zeroed test waveform must never pass the confidence gate.
  for(uint32_t f=kTestFrames;f<kTestFrames+250;++f)
    assert(diagnosticSample(f)==0);
  std::cout<<"PASS S3 7A.1: audible 1kHz/50% PCM, bounded 1.2s, no clipping\n";
  return 0;
}
