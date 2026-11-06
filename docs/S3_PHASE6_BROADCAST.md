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

## Image pipeline

The source is `bg_live_480x320.png` from the Phase 6A design package.
To limit flash consumption and expensive runtime PNG decoding, a 192x128
RGB565 version is zlib-compressed and embedded in
`include/S3ArenaEmbedded.h` (~6KB compressed). It is **not fetched over Wi-Fi**.
On first use, ESP32 ROM miniz inflates the image and smooth-scales it to a
480x320 RGB565 **background cache in PSRAM** (~300 KiB); the existing TFT_eSprite
is a separate 480x320 framebuffer (~300 KiB). Temporary ~49KB source pixels
and ~6KB compressed buffers are released immediately.

The Phase 6A full-resolution artwork is the visual master, and the compressed
version is intentionally a smaller embedded sampling. The logos and text
remain full-resolution; hardware evaluation may later justify higher-density
background data, a separate asset filesystem, or alternative compression.

On page change, `s3Arena.restore(tft)` copies the background into the native
sprite. Logos, labels and scores are layered over it. During a same-game NHL
readout change, only two regions are restored from the cached background and
redrawn: top (144,43,192,160), bottom (48,215,384,53). Those same two
rectangles are sent over SPI via `presentRegion()`; the background is **never
re-inflated** on each poll or animation frame. The Phase 5 full-clear and
sprite-safe text/shape renderer remains in use.

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
2. Confirm correct colors without changing the existing inversion preference,
   sharp official NHL logos, readable clock/score/SOG and no drawn text behind
   another screen's content.
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

- `include/S3ArenaEmbedded.h` — optimized compressed arena art (no logos)
- `include/S3ArenaBackground.h` — miniz inflate, PSRAM cache, region restore
- `include/S3NativeLayout.h` — contextual live hit targets/dirty rectangles
- `src/main.cpp` — native broadcast overlays and navigation
- `tests/test_s3_phase6_arena.py` — compressed payload integrity checks
- `tests/test_s3_native_layout.cpp` — hit geometry tests
