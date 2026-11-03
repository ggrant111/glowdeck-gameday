#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "TeamGradient.h"
int main() {
  using namespace TeamGradient;
  static_assert(kWidth == 320 && kHeight == 240, "CYD rotation must be 320x240");
  static_assert(kWidth % kStep == 0, "No strip may overflow the display");
  assert(teamMix(0) == 0 && teamMix(64) == 0);
  assert(teamMix(256) == 255 && teamMix(319) == 255);
  assert(teamMix(160) >= 126 && teamMix(160) <= 129);
  assert(shadePercent(0) == 73 && shadePercent(160) == 29);
  assert(shadePercent(319) >= 72);
  const uint32_t dallas=0x006847, washington=0xC8102E;
  const uint32_t left=colorAt(dallas,washington,0);
  const uint32_t right=colorAt(dallas,washington,319);
  const uint32_t center=colorAt(dallas,washington,160);
  assert(((left>>8)&255) > ((right>>8)&255)); // green -> red
  assert(((right>>16)&255) > ((left>>16)&255));
  assert(((center>>16)&255) < ((right>>16)&255)); // muted center
  assert(((center>>8)&255) < ((left>>8)&255));
  assert(colorAt(dallas,washington,320)==colorAt(dallas,washington,319));
  assert(rgb565(0xFF0000) == 0xF800);
  assert(rgb565(0x00FF00) == 0x07E0);
  assert(rgb565(0x0000FF) == 0x001F);
  assert(rgb565(0xFFFFFF) == 0xFFFF);
  puts("PASS: 320x240 heap-free NHL team gradient, color fade and RGB565");
}
