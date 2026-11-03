# v0.2.3 — Official NHL logo JSON + automatic carousel stats

The screenshot in v0.2.2 showed that logos were *still* horizontal stripes. The root cause is confirmed in the build generator: it rendered and packed each mark as **56×56**, but hard-coded `TEAM_LOGO_SIZE = 48` in the generated header. The scanline renderer read each row with the wrong stride, corrupting the visual without any compiler error.

**Fix:** Generate the C++ dimension directly from the Python `SIZE` constant, and add a C++ compile-time assertion that the encoded pixel count matches that dimension. The renderer continues to use transparent RGB565 scanline runs as in v0.2.2.

## Using the NHL JSON's online logos

The source NHL score API returns each team's `homeTeam.logo` / `awayTeam.logo` URLs as official SVGs. The generator now reads `/v1/score/now` at build time, prefers those exact returned `assets.nhle.com` URLs, and uses the corresponding official NHL CDN logo endpoint for teams not on today's schedule. These are converted into the firmware's tiny color assets for dependable offline rendering. **The ESP32 does not download or decode SVG images at runtime**, so the image is available as soon as the carousel advances. This still uses the NHL's official online assets as its source.

## Bottom strip loads statistics automatically

When a game first appears in the carousel, the CYD fetches the game-specific right-rail comparison (or standings for upcoming games), saves the available data in a per-game cache, and updates the bottom strip. It no longer waits for a manual visit to STATS.

- Live / completed: **PP**, **PIM**, **FO** (percentages are rounded on the compact strip).
- Pregame: available **team records** and **standings points**.
- Unpublished values show **--**; there is no "GAME 2/4", "VIEW ONLY" or "TAP STATS" placeholder text in the stats strip.
- Stats refresh every 60 seconds for live games, 15 minutes for pregame, and hourly for final results, only when the game is on screen. Fetches defer while a WLED light-state restoration is pending.
- The separate STATS view still displays additional comparisons, and browsing still never changes the armed WLED game.

## Hardware validation

After OTA, compare both logos for Colorado and Chicago and a second pair. They should be full 56×56 team logos, not horizontal bands. Let the carousel rotate: bottom stats should change from `--` to actual figures automatically, without opening STATS. An NHL game without available comparison stats may correctly continue showing `--`. Confirm that WLED, OTA and the top-right menu still work.

---

# Previous updates — Full-screen broadcast scoreboard

The daily carousel has been restyled to match the supplied NHL scoreboard reference, adapted for 320x240 pixels. The full-screen layout presents a compact NHL header, a LIVE/NEXT/FINAL indicator, a large period/clock banner, large scores, authentic 56px transparent team logos, team-color underlines, a shots-on-goal strip, and contextual match information below. It uses a dark, vector-drawn arena/rink background without needing large bitmap framebuffers.

**No persistent bottom navigation buttons appear on the carousel.** To navigate, **swipe left/right** for the next/previous game. **Tap the center of the scoreboard** to open team stats. **Tap the small top-right menu icon** to open the game list; from there you can select SETUP. The corners of the lower band also support previous/next tapping. Stats still has BACK / ARM / SETUP, and the Games page retains regular navigation.

**Logos:** The logo generator packs 56x56 images as 4-bit palette indices in flash. The v0.2.2 renderer explicitly decodes each row to ordinary 16-bit RGB565 runs and skips transparent pixels, avoiding the striped result from the previous indexed pushImage call. Source marks remain NHL/team trademarks.

**Display refresh:** NHL polls that update the same game only redraw the period, score and SOG/stats panels, not the logos or full screen. Automatic carousel transitions repaint the full 320x240 screen, so display colors and the touch targets still require hardware validation.

**Goal alerts:** Swiping/rotating the carousel does *not* change the armed game. To arm a new game, tap the main board to open stats then use ARM, or open the top-right menu and select a game in GAMES.

**Unpublished data:** SOG shows a dash until reported. PP/PIM/faceoff values appear in the bottom band when loaded by the on-demand NHL game-stats endpoint. Before those stats have been fetched, the band uses game number, armed status and a stats hint; no made-up statistics are shown.

**OTA:** As before, use the successful firmware artifact from GitHub Actions, unzip `firmware.bin`, and upload from the CYD's built-in Update page. Existing Preferences/NVS settings are retained.

---

# v0.2.0 — Daily NHL carousel & team stats

## New on the 2.8-inch CYD display

**TODAY** replaces the single-score live card as the main view. It automatically moves through all NHL games returned for the scoreboard's \`currentDate\`, with:

- NHL team logos stored **in firmware flash** (no logo HTTP requests during animation).
- Away/home matchup and scores when underway or final; **VS** if not started.
- Scheduled start formatted in the selected **ET / CT / MT / PT / AZ** timezone; DST is handled with POSIX TZ rules.
- **LIVE** period, OT/shootout and remaining clock, or an intermission label.
- **FINAL**, **FINAL / OT**, **FINAL / SO** for completed games.
- Shots on goal when present in the API.
- A counter such as \`2 / 11\` and **PREV / NEXT** to browse without waiting.

**Tap the matchup** (above the row of control buttons) to open **STATS**. In live/completed games this requests NHL's \`/gamecenter/{id}/right-rail\` comparison and shows available shots, faceoffs, power plays, hits, blocks, and penalty minutes. In a pregame matchup, the firmware requests \`/standings/now\` for the two teams' W-L-OT records and points. Missing stats are shown as unavailable rather than fabricated zeros.

The stats view contains **BACK**, **ARM** (goal events), and **SETUP** navigation. The Games list still allows explicit goal-alert selection.

## Goal alerts are independent of the carousel

The **arm** action (or selecting a game on the Games tab) remains the *only* way to select which game triggers WLED and sound. Browsing or auto-advancing the carousel **does not change** \`selectedGameId\`, goal-event baselines, or saved lighting state. This is essential for monitoring Utah while watching other NHL matchups on the display.

## Browser dashboard

In the new **Display** tab:

- Enable/disable auto-cycling (default enabled).
- Choose 5–30 seconds per matchup (default **9 seconds**).
- Choose local start time display: Eastern, Central, Mountain, Pacific, or Arizona. Default **Central**.
- See the current matchup and the NHL scoreboard date.

The **Games** tab also shows official SVG team logos when the phone/browser can access the NHL CDN, plus a separate **Stats** button and **Arm** button. Actual CYD logos are compiled ahead of time and require no runtime downloads.

## OTA / building the firmware

This repository contains a logo build script, \`scripts/generate_logos.py\`. It downloads the current NHL team marks from the league's public CDN, rasterizes each SVG at 48×48 and packs 4-bit palette sprites into \`include/TeamLogos.h\` (generated and ignored by git). Compiling the firmware then embeds all 32 team marks directly in flash.

GitHub Actions runs this automatically before each PlatformIO build and uploads \`firmware.bin\` as an artifact. For a local build:

\`\`\`powershell
python -m pip install Pillow==11.3.0 CairoSVG==2.8.2
python scripts/generate_logos.py
pio run -e cyd
\`\`\`

It intentionally fails instead of publishing firmware with incomplete or incorrectly downloaded NHL logos. If a CDN is unreachable, retry or inspect network access; do not silently substitute missing marks.

Once the successful CI build artifact is ready, unzip its \`firmware.bin\`, open the running CYD's web dashboard **Update** page, and OTA flash it. Do not upload the ZIP directly. Settings in NVS, including WLED configuration and the v0.1.1 screen inversion preference, are preserved.

## Safe validation steps

1. Confirm dark interface colors remain correct after reboot.
2. Open **TODAY** and watch it cycle; tap NEXT and PREV.
3. Confirm an unplayed game shows a start time in your configured time zone.
4. Confirm a completed game shows FINAL and a score; live games show period and clock.
5. Open the game’s stats; verify shots/faceoffs where they exist and W-L-OT for a future game.
6. Arm a single live matchup; allow the carousel to rotate past that game and verify goal events still monitor the **armed** game.
7. Confirm original WLED state is restored after testing a celebration.
8. Use web **Display → Auto cycle** to pause; repeat and change the interval.
9. Reboot and confirm the carousel and time zone settings survived.

## API considerations

The NHL public, unofficially documented API may change its JSON structure. Scoreboard fallback is designed to keep displaying previous data if a poll fails; stats are fetched only when requested. See [Games API reference](https://github.com/pseudo-r/Public-NHL-API/blob/main/docs/web-api/games.md) and [right-rail schema](https://rdrr.io/cran/fastRhockey/man/nhl_gamecenter_right_rail.html).

NHL team logos are used for identifying game participants; all NHL logos remain the trademarks of the NHL and their respective teams. This is a personal hobby display and no affiliation with the NHL is claimed.

## v0.2.2 hardware fixes

After two device photos showed striped team logos, an overly blue arena, and decimal faceoff ratios, this release changes the drawing path without altering Wi-Fi, WLED, OTA or game selection. Team logos use 16-bit row runs with transparent gaps; the background is much darker; faceoff values render as percentages such as 37.9% rather than 0.37931. The stats page has a higher-contrast dark comparison panel.

**Hardware smoke test:** Verify both team logos on at least two games, confirm the background is dark navy, tap through to see shots/faceoffs, swipe the carousel, and check that OTA/settings and armed-game selections still work. GitHub compilation alone cannot verify the physical panel rendering.
