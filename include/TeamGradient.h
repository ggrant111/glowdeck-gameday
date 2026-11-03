#pragma once
#include <stdint.h>

// Fixed-point, buffer-free 320x240 CYD backdrop. A club's primary RGB is
// retained on its side, with a soft crossfade and dark center for white scores.
// All calculations are local scalars: no images, sprites, heap, or PSRAM.
namespace TeamGradient {
constexpr uint16_t kWidth = 320;
constexpr uint16_t kHeight = 240;
constexpr uint8_t kStep = 4; // 80 solid TFT strips per full slide redraw

inline uint8_t channel(uint32_t rgb, uint8_t shift) {
  return static_cast<uint8_t>((rgb >> shift) & 255U);
}
inline uint8_t mix(uint8_t a, uint8_t b, uint8_t mix255) {
  return static_cast<uint8_t>((uint32_t(a) * (255U - mix255) +
                              uint32_t(b) * mix255 + 127U) / 255U);
}
inline uint8_t scale(uint8_t value, uint8_t percent) {
  return static_cast<uint8_t>((uint32_t(value) * percent + 50U) / 100U);
}
// 0..100% of second team's color. A narrow, eased blend maintains each
// team's identity across the majority of its half.
inline uint8_t teamMix(uint16_t x) {
  if (x <= 64) return 0;
  if (x >= 256) return 255;
  const uint32_t t = uint32_t(x - 64) * 255U / 192U;
  // smoothstep(t) = 3t*t - 2t*t*t, fixed-point 0..255.
  return static_cast<uint8_t>(
      (t * t * (765U - 2U * t) + 32512U) / 65025U);
}
// Approximately 73% color at the outer edges, 29% in the middle.
// The dark center protects score, VS, clock and thin white dividers.
inline uint8_t shadePercent(uint16_t x) {
  const uint16_t d = x > 160 ? x - 160 : 160 - x;
  return static_cast<uint8_t>(29U + (uint32_t(d) * 44U) / 160U);
}
inline uint32_t colorAt(uint32_t left, uint32_t right, uint16_t x) {
  if (x >= kWidth) x = kWidth - 1;
  const uint8_t mixAmount = teamMix(x);
  const uint8_t shade = shadePercent(x);
  const uint8_t r = scale(mix(channel(left, 16), channel(right, 16), mixAmount), shade);
  const uint8_t g = scale(mix(channel(left, 8), channel(right, 8), mixAmount), shade);
  const uint8_t b = scale(mix(channel(left, 0), channel(right, 0), mixAmount), shade);
  return (uint32_t(r) << 16) | (uint32_t(g) << 8) | b;
}
inline uint16_t rgb565(uint32_t rgb) {
  return static_cast<uint16_t>(((rgb >> 19) & 31U) << 11 |
                               ((rgb >> 10) & 63U) << 5 |
                               ((rgb >> 3) & 31U));
}
} // namespace TeamGradient
