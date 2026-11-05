#pragma once
#include <stdint.h>
#include <string.h>

// Single palette used by NHL observations, rear RGB diagnostics, WLED,
// and the web dashboard. All RGB values are full-intensity 24-bit colors.
namespace TeamPalette {
struct TeamColor { const char* abbr; uint32_t rgb; };
static const TeamColor kTeams[] = {
  {"ANA", 0xF47A38}, {"BOS", 0xFFB81C}, {"BUF", 0x003087},
  {"CGY", 0xC8102E}, {"CAR", 0xCC0000}, {"CHI", 0xCF0A2C},
  {"COL", 0x6F263D}, {"CBJ", 0x002654}, {"DAL", 0x006847},
  {"DET", 0xCE1126}, {"EDM", 0xFF4C00}, {"FLA", 0xC8102E},
  {"LAK", 0xA2AAAD}, {"MIN", 0x154734}, {"MTL", 0xAF1E2D},
  {"NJD", 0xCE1126}, {"NSH", 0xFFB81C}, {"NYI", 0x00539B},
  {"NYR", 0x0038A8}, {"OTT", 0xC52032}, {"PHI", 0xF74902},
  {"PIT", 0xFCB514}, {"SEA", 0x99D9D9}, {"SJS", 0x006D75},
  {"STL", 0x002F87}, {"TBL", 0x002868}, {"TOR", 0x00205B},
  {"UTA", 0x73B7D3}, {"VAN", 0x00843D}, {"VGK", 0xB4975A},
  {"WSH", 0xC8102E}, {"WPG", 0x041E42}
};
static const uint8_t kCount = sizeof(kTeams) / sizeof(kTeams[0]);

inline bool find(const char* abbreviation, uint32_t &color) {
  if (!abbreviation) return false;
  for (uint8_t i = 0; i < kCount; ++i)
    if (strcmp(abbreviation, kTeams[i].abbr) == 0) {
      color = kTeams[i].rgb;
      return true;
    }
  return false;
}
inline uint32_t rgb(const char* abbreviation) {
  uint32_t color = 0;
  return find(abbreviation, color) ? color : 0x31837A;
}
inline bool diagnosticRgb(const char* channel, uint32_t &color) {
  if (!channel) return false;
  if (strcmp(channel, "red") == 0) { color = 0xFF0000; return true; }
  if (strcmp(channel, "green") == 0) { color = 0x00FF00; return true; }
  if (strcmp(channel, "blue") == 0) { color = 0x0000FF; return true; }
  return false;
}
} // namespace TeamPalette
