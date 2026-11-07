# Phase 6D — Waveshare 3.5-inch Broadcast acceptance and release gate

**Scope:** Phase 6D closes the Phase 6 display-design sprint for the
**Waveshare ESP32-S3-Touch-LCD-3.5 ST7796** (not Waveshare 3.5B or CYD).
It is an acceptance, stability and documentation release: no UI overhaul,
touch-mapping changes, NHL event policy changes or added display buffers.

## Hardware observations reported by the device owner

On **2026-11-06 (America/Denver)**, after Phase 6C:

| Area | Recorded status | Basis |
|---|---|---|
| Text placement | **Accepted** | Owner reports text positions in order |
| NHL logo placement | **Accepted** | Owner reports logo positions in order |
| Touch alignment | **Accepted (informal)** | Owner reports touch accuracy seems good |
| Goal/PP/final overlay visual smoothness | **Not separately observed** | Requires events or device simulator |
| Overlay dismiss and return to prior exact carousel slide | **Not separately observed** | Software regression covered; physical verification not reported |
| 30–60 minute PSRAM/SPI/OTA on-device soak | **Not separately measured** | Automated tests and built-in telemetry available |
| WLED / Fire TV celebration parity after 6C OTA | **Not re-confirmed in this check** | Shared celebration engine unchanged |

User observation means the UI **placement/touch acceptance milestone is
complete**, not that physical timing, memory and all live NHL scenarios
were measured. Do not translate CI or simulator success into a measured
ST7796 refresh rate or claim unreported hardware checks are green.

## Release safeguards

- `tests/test_s3_phase6d_stability.cpp` exercises **45,000** deterministic
  simulated policy steps, including repeated partial-score frames, game
  switches, goal→final→power-play recovery, bounded live-event backlogs,
  manual overlay dismiss and `millis()` wraparound.
- `tests/test_s3_phase6d_release.py` guards the complete event-to-full-
  scene recovery path, no-op SPI skips, 320×240 native GOAL GIF, team-logo
  presentation, unchanged touch map, retained CYD isolation, and both
  independent S3 rollback flags.
- CI keeps running the earlier Phase 6B.1–6B.4 / 6C tests and builds
  **both** `pio run -e cyd` and `pio run -e s3_waveshare_35`.
- S3's existing serial `[S3 6B.4]` stats are the physical measurement
  path: full/partial/skipped frames, estimated SPI byte totals, longest
  observed transfer in milliseconds, and remaining PSRAM. At 115200 baud,
  observe those during a live NHL period and after GOAL/PP overlays.
- The user-accepted screen geometry remains unchanged; no need to rewire
  the S3, remap touch, change the display inversion, or regenerate logos.

## Physical observability / deferred spot checks

The exact manual testing remaining **after** software 6D closeout:

1. During a real or simulated goal, PP and final event, ensure the screen
   animates, avoids tearing, handles dismissal and returns to the same
   Games/Stats/Standings/Live slide.
2. Test a custom GOAL GIF (if uploaded) and ensure the side NHL mark
   appears without stretching the 320×240 animation.
3. Let the ST7796 device run 30–60 minutes through live-game polling,
   game-carousel rotations, score changes and events. Check serial
   `[S3 6B.4]` metrics for stable PSRAM headroom and no reboot.
4. Confirm selected WLED lights and existing Fire TV celebration sound
   continue to activate and restore prior state on a goal.

These checks are not release blockers for the already-accepted text,
logos and touch layout; log any issue as a Phase 7 regression rather
than silently declaring it tested.

## Rollback and next milestone

Rollback only secondary UI: `-D GLOWDECK_S3_PHASE6C_UI=0`.
Rollback only Arena Live: `-D GLOWDECK_S3_PHASE6_LIVE=0`.
Never flash this S3 firmware onto the original 320×240 CYD or 3.5B.

**Next: Phase 7A — WaveShare S3 onboard audio discovery and goal-horn
preview**, followed by 7B custom WAV files and 7C independent team-color
case LED wiring/animations. Validate actual S3 revision and codec/connector
before selecting GPIO; do **not** copy the CYD rear-LED pin map.
