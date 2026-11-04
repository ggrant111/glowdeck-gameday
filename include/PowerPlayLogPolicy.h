#pragma once
#include <stdint.h>

// A stable NHL gameId-based debouncer for power-play event logging.
// The NHL situation object can be temporarily absent or report 5v5 during
// a short live-feed refresh. Never turn a single stale sample into a false
// "Power play ended" followed immediately by a duplicate "started" event.
namespace PowerPlayLogPolicy {
struct Watch {
  uint32_t gameId = 0;
  uint8_t side = 0; // 0 none, 1 home, 2 away
  uint8_t noPowerPlaySamples = 0;
  uint32_t firstNoPowerPlayAtMs = 0;
  bool initialized = false;
};
struct Change {
  uint8_t started = 0;
  uint8_t ended = 0;
  bool firstObservation = false;
};
inline Change observe(Watch &watch, uint32_t gameId, bool dataKnown,
                      uint8_t observedSide, uint32_t nowMs) {
  Change c;
  if (!dataKnown || !gameId || observedSide > 2) return c;
  if (watch.gameId != gameId) watch = Watch();
  watch.gameId = gameId;
  if (!watch.initialized) {
    watch.initialized = true;
    watch.side = observedSide;
    c.started = observedSide;
    c.firstObservation = true;
    return c;
  }
  if (observedSide == watch.side) {
    watch.noPowerPlaySamples = 0;
    watch.firstNoPowerPlayAtMs = 0;
    return c;
  }
  if (observedSide == 0) {
    if (watch.side != 0) {
      if (!watch.noPowerPlaySamples)
        watch.firstNoPowerPlayAtMs = nowMs;
      if (watch.noPowerPlaySamples < 255)
        ++watch.noPowerPlaySamples;
      // Two distinct samples over >=10s: don't treat duplicated reads of
      // the same fallback snapshot as independent end confirmations.
      if (watch.noPowerPlaySamples >= 2 &&
          uint32_t(nowMs - watch.firstNoPowerPlayAtMs) >= 10000UL) {
        c.ended = watch.side;
        watch.side = 0;
        watch.noPowerPlaySamples = 0;
        watch.firstNoPowerPlayAtMs = 0;
      }
    }
    return c;
  }
  // An explicitly reported switch to the opposing PP team is a transition.
  if (watch.side) c.ended = watch.side;
  watch.side = observedSide;
  watch.noPowerPlaySamples = 0;
  watch.firstNoPowerPlayAtMs = 0;
  c.started = observedSide;
  return c;
}
} // namespace PowerPlayLogPolicy
