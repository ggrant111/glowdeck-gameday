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
