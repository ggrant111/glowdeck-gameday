# GlowDeck GameDay — Phase 6B Arena Broadcast (S3 only)

## Scope and status

The Waveshare ESP32-S3-Touch-LCD-3.5 uses a cinematic arena-image background
for **Live Game** pages, with dynamically rendered NHL team logos, score,
period/clock, shots-on-goal and power-play/empty-net status. This is the first
Phase 6B hardware integration, not the entire Phase 6 redesign. Stats,
Standings, game lists, events, setup, and GIFs still use the Phase 5 renderer.

The original CYD 320x240 target, NHL API logic, all-live event scope, goal
queue priorities, followed/armed games, rear LED, WLED, and Fire TV remain
unchanged. An unreadable background automatically falls back to the prior
Phase 5 Live screen. Build-time switch:

- `-D GLOWDECK_S3_PHASE6_LIVE=1`: use the arena artwork (default).
- `-D GLOWDECK_S3_PHASE6_LIVE=0`: restore the Phase 5 native S3 scoreboard.

## Phase 6B.1: darker, full-native arena artwork

The previously installed S3 used a 192x128 RGB565 thumbnail that the
firmware upscaled to 480x320. Hardware testing showed soft details, a blue
haze and poor text separation. Phase 6B.1 **replaces the thumbnail** with a
crisp, native 480x320 image: deep charcoal, understated arena rafters and
light accents, dark team-logo plinths, a separate score stage and a flat,
high-contrast SOG/status plate. The background contains **no NHL team logo,
score, clock, game state, labels or player data**. Those remain real-time
firmware overlays from the existing NHL engine.

## Phase 6B.2A — de-boxed broadcast layout

Physical S3 feedback showed that even the native, darker Phase 6B.1
background had too many crisp blue-edged cards. This update changes **only
the Arena Live Game visual layer**:

- Remove all visible border strokes around the time, away/home and main score
  zones. Their existing positions and dimensions become subtly feathered
  charcoal fields, keeping legibility without the look of separate boxes.
- Replace the three outlined shots/PP areas with **one continuous lower
  information band**, using two nearly invisible separators.
- Eliminate the outlined logo rings, replacing them with soft radial plinths
  behind the dynamically drawn official NHL logos.
- Remove the full-width footer divider and framed footer surface. The existing
  GAMES, page counter and SETUP labels float over a faint dark vignette.
- Remove the S3 live follow-star's rectangular background. The star and
  armed indicator remain clearly visible at their original position.
- Keep the actual NHL text, logos, team-primary accent bars, game states,
  touch geometry, carousel, and event/WLED functionality **unchanged**.

The exact generated artwork remains full-resolution 480×320 RGB565 and
decompresses just once into PSRAM. Phase 6B.2A modifies only the standard
Python generator and one S3-only live star helper; no display dependency,
compression format or UI coordinates change.

**Tests:** CI checks RGB565/PNG integrity, mean brightness, edge discontinuities
where each former border used to be, diffuse logo-plinth edges and uniform
lower-third fill. It also checks that the follow star has no background card
but retains the game follow/arm behavior and original hit target.

**Physical check:** compare a pregame and a live matchup against the generated
PNG, verify the former border lines are not visible, and ensure the score and
shot data are still high contrast. Test follow/armed stars, swipes and overlays.
A software build cannot certify the look of the physical ST7796 panel.

### Lossless asset pipeline (no third-party Python packages)

`scripts/generate_s3_arena.py` creates two resources using standard Python
only:

- `assets/s3_arena_live_480x320.png` — inspectable true-resolution PNG
  artwork, for designers and hardware comparison.
- `include/S3ArenaEmbeddedGenerated.h` — full 480x320 **big-endian RGB565**
  pixels compressed with zlib for flash. The immutable wrapper
  `include/S3ArenaEmbedded.h` includes this generated resource.

The generator runs automatically through the **S3-only PlatformIO pre-build
hook**, so a `git pull` cannot silently leave the old 192x128 background
in place. Both outputs are ignored in Git and are regenerated during local
builds and CI. To inspect the image manually:
```powershell
python scripts/generate_s3_arena.py
```

On first use, ESP32-S3 ROM miniz decompresses the entire asset **directly
into one ~300 KiB PSRAM background cache**, with no intermediate raster
buffer and no bilinear or nearest-neighbor scaling. The cache bytes already
match TFT_eSprite's swapped RGB565 representation on the little-endian S3.
The firmware checks the uncompressed byte count and CRC32 before making
the asset active; any failure cleanly falls back to the known Phase 5 S3
scoreboard. The existing 480x320 S3 foreground sprite remains separate.

Every S3 Live page copies that cache into the foreground sprite and then
renders dynamic logos, text, game time, scores, shots, PP and empty-net states.
During same-game updates, only two regions are restored from the cached
background and transferred over SPI: top (144,43,192,160), bottom
(48,215,384,53). The full image is decoded **once**, never each game tick.
No change was made to the other S3 pages, original CYD renderer, NHL polling,
favorite/follow behavior, WLED, or Fire TV.

At boot, the serial monitor at 115200 reports:
- `[S3 Color] ST7796 invert=ON/OFF ...` — **existing NVS setting is
  preserved**; this update never forces another inversion.
- `[S3 Arena] native 480x320 | compressed ... | cache ... |
  loaded ... ms | center RGB565 ... | free PSRAM ...`.

### Color integrity

CI compares the generated PNG's actual RGB bytes to the native RGB565
asset at multiple regions, validates full-image SHA-256/CRC32, tests
red/green/blue endianness, and bounds mean background brightness to prevent
a new washed-out image. It also verifies that firmware still reads and
applies the user's saved screen-inversion preference. Passing these tests
does **not** replace inspection of the physical ST7796 panel.


## Larger official NHL logos — Arena Live

The Phase 6B.2A hardware review also called for bigger, sharper team marks.
The logo generator now builds a separate **96×96 transparent NHL SVG raster**
for S3 (previously 80×80), while the original CYD uses its unchanged 56px
image size. On Arena Live the away and home logo rectangles are (38,96,96,96)
and (346,96,96,96). Each centers on its existing diffuse plinth without
moving the scoreboard, abbreviations or navigation hit zones. Team-primary
bars are drawn afterward at y=189 so they stay visible. This is a
**native raster increase**, not a runtime upscale.

The generator still pulls logos from official NHL JSON/CDN at build time, not
over the network during games. Existing 80/60/30/17px logo locations on
other S3 pages use the same new 96px asset scaled down, with no change to
their layout. Regenerate the ignored `include/TeamLogos.h` for a local build
using `python scripts/generate_logos.py` (requires Pillow and CairoSVG), or
use the prebuilt CI firmware to avoid local graphics dependencies.

## Phase 6B.2B — dynamic Upcoming / Live / Intermission / Final

All four NHL game phases now have a dedicated readout hierarchy on the
native Waveshare ESP32-S3 arena Live Game page. The large 96px official
team logos, borderless background, team-primary accents, follow star,
fixed footer controls, and all current hit targets are unchanged.

| NHL API state | Header | Center stage | Unified lower strip |
|---|---|---|---|
| `FUT` / `PRE` | Local start time + UPCOMING | Large VS | Existing cached season W-L-OT records when available, otherwise RECORDS PENDING; **no SOG/PP placeholders** |
| `LIVE` / `CRIT` | Live period/clock + LIVE indicator | Large current score | Actual SOG (or '--' only if NHL has not published it) and **validated** PP / pulled-goalie status |
| `LIVE` / `CRIT` with `inIntermission=true` | END OF [period] + INTERMISSION | Current score | SOG, no stale PP/empty-net indicator |
| `OFF` / `FINAL` / `OVER` | GAME COMPLETE + FINAL / OT / SO (or GAME OVER while awaiting final designation) | Final score, discreet winner-side accent when scores differ | SOG when available + winner abbreviation; no premature victory inferred from live score |

Unrecognized game states use a neutral GAME STATUS view without invented
scores or shots. Winning team is determined **only for completed games**,
not from the current lead during LIVE or INTERMISSION. Overtime/shootout
labels come from `periodDescriptor.periodType`, not from a score guess.

**No new NHL fetches:** The Upcoming records come from the existing
per-game `CarouselStats` cache, refreshed through the normal stats
prefetch. Verified power plays and empty-net states retain the existing
short freshness window and source validation. Cached data is matched
to the current NHL game ID so an adjacent game's record cannot leak
onto another matchup.

**Redraw safety/performance:** The two established dirty rectangles
(top x=144,y=43,w=192,h=160; bottom x=48,y=215,w=384,h=53)
are restored directly from the full-resolution RGB565 PSRAM arena
background before drawing current status, score, and statistics. This
erases old UPCOMING/PP/SOG/FINAL badges when a game changes state.
Only those rectangles are sent to the LCD for same-game updates;
carousel changes, overlay interruptions, and page navigation still
perform a full framebuffer redraw. Original CYD rendering, WLED
trigger rules, Fire TV integration and event queue are unchanged.

### On-device acceptance test

1. **Upcoming:** verify the start time and VS; SOG/PP '--' placeholders
   must be absent. Season records should show only when actually fetched.
2. **Live:** verify score, period, clock and real SOG. With an authenticated
   PP or pulled-goalie event, the indicator names the correct team.
3. **Intermission:** retain score/SOG; switch header to INTERMISSION and
   drop PP/EN badges. No visual leftovers from live clock.
4. **Final:** regulation, OT and SO status labels; winner highlight is
   only present when scores differ.
5. Test a state transition without changing carousel page, followed by
   swipe, tap-for-Stats, follow-star and a preview GOAL interrupt/restore.
   All logo and footer positions should remain stable.
6. Examine smoothness, memory and clarity on the **physical** 480x320
   panel. Passing compilation does not prove the display has been
   tested; retain `GLOWDECK_S3_PHASE6_LIVE=0` for rollback.

## Footer alignment fix — armed game slides (November 2026)

A physical Waveshare S3 test showed that page 2/3 shifted its bottom
navigation while 1/3 and 3/3 looked normal. Page 2/3 was **armed**, with
a small `A` beside its yellow follow star. The S3-only star renderer
changed `TFT_eSprite`'s text datum from centered to top-left after
drawing `A`, unintentionally moving subsequent GAMES, SETUP and page
counter labels on armed slides. The fix explicitly saves/restores the text
datum within the star renderer and anchors footer labels independently:
GAMES uses a left-mid datum at (16,300), SETUP uses a right-mid datum at
(464,300), and the slide number uses center-mid at (240,300). All three
are drawn identically whether or not the displayed game is armed.

**Hardware test:** cycle 1/3, 2/3 and 3/3 while toggling arm/follow for
each game. Confirm GAMES has a 16px left inset, SETUP a 16px right inset,
and the center counter never shifts or clips. Check the touch targets and
scoreboard transitions still respond as before. Only S3 live drawing was
changed; original CYD and WLED/event behavior are unaffected.

## Clean navigation

- **Swipe left/right:** cycle game carousel (same favorite/live ordering).
- **Tap central score:** game statistics.
- **Tap small upper-right star:** follow/unfollow game; does not arm it.
- **Tap lower-left GAMES:** all matchups.
- **Tap lower-right SETUP:** device settings.
- Automatic carousel and priority event overlays remain active.

No large "GLOWDECK" top bar or PREV/GAMES/NEXT footer is drawn on this screen.
Other pages keep Phase 5 navigation until their individual Phase 6 conversions.

## Physical S3 validation (required before considering rollout final)

1. Load the S3 Phase 6B firmware over USB or OTA and check boot diagnostics
   for `[S3 Arena] loaded ...`; reboot loops are not acceptable.
2. Confirm the new background is genuinely dark, clean and sharp at 480x320,
   including its thin arena lines, without cyan haze. Compare the generated
   `assets/s3_arena_live_480x320.png` with the physical LCD. Check 80px NHL
   logos, score/clock/SOG, and verify the saved inversion preference is unchanged.
3. Swipe through games; tap the score for Stats, return, follow/unfollow via
   star, and use lower corners to enter Games and Setup.
4. Trigger preview GOAL/PP/FINAL from the dashboard. Event overlays must
   interrupt and then return to the current game without stale arena artifacts.
5. Verify score and SOG updates transfer just the expected regions; measure
   elapsed frame times and free PSRAM. Check Wi-Fi/API polling remains timely.
6. Switch the build flag to 0 and build/flash the old S3 scoreboard if any
   layout, performance or boot problem is observed. The source and CI maintain
   this fallback intentionally.

## Files

- `scripts/generate_s3_arena.py` — deterministic 480x320 background source
- `include/S3ArenaEmbedded.h` — stable resource include wrapper
- `include/S3ArenaEmbeddedGenerated.h` — generated full-resolution RGB565 asset
- `assets/s3_arena_live_480x320.png` — generated artist preview (not firmware)
- `include/S3ArenaBackground.h` — miniz inflate, PSRAM cache, region restore
- `include/S3NativeLayout.h` — contextual live hit targets/dirty rectangles
- `src/main.cpp` — native broadcast overlays and navigation
- `tests/test_s3_phase6_arena.py` — native asset, color, CRC and saved-inversion tests
- `tests/test_s3_native_layout.cpp` — hit geometry tests
