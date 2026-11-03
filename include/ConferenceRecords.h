#pragma once
#include <stdint.h>
#include <string.h>

namespace ConferenceRecords {
enum class Conference : uint8_t { UNKNOWN = 0, EAST = 1, WEST = 2 };

// NHL alignment for the 2026-27 season. Update when the league realigns.
inline Conference forTeam(const char *abbr) {
  if (!abbr || strlen(abbr) != 3) return Conference::UNKNOWN;
  const char *const east[] = {
    "BOS", "BUF", "CAR", "CBJ", "DET", "FLA", "MTL", "NJD",
    "NYI", "NYR", "OTT", "PHI", "PIT", "TBL", "TOR", "WSH"
  };
  const char *const west[] = {
    "ANA", "CGY", "CHI", "COL", "DAL", "EDM", "LAK", "MIN",
    "NSH", "SEA", "SJS", "STL", "UTA", "VAN", "VGK", "WPG"
  };
  for (const char *code : east) if (strcmp(abbr, code) == 0) return Conference::EAST;
  for (const char *code : west) if (strcmp(abbr, code) == 0) return Conference::WEST;
  return Conference::UNKNOWN;
}

struct Record {
  uint16_t wins = 0, losses = 0, otLosses = 0;
  uint16_t games = 0;
};

// One row from the NHL club-schedule-season/{team}/{season} "games" array.
// Only regular-season finals against teams in the SAME conference count.
// An OT/SO loss is distinct from a regulation loss.
inline void add(Record &record, const char *club, const char *opponent,
                bool regularSeason, bool final, int goalsFor, int goalsAgainst,
                const char *lastPeriodType) {
  if (!regularSeason || !final || goalsFor < 0 || goalsAgainst < 0 ||
      goalsFor == goalsAgainst || !club || !opponent) return;
  const Conference own = forTeam(club);
  if (own == Conference::UNKNOWN || forTeam(opponent) != own) return;
  ++record.games;
  if (goalsFor > goalsAgainst) {
    ++record.wins;
  } else if (lastPeriodType &&
             (!strcmp(lastPeriodType, "OT") || !strcmp(lastPeriodType, "SO"))) {
    ++record.otLosses;
  } else {
    ++record.losses;
  }
}
} // namespace ConferenceRecords
