#pragma once
#include <stdint.h>

// Display-only carousel hold. The NHL feed and all celebration eligibility
// continue polling while a page is held. Game IDs survive feed reordering.
namespace CarouselHoldPolicy {
struct State {
  bool paused = false;
  uint32_t gameId = 0;
  void reset() { paused = false; gameId = 0; }
  void pin(uint32_t visibleGameId) {
    if (!visibleGameId) return;
    paused = true;
    gameId = visibleGameId;
  }
  void toggle(uint32_t visibleGameId) {
    if (paused) { reset(); return; }
    pin(visibleGameId);
  }
  // A transient NHL API omission is not an explicit resume command.
  // Only user navigation, resume, or a confirmed new scoreboard date
  // releases the lock; the next valid feed can restore this game ID.
  void onFeedRefresh(bool sameScoreboardDate) {
    if (!sameScoreboardDate) reset();
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
