# Phase 6B.4 — Waveshare S3 Arena Broadcast performance & stability

## Scope

This phase is **Waveshare ESP32-S3-Touch-LCD-3.5 only**, with the
**480x320** Arena Live page controlled by `GLOWDECK_S3_PHASE6_LIVE=1`.
No NHL endpoint, status inference, PP ledger, WLED, rear LED, Fire TV,
original 320x240 CYD, or follow/arm policy is changed.

## Partial update optimization

Previously an unchanged game poll, an isolated clock/score change, or an
isolated shot/status change could repaint and transfer *both* readout
regions. Phase 6B.4 remembers a tiny two-region, fixed-width hash
snapshot of the pixels' underlying game fields. No additional sprite,
full-frame staging buffer, heap-owned per-frame record, or JPEG/PNG decode
is introduced.

- **Top** `(144,43,192,160)`: period, clock, game state, headline,
  score/VS, final winner cue, Stats hint
- **Bottom** `(48,215,384,53)`: real SOG, verified PP and pulled-goalie
  data, pregame record cache, intermission/winner information
- **Same game, top only:** restore top rectangle from existing PSRAM
  arena cache, draw only top text, push only top rectangle over SPI
- **Same game, bottom only:** same behavior for bottom; explicitly reset
  font datum to `MC_DATUM` so a status-only update cannot misalign text
- **Same game, both changed:** restore/push both, never the entire screen
- **No changed fields:** skip both rendering and SPI transfers
- **New game:** draw the **full scene**, including official logos,
  team colors, footer, follow star and both readouts

The keys include actual NHL game ID, phase, score, period/clock, live
shots, verified PP/EN freshness signature, and only that game's
pregame cached records. Expiring an old PP/EN label after 45 seconds
therefore changes only the bottom region while no new NHL API request
is needed.

The unchanged fallback `GLOWDECK_S3_PHASE6_LIVE=0` still shows the
Phase 5 native S3 scoreboard.

### SPI transfer budget (RGB565 pixel bytes)

| Transfer | Before 6B.4 | After 6B.4 |
|---|---:|---:|
| Same-game clock/score only | 102,144 bytes | 61,440 bytes |
| Same-game SOG/PP/EN only | 102,144 bytes | 40,704 bytes |
| Both changed | 102,144 bytes | 102,144 bytes |
| No changed fields | 102,144 bytes if scheduled | 0 bytes |
| Game change or event recovery | 307,200 bytes | 307,200 bytes |

Numbers are estimates for raw RGB565 data, not measured Wi-Fi/SPI
throughput and exclude panel command overhead.

## Overlay recovery and memory

A GOAL/PP/FINAL overlay and 320x240 unscaled GIF still occupy the
same dedicated drawing path and have priority over readout refreshes.
On completion or dismissal, `needsDraw=true` forces a complete restore
of the exact previous LIVE/Stats/Standings/Setup page before further
partial updates. A carousel swipe or game transition similarly performs
a full scene paint. The immutable arena background is still inflated
**once** into PSRAM, and the single native TFT_eSprite remains the only
foreground framebuffer.

A once-per-minute (when frames are presented) serial performance
summary reports full/partial/skipped frames, estimated RGB565 bytes,
worst observed present time and free PSRAM:

```
[S3 6B.4] full=... partial=... skipped=... SPI~...B maxTransfer=...ms freePSRAM=...B
```

## Tests

The new `tests/test_s3_broadcast_frame.cpp` exercises the field hash,
top-only, bottom-only, combined, identical, and changed-game masks plus
raw RGB565 byte counts. The new source-level contract
`tests/test_s3_phase6b4_contract.py` protects independent redraws,
existing full-scene event recovery, the fallback and CYD isolation.
GitHub Actions runs both tests, the inherited 6B.1–6B.3 regression
suite, and separate `pio run -e cyd` and
`pio run -e s3_waveshare_35` builds.

## Physical acceptance checklist (not performed by CI)

1. Flash the **Waveshare S3 3.5-inch** firmware, confirm startup and
   `[S3 Arena]` cache diagnostics without a boot loop
2. Verify a live clock update doesn't blink SOG or team logos; inspect
   the `[S3 6B.4]` partial/byte counters on USB serial
3. Verify a genuine SOG change repaints only the bottom and centers its
   shot numbers; simulate PP/EN expiry (or disconnect Wi-Fi after a
   verified badge) and confirm stale labels clear
4. Swipe rapidly among games, trigger a pregame→live→intermission→final
   transition, and check no residual text, incorrect logo or shifted
   footer on followed/armed slides
5. Trigger GOAL, PP and FINAL previews, and a custom GOAL GIF if present;
   dismiss some manually and let others expire. Confirm the exact
   previous carousel slide and follow state returns cleanly
6. Compare free PSRAM and worst transfer time over several minutes;
   verify NHL polling, WLED and Fire TV behavior remain unchanged

Physical ST7796 color/touch/PSRAM validation is still outstanding
until this firmware runs on the actual Waveshare board.
