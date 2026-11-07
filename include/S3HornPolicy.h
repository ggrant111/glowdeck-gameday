#pragma once
#include <stdint.h>

// Phase 7A — deliberately original, synthesized NHL goal horn (no WAV or
// media files). Pure C++11 policy: host-testable, deterministic, no heap.
namespace S3HornPolicy {
constexpr uint32_t kSampleRate=48000UL;
constexpr uint16_t kDurationMs=900;
constexpr uint32_t kFrames=kSampleRate*kDurationMs/1000U;
constexpr uint16_t kFrameChunk=256;
constexpr uint8_t kVolumePercent=50;
constexpr uint32_t kRetriggerMs=1200UL;
constexpr uint16_t kAttackFrames=kSampleRate*25UL/1000UL;
constexpr uint16_t kReleaseFrames=kSampleRate*130UL/1000UL;
static_assert(kFrames>kAttackFrames+kReleaseFrames,"Envelope must fit sound");
static_assert(kVolumePercent<=50,"Phase 7A preview must not exceed 50%");

// Three independent triangle oscillators: original broad horn voicing at
// 220, 277 and 330 Hz. All output is bounded to 16-bit stereo at 48k.
inline int16_t triangle(uint32_t phase) {
  const uint16_t p=static_cast<uint16_t>(phase>>16);
  const int32_t wave=p<32768 ? (int32_t(p)*2-32767) :
                                   (98303-int32_t(p)*2);
  return static_cast<int16_t>(wave);
}
inline uint32_t stepHz(uint32_t hertz) {
  return static_cast<uint32_t>((uint64_t(hertz)<<32)/kSampleRate);
}
inline int16_t sample(uint32_t frame) {
  if(frame>=kFrames)return 0;
  const uint32_t p0=frame*stepHz(220);
  const uint32_t p1=frame*stepHz(277);
  const uint32_t p2=frame*stepHz(330);
  const int32_t sum=int32_t(triangle(p0))/3+
                    int32_t(triangle(p1))/3+
                    int32_t(triangle(p2))/3;
  uint32_t env=1024;
  if(frame<kAttackFrames)env=(frame*1024UL)/kAttackFrames;
  else if(frame>kFrames-kReleaseFrames)
    env=((kFrames-frame)*1024UL)/kReleaseFrames;
  // Peak 10,000 PCM units, well below full scale. Hardware ES8311
  // volume is separately set to 50%, and the amplifier is off at idle.
  return static_cast<int16_t>((int64_t(sum)*int64_t(env)*10000LL)/(32768LL*1024LL));
}
// Audible high-mid diagnostic for small enclosure speakers: 1 kHz at
// fixed 50% ES8311 DAC gain (PCM peak <16k). Never triggers NHL/WLED.
constexpr uint16_t kTestDurationMs=1200;
constexpr uint32_t kTestFrames=kSampleRate*kTestDurationMs/1000UL;
constexpr uint16_t kDiagnosticHz=1000;
constexpr uint16_t kTestReleaseFrames=kSampleRate*100UL/1000UL;
inline int16_t diagnosticSample(uint32_t frame) {
  if(frame>=kTestFrames)return 0;
  uint32_t env=1024;
  if(frame<kAttackFrames)env=frame*1024UL/kAttackFrames;
  else if(frame>kTestFrames-kTestReleaseFrames)
    env=(kTestFrames-frame)*1024UL/kTestReleaseFrames;
  // 1k triangle has a strong speaker-audible fundamental. Fade on/off
  // avoids the clicks that a sustained square-wave could produce.
  return static_cast<int16_t>(int64_t(triangle(frame*stepHz(kDiagnosticHz)))*
                              int64_t(env)*15000LL/(32768LL*1024LL));
}
inline bool canPlay(bool ready,bool playing,uint32_t now,uint32_t last,
                    bool everPlayed) {
  return ready && !playing &&
         (!everPlayed || uint32_t(now-last)>=kRetriggerMs);
}
} // namespace S3HornPolicy
