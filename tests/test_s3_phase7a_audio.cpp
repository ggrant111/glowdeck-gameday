#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include "S3HornPolicy.h"

using namespace S3HornPolicy;
int main(){
  static_assert(kVolumePercent==70,"Improved S3 default matches Waveshare 70% reference gain");
  static_assert(kMaxVolumePercent==80,"Physical speaker cap is 80%");
  static_assert(kSampleRate==48000UL,"Match ES8311/I2S 48kHz");
  static_assert(kDurationMs==900,"User-preview duration should be bounded");
  static_assert(kFrames==43200UL,"900ms @ 48kHz");
  static_assert(kFrameChunk<=512,"Avoid large DMA task stack");
  assert(sample(0)==0);
  assert(sample(kFrames)==0);
  assert(sample(kFrames+1000)==0);
  assert(triangle(0)<-32000);
  assert(triangle(0x80000000U)>32000);
  assert(canPlay(true,false,100,0,false));
  assert(!canPlay(false,false,100,0,false));
  assert(!canPlay(true,true,100,0,false));
  assert(!canPlay(true,false,1250,100,true));
  assert(canPlay(true,false,1300,100,true));
  assert(canPlay(true,false,500U,0xFFFFFF00U,true)==false);
  assert(canPlay(true,false,1500U,0xFFFFFF00U,true));

  int32_t peak=0;
  int64_t absTotal=0;
  uint32_t nonzero=0;
  int32_t tail=0;
  for(uint32_t f=0;f<kFrames;++f){
    const int32_t s=sample(f);
    const int32_t absolute=s<0?-s:s;
    if(absolute>peak)peak=absolute;
    if(absolute){++nonzero;absTotal+=absolute;}
    if(f>kFrames-kReleaseFrames && absolute>tail)tail=absolute;
    // Stronger 30k peak stays below int16 32767 and does not clip.
    assert(absolute<=30000);
  }
  assert(nonzero>kFrames*8/10);
  assert(peak>22000 && peak<=30000);
  assert(absTotal>100000000);
  // Check a full stream's L/R sample mapping stays byte-budget bounded.
  assert(kFrameChunk*2u*sizeof(int16_t)<=2048u);
  assert(kFrames*4u==172800u);
  std::cout<<"PASS S3 Phase 7A horn policy: safe default gain, brighter original PCM, "
              "48kHz sample bounds, 900ms duration, retrigger wraparound\n";
}
