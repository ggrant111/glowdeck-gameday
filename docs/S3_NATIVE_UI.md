# GlowDeck GameDay — Native Waveshare S3 UI (Phases 1 and 2)

## Scope

The **Waveshare ESP32-S3-Touch-LCD-3.5** (ST7796, FT6336, 480x320
landscape) uses a separate full-resolution "Arena Broadcast" rendering path
for the primary NHL scoreboard, compact four-game carousel slides, the
all-games picker and the no-games state.

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
- Legacy event/GIF overlays, stats, standings, setup and calibration are
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
5. **Other screens:** remain available through the original legacy renderer
   until Phase 3 (stats/standings) and Phase 4 (overlays/GIF).

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
- Tap a scoreboard to reach existing detailed statistics; view standings
  and goal GIF preview; confirm they still render during transition.
- Verify all-game event overlays, goal triggering, queue priority, and WLED
  restoration still operate. Confirm no AP or OTA changes.
- Record serial monitor at 115200 and free PSRAM after native sprite startup.

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
