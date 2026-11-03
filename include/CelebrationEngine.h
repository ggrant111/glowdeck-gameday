#pragma once
#include <stdint.h>
#include <string.h>

// Shared, allocation-free event semantics used by the CYD firmware and CI.
// All selected WLEDs receive the same effect ID, color, timing and settings.
namespace CelebrationEngine {
enum class Event : uint8_t {
  NONE = 0, GOAL = 1, POWER_PLAY = 2, GAME_START = 3,
  FINAL_WIN = 4, FINAL_LOSS = 5
};
enum : uint8_t { GOAL_FLAG = 1, PP_FLAG = 2, START_FLAG = 4, FINAL_FLAG = 8 };
constexpr uint8_t ALL_EVENTS = 15;
constexpr uint32_t kStartDurationMs = 4500UL;
constexpr uint32_t kFinalDurationMs = 7000UL;
constexpr uint8_t kMaxSegments = 16;

struct Effect {
  uint8_t fx, speed, intensity;
};

inline uint8_t flagFor(Event event) {
  switch (event) {
    case Event::GOAL: return GOAL_FLAG;
    case Event::POWER_PLAY: return PP_FLAG;
    case Event::GAME_START: return START_FLAG;
    case Event::FINAL_WIN:
    case Event::FINAL_LOSS: return FINAL_FLAG;
    default: return 0;
  }
}
inline bool eligible(bool enabled, uint8_t mask, Event event) {
  return enabled && (mask & flagFor(event)) != 0;
}
inline bool temporary(Event event) {
  return event != Event::NONE && event != Event::POWER_PLAY;
}
inline uint32_t duration(Event event, uint8_t goalSeconds) {
  if (event == Event::GOAL) return uint32_t(goalSeconds) * 1000UL;
  if (event == Event::GAME_START) return kStartDurationMs;
  if (event == Event::FINAL_WIN || event == Event::FINAL_LOSS)
    return kFinalDurationMs;
  return 0; // PP lasts only while NHL reports a man advantage.
}

// Official WLED FX mode IDs: solid 0, Breathe 2, Wipe 3, Fade 12,
// Twinkle 17, Strobe 23, Chase 28.
// Style 0 = Strobe -> Chase, 1 = Strobe, 2 = Chase, 3 = Solid.
inline uint8_t phase(Event event, uint32_t elapsed, uint32_t fullDuration,
                     uint8_t goalStyle) {
  if (event != Event::GOAL || goalStyle != 0) return 0;
  return elapsed >= fullDuration / 3UL ? 1 : 0;
}
inline Effect effect(Event event, uint8_t stage, uint8_t goalStyle) {
  switch (event) {
    case Event::GOAL:
      if (goalStyle == 3) return {0, 128, 128};
      if (goalStyle == 2 || (goalStyle == 0 && stage == 1))
        return {28, 190, 165};
      return {23, 230, 185};
    case Event::POWER_PLAY: return {2, 115, 160};
    case Event::GAME_START: return {3, 170, 170};
    case Event::FINAL_WIN: return {17, 165, 195};
    case Event::FINAL_LOSS: return {12, 65, 105};
    default: return {0, 128, 128};
  }
}
inline bool isLive(const char *state) {
  return state && ((!strcmp(state, "LIVE")) || (!strcmp(state, "CRIT")));
}
inline bool isFinal(const char *state) {
  return state && ((!strcmp(state, "OFF")) || (!strcmp(state, "FINAL")));
}
inline bool isPregame(const char *state) {
  return state && ((!strcmp(state, "FUT")) || (!strcmp(state, "PRE")));
}
inline Event transition(bool hasBaseline, const char *oldState,
                        const char *newState, bool favoredWon) {
  if (!hasBaseline) return Event::NONE;
  if (isPregame(oldState) && isLive(newState)) return Event::GAME_START;
  if (isLive(oldState) && isFinal(newState))
    return favoredWon ? Event::FINAL_WIN : Event::FINAL_LOSS;
  return Event::NONE;
}
inline const char *label(Event event) {
  switch(event) {
    case Event::GOAL: return "GOAL";
    case Event::POWER_PLAY: return "POWER PLAY";
    case Event::GAME_START: return "GAME START";
    case Event::FINAL_WIN: return "FINAL WIN";
    case Event::FINAL_LOSS: return "FINAL LOSS";
    default: return "IDLE";
  }
}
} // namespace CelebrationEngine
