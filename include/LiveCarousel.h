#pragma once
#include <stdint.h>
#include <string.h>
#include "DisplayEventPolicy.h"

// Zero-heap NHL carousel policy for ESP32-2432S028R.
// During an active NHL slate: detailed LIVE/CRIT games first, followed by
// compact four-game pages for FUT/PRE/FINAL/OFF/other not-live matchups.
// When no games are live, preserve every original game and standings page.
namespace LiveCarousel {
constexpr uint8_t kMaxGames = 20;
constexpr uint8_t kMaxPages = 28; // 20 games plus 8 standings
constexpr uint8_t kRows = 4;
constexpr uint8_t kStandingsPages = 8;
enum class Kind : uint8_t { GAME, SUMMARY, STANDINGS, NONE };
struct Page {
  Kind kind = Kind::NONE;
  uint8_t index = 0; // GAME: game index, STANDINGS: page index
  uint8_t count = 0; // number of rows on SUMMARY
  uint8_t rows[kRows] = {};
};
struct Plan {
  Page pages[kMaxPages] = {};
  uint8_t count = 0;
  uint8_t liveCount = 0;
  uint8_t otherCount = 0;
  uint8_t summaryPages = 0;
  bool liveMode = false;
  Page at(uint8_t slide) const {
    return slide < count ? pages[slide] : Page();
  }
};
inline bool isLive(const char *state) {
  return state && (!strcmp(state,"LIVE") || !strcmp(state,"CRIT"));
}
template <typename Game>
inline Plan build(const Game *games, uint8_t gameCount, bool showStandings,
                  uint32_t favoriteTeams=0) {
  Plan plan;
  if (gameCount > kMaxGames) gameCount = kMaxGames;
  for (uint8_t i=0; i<gameCount; ++i)
    if (isLive(games[i].state.c_str())) ++plan.liveCount;
  plan.liveMode = plan.liveCount > 0;
  if (!plan.liveMode) {
    for (uint8_t i=0; i<gameCount; ++i) {
      Page &p = plan.pages[plan.count++];
      p.kind = Kind::GAME;
      p.index = i;
    }
    if (showStandings)
      for (uint8_t i=0; i<kStandingsPages; ++i) {
        Page &p = plan.pages[plan.count++];
        p.kind = Kind::STANDINGS;
        p.index = i;
      }
    return plan;
  }
  // Favorite live matchups appear first but other live games continue rotating.
  // Favorites are visual-only: they never arm WLED or rear RGB outputs.
  for (uint8_t tier=0; tier<2; ++tier) {
    for (uint8_t i=0; i<gameCount; ++i) {
      if (!isLive(games[i].state.c_str())) continue;
      const bool fav=DisplayEventPolicy::favoriteMatch(
          favoriteTeams,games[i].away.c_str(),games[i].home.c_str());
      if (fav != (tier==0)) continue;
      Page &p=plan.pages[plan.count++];
      p.kind=Kind::GAME;
      p.index=i;
    }
  }
  // Append one (or more) 4-game summaries. All remaining FUT/PRE/OFF/FINAL
  // games are displayed, retaining the NHL's date-feed game order.
  for (uint8_t i=0; i<gameCount; ++i) {
    if (isLive(games[i].state.c_str())) continue;
    if (plan.count == plan.liveCount ||
        plan.pages[plan.count-1].count == kRows) {
      Page &p = plan.pages[plan.count++];
      p.kind = Kind::SUMMARY;
      p.index = plan.summaryPages++;
    }
    Page &p = plan.pages[plan.count-1];
    p.rows[p.count++] = i;
    ++plan.otherCount;
  }
  return plan;
}
inline uint8_t advance(uint8_t slide, const Plan &plan, int direction) {
  if (!plan.count) return 0;
  return direction < 0 ? (uint8_t)((slide + plan.count-1)%plan.count)
                       : (uint8_t)((slide+1)%plan.count);
}
template <typename Game>
inline uint8_t locateGame(const Plan &plan, const Game *games,
                          uint32_t gameId) {
  if (!gameId) return 255;
  for (uint8_t i=0; i<plan.count; ++i) {
    const Page &p=plan.pages[i];
    if (p.kind==Kind::GAME && games[p.index].id==gameId) return i;
  }
  for (uint8_t i=0; i<plan.count; ++i) {
    const Page &p=plan.pages[i];
    if (p.kind!=Kind::SUMMARY) continue;
    for (uint8_t j=0; j<p.count; ++j)
      if (games[p.rows[j]].id==gameId) return i;
  }
  return 255;
}
template <typename Game>
inline uint8_t restore(const Plan &plan, const Game *games,
                       Kind oldKind, uint32_t oldGameId,
                       uint8_t oldStandingsIndex, uint8_t fallbackGameIndex) {
  if (!plan.count) return 0;
  if (oldKind==Kind::STANDINGS)
    for (uint8_t i=0; i<plan.count; ++i)
      if (plan.pages[i].kind==Kind::STANDINGS &&
          plan.pages[i].index==oldStandingsIndex) return i;
  const uint8_t found=locateGame(plan,games,oldGameId);
  if (found!=255) return found;
  if (!plan.liveMode && fallbackGameIndex<kMaxGames) {
    for (uint8_t i=0; i<plan.count; ++i)
      if (plan.pages[i].kind==Kind::GAME &&
          plan.pages[i].index==fallbackGameIndex) return i;
  }
  return 0; // When the live slate begins, prefer first live game.
}
} // namespace LiveCarousel
