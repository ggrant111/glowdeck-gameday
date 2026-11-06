# GlowDeck GameDay — Native Waveshare S3 UI (Phases 1–3)

## Scope

The **Waveshare ESP32-S3-Touch-LCD-3.5** (ST7796, FT6336, 480x320
landscape) uses a separate full-resolution "Arena Broadcast" rendering path
for the primary NHL scoreboard, compact four-game carousel slides, the
all-games picker, pregame/live/final game statistics, conference/division
standings, and the no-games state.

The original 2.8-inch CYD keeps its existing 320x240 drawing and resistive
touch implementation. There is no change to the NHL API, event queue, favorites,
goal celebration rules, WLED state restoration or AP/browser controls.

## Architecture and memory

- The S3 allocates one 480x320 RGB565 TFT_eSprite canvas (~300 KiB), and
  sends that sprite directly to the physical TFT for **native** screens using
  pushSprite(). These pages are not horizontally or vertically resampled.
- A second ~300 KiB RGB565 PSRAM staging buffer remains for the **transitional**
  320x240 legacy screens. As those pages are converted, that buffer can be
  removed. The S3 must have its configured OPI PSRAM operational.
- Legacy event/GIF overlays, setup and calibration are
  temporarily scaled with the old path; the GIF remains exactly 320x240 and
  retains its uploaded flash storage and WLED synchronization.
- Score/period/SOG/PP/EN changes on the same featured game repaint the opaque
  readout regions of the sprite without re-decoding official NHL logos. An
  update still transfers the complete native sprite to the display; dirty
  rectangle SPI transfers and higher-resolution logos are later optimizations.
- The geometry in `include/S3NativeLayout.h` is hardware-pixel based and
  independently host-tested. The FT6336 180-degree correction is retained.
  Native touch uses actual display coordinates, independent of saved legacy
  CYD calibration values.

## New behavior

1. **Live/pre-game/final scoreboard:** bold native score, official logo pixels,
   away/home team colors, clock/status, SOG, verified PP and empty-net status;
   follow/armed indicator, menu, and large footer controls.
2. **Live-first carousel:** retains favorite-first ordering and one page per
   live game. Up to four non-live games appear in a native summary with a
   per-game follow star and stats tap target.
3. **All games:** GAMES / menu opens a four-row native picker, with up to 20
   matchups, page buttons, swipe paging, individual follow stars and tap-to-arm.
4. **No games:** native loading/empty/error view.
5. **Game details (Phase 3):** fully native team comparison view with official
   logos, current game state, up to five NHL detail rows and compact numerical
   comparison bars when both values are known. Pregame rows show season
   records, conference comparison, rank and points without inventing stats.
   Back, ARM GOALS, SETUP and follow-star buttons have dedicated touch targets.
6. **Standings (Phase 3):** native eight-row conference/division tables with
   official ranked team order, team color accents, GP, W-L-OT, PTS and armed
   team emphasis. Previous/next and swipe actions keep the old carousel order;
   standings rows do not arm games.
7. **Remaining legacy screens:** setup, calibration and event/GIF overlays
   retain compatibility scaling until Phase 4.

## Test checklist on the physical S3

- Confirm startup and 480x320 landscape color rendering; no Guru Meditation
  reboot loop. Keep `-D USE_FSPI_PORT` enabled in the S3 environment.
- On a live game, check crisp unmagnified logos/text, two-digit scores, correct
  clock and SOG, and PP/EN status where NHL data exists.
- Tap the top-right menu; open native GAMES picker, then its SETUP menu; return
  via TODAY. Arm any matchup from the picker and verify WLED/game state.
- Tap the game-level star and each summary/picker star; verify that follow
  state changes without arming a game or changing WLED preferences.
- Swipe between carousel games and use large PREV/NEXT footer buttons. In the
  GAMES picker, page through all games, including the last partial page.
- Tap the scoreboard to view native game details. Test Back, ARM GOALS,
  follow star and Setup separately, including games that are already armed.
- Check pregame season, conference rank, records and points, then live/final
  numeric comparison rows; unavailable metrics should display an appropriate
  loading/empty state and should not become fabricated zeroes.
- Navigate to native standings when no games are live and verify all eight
  conference/division pages, sorted ranks and current-day freshness.
  Confirm standings rows never accidentally arm a game.
- Preview the legacy goal GIF and ensure it still renders after returning
  from the native detail/standings screens.
- Verify all-game event overlays, goal triggering, queue priority, and WLED
  restoration still operate. Confirm no AP or OTA changes.
- Record serial monitor at 115200 and free PSRAM after native sprite startup.

## Waveshare panel color default (Phase 3)

The original S3 firmware chose a fresh-device `invert=false` setting, but the
first physical unit displayed incorrect colors until the user tapped **INVERT**
once. For **new or erased S3 configurations**, the firmware now chooses the
observed **correct `invert=true` controller state** by default. This does not
reverse the sprite's geometry or change CYD behavior; the switch is a TFT
controller color-mode command. An existing saved `invert` choice in ESP32 NVS
is honored, so the user who already corrected the colors does not have to
reconfigure anything. The web dashboard and touchscreen toggle remain available
for panel revisions that behave differently.

## Build and rollback

```powershell
pio run -e s3_waveshare_35 -t upload --upload-port COM7
pio device monitor -p COM7 -b 115200
```

Replace COM7 with your board's port. For firmware OTA, upload only the S3
`firmware.bin` built with the same flash layout. Keep a known-good S3
firmware build for recovery by USB. The CYD builds independently:

```powershell
pio run -e cyd
```

Physical touch alignment and graphics performance must be confirmed on the
board even when both GitHub Actions builds pass.
