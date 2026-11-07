# GlowDeck GameDay — Waveshare S3 Phase 6C: Unified Broadcast Pages

## What changed

The 480×320 Waveshare **ESP32-S3-Touch-LCD-3.5** now has a single
broadcast presentation language across the existing native screens,
not just the Arena Live Game slide introduced in Phase 6B.

This is a **presentation-only** update. The shared NHL scoreboard,
validated PP/empty-net evidence, carousel priority, multiple followed
games, armed goal celebrations, rear LED/WLED rules, web dashboard, and
Fire TV are unchanged. The original ESP32-2432S028R CYD build is
unaffected, as is the distinct Waveshare **3.5B** (unsupported).

### Updated S3 pages

| Screen | Phase 6C appearance | Content/navigation preserved |
| --- | --- | --- |
| Four-game LIVE summary and All Games picker | Borderless alternating fixture rows, twin NHL marks, compact scores, subtle team-color rails and frameless follow stars | All daily games, game status, star follow/arm distinction, 4 rows/page, tap rows, swipe and paging |
| Game Stats | Broadcast matchup hero with real NHL logos, score and status; comparison rows with team-color data bars | Cached pregame records, gamecenter stats, ARM GOALS action, Stats BACK/SETUP and follow star |
| Standings | Condensed standings table with understated alternating bands, NHL marks and clear W-L-OT/PTS typography | Existing conference/division pages, real ranks, GP, records, points, carousel order |
| Setup | Same low-contrast canvas, restrained text hierarchy and three clear utility touch zones | Network/IP/hotspot details, test lights, touch check, colors, TODAY/GAMES/BACK |
| Touch Check | Simplified alignment targets and consistent footer | All four FT6336 targets, touch coordinates, reset/back/game navigation |
| No games/loading | Minimal empty-state message and clear navigation | Timezone/date and NHL error/loading state are unchanged |
| GOAL, PP, START, FINAL and GOAL GIF | Broadcast event hierarchy, team color rails and frameless close; GIF remains centered at native 320×240 | Event priority, timing, queue, logo, scoreboard, progress, dismiss, overlay-to-previous-page restoration |

All new pages use an opaque procedural dark charcoal arena-like canvas
rendered **directly into the existing TFT_eSprite**. No new image
resource, network call or extra PSRAM framebuffer is allocated. NHL
team colors remain live overlays; the original Arena Live Game's
separate native PSRAM art cache and partial refresh from Phase 6B.4
remain unchanged.

### Hit targets are intentionally unchanged

The UI gets cleaner without moving any interactive regions:

- Header follow star: x=374..421; menu: x=428..476
- Four-game row: x=12..467, star x=420..467
- Bottom navigation: PREV x=12..126, center x=172..307,
  NEXT x=353..467
- Setup tests and four-corner FT6336 alignment positions retain their
  existing rectangles
- Event close stays x=428..477, y=2..39
- GOAL GIF viewport remains exactly x=80..399, y=40..279
- The Phase 6B Arena Live screen retains its custom corner footer
  and broad tap-for-Stats zone, independent of this change

### Reversible rollout

The new build flag is set **only** in the S3 environment:

```ini
-D GLOWDECK_S3_PHASE6C_UI=1
```

Change it to `=0` and rebuild to recover the **Phase 5 secondary
screen renderers** without restoring older firmware. This does not
change the independent Arena Live rollback flag:

```ini
-D GLOWDECK_S3_PHASE6_LIVE=1
```

Both flags can be toggled independently. All original functions remain
in `src/main.cpp`, while the new render layer is
`include/S3BroadcastPages.h` and its pure geometry/palette contract
is `include/S3BroadcastPagesPolicy.h`.

### Test coverage

GitHub Actions runs Phase 6C host C++ rectangle/touch tests and Python
checks that the renderer uses the existing data model, preserves the
original UI behind the feature flag, does not fetch NHL data or change
celebration state, and keeps the unscaled GIF viewport. It also runs
all inherited Phase 6B tests and builds both `cyd` and
`s3_waveshare_35` firmware targets.

**These tests cannot certify physical color rendering or touch
alignment.** Hardware validation is still required after OTA.

## Device acceptance sequence

1. On the actual Waveshare ST7796 **3.5** board (not 3.5B), install
   the `s3_waveshare_35` CI firmware artifact via the established OTA
   flow. Verify a normal boot and arena logo colors.
2. Swipe the live carousel. Confirm the dedicated Arena Game slide is
   unchanged and the compact four-game summary is readable with up to
   four NHL matchups and no clipped star/score fields.
3. Open All Games, switch pages, tap each follow star, tap a game row,
   and check arm vs follow behavior. Return to Live and make sure the
   page footer positions don't shift for the armed game.
4. Open Stats for a future and live game; check both logos, real
   score/clock/status, updated stats, and BAR/ARM/BACK/SETUP navigation.
5. Browse all conference/division standings slides. Confirm rank,
   GP, W-L-OT and points fit without overlap or ghost pixels.
6. Open Setup, test all three utility buttons, enter Touch Check and
   register every corner. Confirm the diagnostic values and return
   controls work without accidental game selection.
7. Trigger goal, PP and final overlays; try a custom GIF if installed.
   Confirm the new event presentation, unchanged sound/WLED behavior,
   manual dismiss and automatic return to the exact previous slide.
8. Watch the device during live NHL updates for memory stability,
   normal polling and no stray Phase 5 pixels. Use serial 115200 to
   inspect the Phase 6B.4 `[S3 6B.4]` telemetry.

**Release status:** Automated tests/firmware build and real-hardware
verification are separate gates. Do not describe a clean CI run as
proof that the ST7796 panel itself has been tested.
