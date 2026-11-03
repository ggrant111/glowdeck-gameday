#pragma once
#include <stdint.h>
#include <string.h>

// Small, allocation-free page description and rank sorting for the 320x240 CYD.
// A conference needs two eight-row slides; each division fits on one slide.
namespace StandingsCarousel {
constexpr uint8_t kPageCount = 8;
constexpr uint8_t kRowsPerPage = 8;

enum class Group : uint8_t {
  EAST = 1, WEST = 2, ATLANTIC = 3, METROPOLITAN = 4,
  CENTRAL = 5, PACIFIC = 6
};

struct Page {
  Group group;
  uint8_t offset;
  const char *title;
  const char *range;
};

inline Page page(uint8_t index) {
  switch (index % kPageCount) {
    case 0: return {Group::EAST, 0, "EAST CONFERENCE", "RANKS 1 - 8"};
    case 1: return {Group::EAST, 8, "EAST CONFERENCE", "RANKS 9 - 16"};
    case 2: return {Group::WEST, 0, "WEST CONFERENCE", "RANKS 1 - 8"};
    case 3: return {Group::WEST, 8, "WEST CONFERENCE", "RANKS 9 - 16"};
    case 4: return {Group::ATLANTIC, 0, "ATLANTIC DIVISION", "RANKS 1 - 8"};
    case 5: return {Group::METROPOLITAN, 0, "METRO DIVISION", "RANKS 1 - 8"};
    case 6: return {Group::CENTRAL, 0, "CENTRAL DIVISION", "RANKS 1 - 8"};
    default: return {Group::PACIFIC, 0, "PACIFIC DIVISION", "RANKS 1 - 8"};
  }
}

inline Group groupFor(const char *conference, const char *division) {
  if (division) {
    if (!strcmp(division, "A") || !strcmp(division, "ATL"))
      return Group::ATLANTIC;
    if (!strcmp(division, "M") || !strcmp(division, "MET"))
      return Group::METROPOLITAN;
    if (!strcmp(division, "C") || !strcmp(division, "CEN"))
      return Group::CENTRAL;
    if (!strcmp(division, "P") || !strcmp(division, "PAC"))
      return Group::PACIFIC;
  }
  if (conference && !strcmp(conference, "W")) return Group::WEST;
  return Group::EAST;
}

inline bool matches(Group pageGroup, Group teamDivision) {
  if (pageGroup == Group::EAST)
    return teamDivision == Group::ATLANTIC ||
           teamDivision == Group::METROPOLITAN;
  if (pageGroup == Group::WEST)
    return teamDivision == Group::CENTRAL ||
           teamDivision == Group::PACIFIC;
  return pageGroup == teamDivision;
}

inline bool isConference(Group group) {
  return group == Group::EAST || group == Group::WEST;
}

inline uint8_t rank(Group pageGroup, uint8_t conferenceRank,
                    uint8_t divisionRank) {
  return isConference(pageGroup) ? conferenceRank : divisionRank;
}

// Sort at most 32 eligible team indices by the NHL's official rank fields;
// deterministic fallback (source-index order) for missing/equal ranks.
template <typename Record>
inline uint8_t collectAndSort(const Record *teams, uint8_t count,
                              Page view, uint8_t *output, uint8_t maxRows = 32) {
  uint8_t n = 0;
  for (uint8_t i = 0; i < count && n < maxRows; ++i)
    if (matches(view.group, teams[i].division) &&
        rank(view.group, teams[i].conferenceSequence, teams[i].divisionSequence) > 0)
      output[n++] = i;
  for (uint8_t i = 1; i < n; ++i) {
    const uint8_t value = output[i];
    const uint8_t valueRank = rank(view.group,
              teams[value].conferenceSequence, teams[value].divisionSequence);
    uint8_t j = i;
    while (j && rank(view.group, teams[output[j-1]].conferenceSequence,
                       teams[output[j-1]].divisionSequence) > valueRank) {
      output[j] = output[j-1];
      --j;
    }
    output[j] = value;
  }
  return n;
}

inline uint8_t totalSlides(uint8_t gameCount, bool showStandings) {
  return (uint8_t)(gameCount + (showStandings ? kPageCount : 0));
}
inline uint8_t advance(uint8_t current, uint8_t count, int direction) {
  if (!count) return 0;
  if (direction < 0) return (uint8_t)((current + count - 1) % count);
  return (uint8_t)((current + 1) % count);
}
} // namespace StandingsCarousel
