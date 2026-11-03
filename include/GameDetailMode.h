#pragma once
#include <string.h>

namespace GameDetailMode {
inline bool isPregame(const char *state) {
  return state && (!strcmp(state, "FUT") || !strcmp(state, "PRE"));
}
inline bool needsModeSwitch(bool detailLoaded, bool fetchedPregame,
                            const char *currentGameState) {
  return detailLoaded && fetchedPregame != isPregame(currentGameState);
}
} // namespace GameDetailMode
