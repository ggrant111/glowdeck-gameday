#pragma once
#include <stdint.h>

// Display-only carousel hold. The NHL feed and all celebration eligibility
// continue polling while a page is held. Game IDs survive feed reordering.
namespace CarouselHoldPolicy {
struct State {
  bool paused = false;
  uint32_t gameId = 0;
  void reset() { paused = false; gameId = 0; }
  void toggle(uint32_t visibleGameId) {
    if (paused) { reset(); return; }
    if (visibleGameId) { paused = true; gameId = visibleGameId; }
  }
  template<typename Game>
  uint8_t index(const Game *games, uint8_t count) const {
    if (!paused || !gameId) return 255;
    for (uint8_t i = 0; i < count; ++i)
      if (games[i].id == gameId) return i;
    return 255;
  }
  void navigatedTo(uint32_t visibleGameId) {
    if (!paused) return;
    if (!visibleGameId) reset(); // Summary/standings navigation resumes cycle.
    else gameId = visibleGameId;
  }
};
} // namespace CarouselHoldPolicy
