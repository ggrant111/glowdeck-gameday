#include <assert.h>
#include <stdio.h>
#include "TeamPalette.h"

int main() {
  using namespace TeamPalette;
  assert(kCount == 32);
  uint32_t color = 0;
  assert(find("DAL", color) && color == 0x006847);
  assert(find("ANA", color) && color == 0xF47A38);
  assert(find("UTA", color) && color == 0x73B7D3);
  assert(rgb("DAL") != rgb("ANA"));
  assert(rgb("BOS") == rgb("NSH")); // Legitimate identical primary colors.
  assert(!find("ZZZ", color));
  assert(!find("", color));
  assert(!find(nullptr, color));
  assert(rgb("ZZZ") == 0x31837A); // Normal feed fallback, not test preview.
  assert(diagnosticRgb("red", color) && color == 0xFF0000);
  assert(diagnosticRgb("green", color) && color == 0x00FF00);
  assert(diagnosticRgb("blue", color) && color == 0x0000FF);
  assert(!diagnosticRgb("purple", color));
  puts("PASS NHL palette, team lookup and independent RGB diagnostics");
}
