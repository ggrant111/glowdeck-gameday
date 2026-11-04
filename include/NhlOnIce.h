#pragma once
#include <stdint.h>
#include <string.h>

// NHL on-ice status. A power play is separately validated by official
// penalties: six attackers and no goalie means EMPTY NET, not POWER PLAY.
// Side codes: 0 none/unknown, 1 home, 2 away, 3 both.
namespace NhlOnIce {
inline uint8_t emptyNetSide(int awaySkaters, int homeSkaters,
                            bool awayPulled, bool homePulled) {
  const bool away = awayPulled || awaySkaters == 6;
  const bool home = homePulled || homeSkaters == 6;
  return (home ? 1U : 0U) | (away ? 2U : 0U);
}
inline uint8_t fromSituationCode(const char *code) {
  if (!code || strlen(code) != 4) return 0;
  if ((code[0] != '0' && code[0] != '1') ||
      (code[3] != '0' && code[3] != '1')) return 0;
  for (int i=1;i<3;++i) if(code[i]<'0'||code[i]>'6')return 0;
  return (code[3] == '0' ? 1U : 0U) |
         (code[0] == '0' ? 2U : 0U);
}
inline bool teamIsPowerPlay(uint8_t verifiedSide, uint8_t side) {
  return verifiedSide == side;
}
inline bool teamHasEmptyNet(uint8_t netSide, uint8_t side) {
  return (netSide & side) != 0;
}
} // namespace NhlOnIce
