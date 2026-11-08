#include <assert.h>
#include <stdio.h>
#include "NhlPowerPlayEvidence.h"
using namespace NhlPowerPlayEvidence;
int main() {
  assert(ppLabel("PP") && ppLabel("PP 5-on-4"));
  assert(!ppLabel("SH") && !ppLabel("EXTRA ATTACKER"));
  assert(confirmedSide(true,false)==1);
  assert(confirmedSide(false,true)==2);
  assert(!confirmedSide(false,false));
  assert(!confirmedSide(true,true));
  assert(skaterAdvantage("1451")==1); // VAN PP
  assert(skaterAdvantage("1541")==2); // ANA PP
  assert(!skaterAdvantage("1551")); // 5 on 5
  assert(!skaterAdvantage("1560")); // Empty-net 6-on-5, home goalie pulled
  assert(!skaterAdvantage("0651")); // Away goalie pulled
  assert(!skaterAdvantage("1460")); // Goalie absent
  assert(confirmedSide(true,false,"1451")==1);
  assert(!confirmedSide(true,false,"1551"));
  assert(!confirmedSide(true,false,"1560"));
  assert(!confirmedSide(false,true,"1451"));
  // Manpower is only a candidate and is subject to penalty validation.
  assert(manpowerCandidate(4,5,false,false)==1);
  assert(manpowerCandidate(5,4,false,false)==2);
  assert(manpowerCandidate(4,4,false,false)==0);
  assert(manpowerCandidate(5,6,false,true)==0);
  assert(manpowerCandidate(5,6,false,false)==0);
  assert(manpowerCandidate(4,5,true,false)==0);
  assert(isPenaltySkaterReduction("MIN",2));
  assert(!isPenaltySkaterReduction("MIS",10));
  assert(!isPenaltySkaterReduction("GAM",10));
  puts("PASS: labeled PP + both-goalies guard, reject empty net and 5v5");
}
