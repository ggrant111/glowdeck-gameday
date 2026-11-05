#pragma once
#include <stdint.h>

// Today's NHL slate is capped at 20 games in this firmware.  The user can
// follow the entire slate; an armed game is included automatically.
namespace GameFollowPolicy {
constexpr uint8_t kMaxDailyGames = 20;
enum class Result : uint8_t { ADDED, REMOVED, UNCHANGED, INVALID, FULL, ARMED };
// slot is the only NVS key that needs an update after a successful change.
// Return 255 for no persistent mutation (no flash wear on duplicate taps).
inline Result change(uint32_t *ids, uint8_t capacity, uint32_t id,
                     bool scheduled, uint32_t armedId, bool follow,
                     uint8_t &slot) {
  slot = 255;
  if (!id || !scheduled || !ids || !capacity) return Result::INVALID;
  if (id == armedId) return Result::ARMED;
  uint8_t found = capacity, empty = capacity;
  for (uint8_t i = 0; i < capacity; ++i) {
    if (ids[i] == id) found = i;
    if (ids[i] == 0 && empty == capacity) empty = i;
  }
  if (follow) {
    if (found != capacity) return Result::UNCHANGED;
    if (empty == capacity) return Result::FULL;
    ids[empty] = id;
    slot = empty;
    return Result::ADDED;
  }
  if (found == capacity) return Result::UNCHANGED;
  ids[found] = 0;
  slot = found;
  return Result::REMOVED;
}
inline bool headerStarTap(int startX, int startY, int endX, int endY) {
  return startX >= 122 && startX <= 176 && startY >= 0 && startY <= 40 &&
         endX >= 122 && endX <= 176 && endY >= 0 && endY <= 44 &&
         endX - startX < 24 && startX - endX < 24 &&
         endY - startY < 24 && startY - endY < 24;
}
inline bool listStarTap(int x) { return x >= 266 && x <= 319; }
} // namespace GameFollowPolicy
