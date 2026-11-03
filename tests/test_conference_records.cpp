#include <assert.h>
#include <stdio.h>
#include "ConferenceRecords.h"

int main() {
  using namespace ConferenceRecords;
  assert(forTeam("COL") == Conference::WEST);
  assert(forTeam("CBJ") == Conference::EAST);
  assert(forTeam("UTA") == Conference::WEST);
  assert(forTeam("NJD") == Conference::EAST);
  assert(forTeam("SJS") == Conference::WEST);
  assert(forTeam("ZZZ") == Conference::UNKNOWN);
  Record r;
  add(r, "COL", "CBJ", true, true, 3, 2, "REG"); // East opponent: excluded
  add(r, "COL", "UTA", true, true, 2, 3, "OT"); // Western OT loss
  add(r, "COL", "STL", true, true, 4, 2, "REG"); // Western regulation win
  add(r, "COL", "VAN", true, true, 1, 4, "REG"); // Western regulation loss
  add(r, "COL", "EDM", true, true, 3, 2, "SO"); // Western shootout win
  add(r, "COL", "CGY", true, false, 0, 0, "REG"); // Future game: excluded
  add(r, "COL", "SJS", false, true, 4, 0, "REG"); // Playoff game: excluded
  assert(r.games == 4 && r.wins == 2 && r.losses == 1 && r.otLosses == 1);
  Record e;
  add(e, "CBJ", "BOS", true, true, 2, 3, "SO");
  add(e, "CBJ", "COL", true, true, 1, 3, "REG");
  assert(e.games == 1 && e.otLosses == 1);
  puts("PASS: East/West teams, vs-conference W-L-OT and excluded matchups");
  return 0;
}
