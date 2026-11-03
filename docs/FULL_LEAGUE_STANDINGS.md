# GlowDeck GameDay v0.3.0 — Full NHL conference and division rankings

The CYD's full-screen 320x240 carousel now presents **league-wide** NHL standings, not just the two teams playing in the current game.

## Slide sequence

The existing current-day NHL game slides are followed by eight standings slides:

1. Eastern Conference — ranks 1–8
2. Eastern Conference — ranks 9–16
3. Western Conference — ranks 1–8
4. Western Conference — ranks 9–16
5. Atlantic Division — all eight teams
6. Metropolitan Division — all eight teams
7. Central Division — all eight teams
8. Pacific Division — all eight teams

Each slide contains up to **eight comfortably spaced rows** with the **official conference/division rank**, 3-letter team abbreviation, games played, full season W-L-OT record, and standings points. Alternating dark rows, a fixed header and columns, and an accent mark for the armed game's teams make the whole league legible without a tiny 16-row conference page. Rows never change which game is armed.

Swipes move backward and forward through both games and rankings, and the existing rotation interval applies to all carousel slides. The top-right menu works on standings slides. Stats/detail taps are only active for game slides. The carousel also shows standings when **no NHL games are scheduled today**, if the setting is enabled.

## Data freshness and memory

Reuses the existing NHL dated standings endpoint used for pregame records: \`/v1/standings/YYYY-MM-DD\`. Parsed fields include \`teamAbbrev.default\`, \`conferenceAbbrev\`, \`divisionAbbrev\`, \`conferenceSequence\`, \`divisionSequence\`, \`gamesPlayed\`, \`wins\`, \`losses\`, \`otLosses\` and \`points\`. Official sequence fields determine rank, **not** local sorting by points (which might mishandle ties). The standings cache is league-wide and includes 32 teams. The same cache is shared by pregame team preview and standings pages.

Standings update on a separate cadence of **at most once every 3 minutes during live games**, or **once every 15 minutes when idle**. The scoreboard continues its existing 10-second armed/followed live checks even if the screen is showing standings; WLED and rear RGB alerts are independent of the visible slide. Requests for the extra standings data are deferred while WLED restoration or rear goal flashes are active.

To avoid repeating the v0.2.8 stack overflow, both the standings filter and parsed JSON live on heap, and no fullscreen framebuffer is allocated. The new status JSON is also heap-allocated. Invalid/truncated standings results cannot replace an already loaded cache. Date rollover clears stale cache automatically.

## Logo-variant fix

Some NHL team logos appeared color-inverted while others were correct. The NHL's returned \`logo\` field may reference \`*_light.svg\` (for light backgrounds); the scoreboard JSON also exposes \`darkLogo\` for \`*_dark.svg\` artwork. The generator now **prefers \`darkLogo\`**, with official \`*_dark.svg\` as the first fallback, and only uses \`*_light.svg\` if no dark variant can be retrieved. This affects firmware-embedded game logos and their detail-page counterparts. It **does not** change the CYD's display-wide invert setting or the RGB565 rendering path. Check contrast and team colors on actual hardware after updating.

## Web controls

Dashboard → **Display** → **Include league-wide conference & division rankings** turns all eight standings slides on or off. Defaults to on. The value is stored in NVS and persists over reset.

The Display tab reports which slide is currently on the screen, the total number of game/standings slides, and whether the league cache has loaded. The original carousel auto-cycle switch and seconds-per-slide setting continue to apply.

## Testing

The native C++ \`tests/test_standings_carousel.cpp\` verifies all eight page descriptors, group membership, ordering by official ranks, slide counts, zero-game days, and swipe wrapping. GitHub Actions also rebuilds 32 official logo sprites using the dark-mode preference and compiles the CYD firmware.

**Physical test:** After OTA flashing, swipe past today's games. Inspect East 1–8 and 9–16, West 1–8 and 9–16, then all four divisions. Verify ranks, records and points, check how legible the rows are at normal viewing distance, and confirm the top-right menu. Disable standings in the Display tab, save, and confirm that only NHL games remain. Re-enable and check persistence after restart. Confirm goal light updates continue while rankings are showing and previously inconsistent team logos now have a readable dark-background variant.
