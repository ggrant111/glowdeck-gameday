# GlowDeck GameDay — Native Waveshare S3 UI (Phases 1–5)

## Scope

The **Waveshare ESP32-S3-Touch-LCD-3.5** (ST7796, FT6336, 480x320
landscape) uses a separate full-resolution "Arena Broadcast" rendering path
for the primary NHL scoreboard, compact four-game carousel slides, the
all-games picker, pregame/live/final game statistics, conference/division
standings, the no-games state, event celebrations, device setup and touch check.

The original 2.8-inch CYD keeps its existing 320x240 drawing and resistive
touch implementation. There is no change to the NHL API, event queue, favorites,
goal celebration rules, WLED state restoration or AP/browser controls.

## Architecture and memory

- The S3 allocates one 480x320 RGB565 TFT_eSprite canvas (~300 KiB), and
  sends that sprite directly to the physical TFT for **native** screens using
  pushSprite(). These pages are not horizontally or vertically resampled.
- Phase 4 removes the additional ~300 KiB RGB565 resampling buffer entirely.
  All S3 screens now render at the physical 480x320 resolution. The board still
  requires its configured OPI PSRAM for the single native canvas.
- Uploaded 320x240 goal GIFs are drawn at original pixel size inside a
  centered (80,40) viewport, with purpose-built 40px top/bottom ribbons and
  side rails. The GIF is **not stretched, cropped or resampled**. GIF storage
  in LittleFS, the existing 1 MB upload validation, NHL/WLED triggers and the
  goal event queue continue unchanged.
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
7. **Celebrations (Phase 4):** full-resolution goal, power play, puck drop
   and final-result cards, with team-primary color, logo, score, period/clock,
   dismiss control, pulsing/flashing border and duration indicator. Goal GIFs
   play 1:1 in the center with a separately drawn native scoreboard HUD.
   Invalid GIFs fall back to vector celebrations without interrupting WLED.
8. **Setup (Phase 4):** native network/hotspot details, TEST LIGHTS, TOUCH
   CHECK and COLORS controls, plus TODAY/GAMES/BACK navigation. The existing
   home-Wi-Fi web admin policy and protected setup hotspot remain unchanged.
9. **Touch check (Phase 4):** 4 corners and last-touch coordinates to verify
   orientation. The factory-capacitive FT6336 requires no two-point resistive
   calibration; this page never rewrites calibration NVS values. Legacy CYD
   calibration is unaffected.

## Phase 5 — framebuffer isolation, readable stats and faster live updates

**Standings-to-stats overlap root cause.** The first S3 native renderer
inherited several **nonvirtual** `TFT_eSPI` drawing methods, including
`fillScreen()`, `fillRoundRect()`, `drawRoundRect()`, `fillCircle()`,
and `fillTriangle()`. Those routines rendered directly to the LCD, even
though the final GameDay frame is composed in `TFT_eSprite`. The next
`pushSprite()` could display pixels from the *previous* page because the
native framebuffer had not actually been cleared. This particularly affected
opening stats immediately after standings or a game list.

`GameDayS3Display` now shadows these routines with sprite-native drawing
operations: `fillScreen` delegates to `fillSprite`, cards/borders use sprite
scanlines and pixels, and star/circle fills stay within the framebuffer.
Every complete page change clears the entire 480×320 sprite before drawing
opaque new cards and labels. CYD continues using its old direct-LCD methods.

**Text layout:** labels, team records, and status values in native stats and
standings have measured width limits and are truncated with ellipses rather
than overwriting neighboring cells.

**SPI efficiency:** the existing same-game score/period/SOG update path now
presents only two bounded rectangles: (154,61)–(326,190) for clock/score and
(14,220)–(466,265) for shots. Full navigation, NHL goal/PP overlays, standings,
stats and logo updates continue to present the *whole* sprite, never
reusing a partial region from a different screen.

**Validation:** host geometry tests verify rounded-card corners, cropped
regions, footer/logo exclusion, and a complete buffer-clear model. A
code-contract test specifically guards against regressing to the inherited
physical LCD drawing primitives. Passing CI does **not** substitute for
checking the screen on the physical Waveshare panel.

### Physical acceptance check

1. View any conference/division standings slide; open GAMES, choose a game,
   then open its stats. No standings rank, points, rows or title should
   show through any part of the Stats page, including rounded corners.
2. Repeat from a live score slide, a four-game summary and a GOAL overlay.
   Navigate back and forth multiple times and leave stats loading/updating.
3. Ensure the official 80px logos, text, star fills and rounded button cards
   remain visible and use the correct team colors.
4. Watch a live game clock/score update and confirm only the readout changes,
   while logos and navigation stay stable without flickering.
5. Confirm original CYD, WLED celebrations, follow/arm behavior and
   settings are unchanged.

## Official NHL logos on the native S3 framebuffer

**Fix for blank logos (November 2026).** The previous generated header
declared only `drawTeamLogo(TFT_eSPI&)`, whose non-virtual `pushImage` went
straight to the physical ST7796. The S3 UI now composes every screen into an
off-screen `TFT_eSprite` first; presenting that framebuffer subsequently
overwrote those direct-to-panel logo pixels. A dedicated
`drawTeamLogo(TFT_eSprite&)` overload now unpacks transparent 4-bit pixels
into the native sprite with `drawPixel`, which persists when
`pushSprite()` transmits the final frame.

The **logo URLs are sourced from the NHL scoreboard JSON** during the build,
preferring `darkLogo` and falling back to the official
`assets.nhle.com/logos/nhl/svg/{TEAM}_dark.svg` endpoint. CairoSVG converts
SVG once at build time; the image generator packs all 32 clubs into flash.
The new S3 build uses **80×80 source assets** and sizes them appropriately
(80px scoreboard, 60px details, 30px game lists, 17px standings, 72px
celebration card, 56px GIF side rail). The CYD retains its original 56px
assets and unchanged direct-panel renderer.

No extra network requests, SVG rendering libraries, Wi-Fi dependency, user
settings changes or filesystem uploads are needed **on the ESP32-S3** for the
logos to appear. If the NHL API omits a team on the current day, the generator
uses that team's official CDN asset at build time. Unknown team codes display
their abbreviation rather than a silent blank.

**First hardware check:** after updating the S3 firmware, verify away/home
logos on the main score view, two smaller icons per row in GAMES, standings
icons, both logos in GAME STATS and the scoring-team logo on a GOAL overlay.
For a GOAL GIF, the logo lives in the left side rail while the centered
animation remains 320×240 unscaled.

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
- Upload and preview a 320x240 goal GIF. Confirm 1:1 centered pixels, team
  color framing, live score ribbon, dismiss X and return to the exact screen.
  Remove the GIF and preview again to confirm native vector fallback.
- Preview GOAL, both POWER PLAY states, PUCK DROP and FINAL from the dashboard.
  Verify each native overlay's logo, time/status, duration indicator, visual
  treatment and queue priority, including preemptions during goal events.
- Enter native SETUP from the GAMES picker or stats page, verify the protected
  AP credentials and locally hosted dashboard, and test light preview/color
  toggle without losing preferences. Run TOUCH CHECK on all four corners;
  it must never modify the CYD/legacy calibration fields.
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
