#include <assert.h>
#include <stdio.h>
#include "StandingsCarousel.h"
using namespace StandingsCarousel;
struct Team {
  Group division;
  uint8_t conferenceRank, divisionRank;
};
int main() {
  const Team t[] = {
    {Group::ATLANTIC, 9, 5},
    {Group::METROPOLITAN, 1, 1},
    {Group::ATLANTIC, 2, 1},
    {Group::PACIFIC, 16, 8},
    {Group::CENTRAL, 1, 1},
    {Group::PACIFIC, 2, 1},
    {Group::METROPOLITAN, 16, 8}
  };
  assert(kPageCount == 8);
  assert(page(0).group == Group::EAST && page(0).offset == 0);
  assert(page(1).group == Group::EAST && page(1).offset == 8);
  assert(page(2).group == Group::WEST && page(2).offset == 0);
  assert(page(3).group == Group::WEST && page(3).offset == 8);
  assert(page(4).group == Group::ATLANTIC);
  assert(page(5).group == Group::METROPOLITAN);
  assert(page(6).group == Group::CENTRAL);
  assert(page(7).group == Group::PACIFIC);
  assert(groupFor("E", "A") == Group::ATLANTIC);
  assert(groupFor("W", "P") == Group::PACIFIC);
  assert(groupFor("E", "M") == Group::METROPOLITAN);
  assert(groupFor("W", "C") == Group::CENTRAL);
  assert(matches(Group::EAST, Group::ATLANTIC));
  assert(!matches(Group::WEST, Group::ATLANTIC));
  uint8_t out[32];
  uint8_t n=collectAndSort(t, 7, page(0), out);
  assert(n == 4 && out[0] == 1 && out[1] == 2 && out[2] == 0 && out[3] == 6);
  n=collectAndSort(t, 7, page(4), out);
  assert(n == 2 && out[0] == 2 && out[1] == 0);
  n=collectAndSort(t, 7, page(2), out);
  assert(n == 3 && out[0] == 4 && out[1] == 5 && out[2] == 3);
  assert(totalSlides(11, true) == 19);
  assert(totalSlides(11, false) == 11);
  assert(totalSlides(0, true) == 8);
  assert(advance(0,19,-1) == 18);
  assert(advance(18,19,1) == 0);
  puts("PASS: ranked conference/division standings pages and carousel wrapping");
}
