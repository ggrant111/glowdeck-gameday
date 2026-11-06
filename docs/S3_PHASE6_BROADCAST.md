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
