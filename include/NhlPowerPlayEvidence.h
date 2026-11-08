#pragma once
#include <stdint.h>
#include <string.h>

// An NHL skater advantage is not automatically a POWER PLAY. It can be a
// pulled goalie, extra attacker on a delayed penalty, or special game state.
// /scoreboard/now.situationDescriptions[] explicitly carries PP labels.
// Absence of a labeled NHL PP is NOT a confirmed penalty/PP start.
namespace NhlPowerPlayEvidence {
inline bool ppLabel(const char *label) {
  if (!label) return false;
  return (!strcmp(label,"PP") || !strncmp(label,"PP ",3) ||
          !strcmp(label,"POWER PLAY") || !strcmp(label,"Power Play"));
}
// NHL PBP situationCode positions: away goalies, away skaters,
// home skaters, home goalies. "1560" = Vancouver six skaters, no goalie,
// NOT a power play. "1451" = 4 ANA vs 5 VAN, true manpower advantage.
inline bool goalieOnBothEnds(const char *code) {
  return code && strlen(code) == 4 && code[0]=='1' && code[3]=='1';
}
inline uint8_t skaterAdvantage(const char *code) {
  if (!goalieOnBothEnds(code)) return 0;
  const uint8_t away=code[1]-'0', home=code[2]-'0';
  if (away<3 || away>5 || home<3 || home>5) return 0;
  if (home>away) return 1; // home power play candidate
  if (away>home) return 2; // away power play candidate
  return 0;
}
// Some /score/{date} responses omit situationDescriptions during TV breaks,
// but retain official on-ice strengths. A 5-on-4 / 4-on-3 with both goalies
// present can be a *candidate* only; an actual start additionally requires
// the separately fetched NHL penalty ledger to confirm the same side.
// Never infer a PP from goalie-pulled 6-on-5 or a delayed extra attacker.
inline uint8_t manpowerCandidate(int away,int home,
                                 bool awayPulled,bool homePulled) {
  if(awayPulled||homePulled||away<3||away>5||home<3||home>5)
    return 0;
  return home>away?1:away>home?2:0;
}
// Require the NHL PP label in the live scoreboard. Team strength by itself
// can NEVER create an event. When an official PBP situation is available,
// it additionally must show both goalies present and match the team label.
inline uint8_t confirmedSide(bool homePP, bool awayPP,
                             const char *situationCode=nullptr) {
  const uint8_t labeled = homePP == awayPP ? 0 : (homePP ? 1 : 2);
  if (!labeled) return 0;
  if (situationCode && situationCode[0] &&
      skaterAdvantage(situationCode)!=labeled) return 0;
  return labeled;
}
inline bool isPenaltySkaterReduction(const char *typeCode,
                                      unsigned durationMinutes) {
  // Fighting majors, misconduct and penalty shots do not cause 5v4 by
  // themselves. NHL may separately issue an accompanying serving minor.
  if (!typeCode || !durationMinutes) return false;
  return !strcmp(typeCode,"MIN") || !strcmp(typeCode,"BEN") ||
         !strcmp(typeCode,"DBL") || !strcmp(typeCode,"MAJ");
}
} // namespace NhlPowerPlayEvidence
