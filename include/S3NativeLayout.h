#pragma once
#include <stdint.h>

// Native landscape hit targets shared by the Waveshare S3 renderer and tests.
// Pixel coordinates are in the ST7796 physical 480x320 orientation. This
// header has no Arduino dependencies and is not included in the CYD build.
namespace S3NativeLayout {
constexpr int16_t kWidth = 480;
constexpr int16_t kHeight = 320;
struct Rect {
  int16_t x, y, w, h;
  bool contains(int16_t px, int16_t py) const {
    return px >= x && py >= y && px < x + w && py < y + h;
  }
};
constexpr Rect kFollow = {374, 3, 48, 42};
constexpr Rect kMenu = {428, 3, 49, 42};
constexpr Rect kPrev = {12, 275, 115, 41};
constexpr Rect kGames = {172, 275, 136, 41};
constexpr Rect kNext = {353, 275, 115, 41};
constexpr int16_t kSummaryFirstY = 56;
constexpr int16_t kSummaryStride = 54;
constexpr int16_t kSummaryHeight = 49;
constexpr uint8_t kSummaryRows = 4;
inline Rect summaryRow(uint8_t n) {
  return {12, static_cast<int16_t>(kSummaryFirstY + n * kSummaryStride),
          456, kSummaryHeight};
}
inline Rect summaryStar(uint8_t n) {
  return {420, static_cast<int16_t>(kSummaryFirstY + n * kSummaryStride),
          48, kSummaryHeight};
}
enum class Action : uint8_t {
  NONE, MENU, FOLLOW, PREVIOUS, NEXT, GAMES, DETAILS,
  SUMMARY_DETAILS, SUMMARY_FOLLOW
};
struct Hit {
  Action action;
  uint8_t row;
};
inline Hit hitTest(int16_t x, int16_t y, bool summary) {
  if (x < 0 || y < 0 || x >= kWidth || y >= kHeight)
    return {Action::NONE, 0};
  if (kMenu.contains(x, y)) return {Action::MENU, 0};
  if (!summary && kFollow.contains(x, y)) return {Action::FOLLOW, 0};
  if (kPrev.contains(x, y)) return {Action::PREVIOUS, 0};
  if (kNext.contains(x, y)) return {Action::NEXT, 0};
  if (kGames.contains(x, y)) return {Action::GAMES, 0};
  if (summary) {
    for (uint8_t row = 0; row < kSummaryRows; ++row) {
      if (summaryStar(row).contains(x, y))
        return {Action::SUMMARY_FOLLOW, row};
      if (summaryRow(row).contains(x, y))
        return {Action::SUMMARY_DETAILS, row};
    }
  } else if (y >= 55 && y < 269) {
    return {Action::DETAILS, 0};
  }
  return {Action::NONE, 0};
}
inline bool carouselSwipe(int dx, int dy) {
  const int magnitudeX = dx < 0 ? -dx : dx;
  const int magnitudeY = dy < 0 ? -dy : dy;
  return magnitudeX >= 65 && magnitudeY < 95;
}
} // namespace S3NativeLayout
