#include <assert.h>
#include <stdio.h>
#include "NhlOnIce.h"
using namespace NhlOnIce;
int main() {
  // Actual NHL ANA @ VAN, 2026-11-03.
  assert(fromSituationCode("1560")==1); // Vancouver net EMPTY
  assert(fromSituationCode("0651")==2); // Anaheim net EMPTY
  assert(fromSituationCode("1541")==0); // ANA 5v4 PP, both goalies in
  assert(fromSituationCode("1451")==0); // VAN 5v4 PP, both goalies in
  assert(fromSituationCode("1551")==0); // even strength
  assert(fromSituationCode("0550")==3); // both nets empty
  assert(!fromSituationCode("bad"));
  assert(!emptyNetSide(5,5,false,false));
  assert(emptyNetSide(5,6,false,false)==1);
  assert(emptyNetSide(6,5,false,false)==2);
  assert(emptyNetSide(5,5,false,true)==1);
  assert(emptyNetSide(5,5,true,false)==2);
  assert(emptyNetSide(6,6,false,false)==3);
  assert(teamIsPowerPlay(1,1) && !teamIsPowerPlay(1,2));
  assert(teamHasEmptyNet(3,1) && teamHasEmptyNet(3,2));
  puts("PASS: independent PP vs empty net status, home/away 6-on-5 codes");
}
