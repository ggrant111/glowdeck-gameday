#pragma once
#include <stdint.h>

// Phase 7A — deliberately original, synthesized NHL goal horn (no WAV or
// media files). Pure C++11 policy: host-testable, deterministic, no heap.
namespace S3HornPolicy {
constexpr uint32_t kSampleRate=48000UL;
constexpr uint16_t kDurationMs=900;
constexpr uint32_t kFrames=kSampleRate*kDurationMs/1000U;
constexpr uint16_t kFrameChunk=256;
// ES8311 reference uses 70/100 on this Waveshare speaker. 50% was
// noticeably too quiet on physical hardware; keep it selectable, start at 70.
constexpr uint8_t kVolumePercent=70;
constexpr uint8_t kMinVolumePercent=30;
constexpr uint8_t kMaxVolumePercent=80;
constexpr bool validVolume(uint8_t percent){
  return percent>=kMinVolumePercent && percent<=kMaxVolumePercent;
}
// Match Waveshare's ES8311 voice_volume_set mapping: register 0x32.
constexpr uint8_t codecVolumeRegister(uint8_t percent){
  return percent==0?0:static_cast<uint8_t>((uint32_t(percent)*256UL/100UL)-1UL);
}
constexpr uint32_t kRetriggerMs=1200UL;
constexpr uint16_t kAttackFrames=kSampleRate*25UL/1000UL;
constexpr uint16_t kReleaseFrames=kSampleRate*130UL/1000UL;
static_assert(kFrames>kAttackFrames+kReleaseFrames,"Envelope must fit sound");
static_assert(kVolumePercent<=kMaxVolumePercent,"Default gain must not exceed safe cap");

// The original 220–330 Hz three-note mix was barely audible on the
// Waveshare's small speaker. Keep 220 Hz body but add strong 440/660/880 Hz
// harmonics, which project much better through a small driver. 48kHz PCM.
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
  const int32_t sum=(
      int32_t(triangle(frame*stepHz(220)))*18L+
      int32_t(triangle(frame*stepHz(440)))*30L+
      int32_t(triangle(frame*stepHz(660)))*32L+
      int32_t(triangle(frame*stepHz(880)))*20L)/100L;
  uint32_t env=1024;
  if(frame<kAttackFrames)env=(frame*1024UL)/kAttackFrames;
  else if(frame>kFrames-kReleaseFrames)
    env=((kFrames-frame)*1024UL)/kReleaseFrames;
  // 30k PCM ceiling gives useful headroom below int16 32767. The ES8311
  // codec gain is independently capped to 80%, configurable from 30–80%.
  return static_cast<int16_t>((int64_t(sum)*int64_t(env)*30000LL)/(32768LL*1024LL));
}
// Independent 1kHz diagnostic, at the currently selected ES8311 DAC gain.
// PCM peak <16k. Never triggers NHL events or WLED.
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
