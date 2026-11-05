#pragma once
#include <stdint.h>

// Pure timing math, shared by the ESP32 CYD firmware and native CI tests.
// No delay(), blocking network calls, or Arduino dependencies.
namespace RearLedPattern {
// Confirmed physical RGB channels on this project's CYD board revision.
// The green/blue GPIO assignments were reversed in <= v0.3.8.
// Keep these mappings separate from NHL 24-bit colors and WLED effects.
constexpr uint8_t kRedPin = 4;
constexpr uint8_t kGreenPin = 17;
constexpr uint8_t kBluePin = 16;
inline uint8_t flash(uint32_t elapsedMs, uint32_t durationMs) {
  if (elapsedMs >= durationMs || durationMs == 0) return 0;
  // Bright 150ms bursts alternating with 110ms gaps.
  if (elapsedMs % 260UL >= 150UL) return 0;
  const uint32_t remaining = durationMs - elapsedMs;
  // Fade out on the last 500ms rather than ending abruptly.
  return remaining >= 500UL ? 255 :
      (uint8_t)((remaining * 255UL) / 500UL);
}
inline uint8_t pulse(uint32_t elapsedMs) {
  // Gentle triangle wave at 1.8s/cycle, baseline off.
  const uint32_t phase = elapsedMs % 1800UL;
  const uint32_t ramp = phase <= 900UL ? phase : 1800UL - phase;
  return (uint8_t)((ramp * 210UL) / 900UL);
}
inline uint8_t invertedDuty(uint8_t color, uint8_t globalBrightness,
                            uint8_t intensity) {
  // RGB LED on ESP32-2432S028R is active LOW.
  const uint32_t value = uint32_t(color) * globalBrightness * intensity;
  return (uint8_t)(255UL - value / (255UL * 255UL));
}
} // namespace RearLedPattern
