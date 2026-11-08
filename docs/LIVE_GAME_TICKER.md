# Live NHL Game Ticker (CYD + Waveshare S3)

## Live-game display

A moving one-line ticker is shown **only on a live NHL game slide**. The
existing full scoreboard, goals, game clock, team logos, PP/empty-net labels,
star/follow controls, overlay priority and WLED/audio eligibility are unchanged.

- **Waveshare S3 480×320**: 480×20px strip at `y=269..288`. It covers the
  previous live page counter, leaving the five bottom navigation icons and
  their touch hit targets unchanged. A marquee step sends **only that region**
  to the ST7796; it never re-sends the 480×320 arena background or logos.
- **CYD 320×240**: replaces the small PP/PIM/FO footer cells while a game is
  live, in `x=10..309, y=206..230`. Non-live footer stats are unchanged.
- During goal/PP overlays, the ticker is not repainted. The normal live slide
  resumes automatically and its cached content is redrawn.
- One **pixel advances every 30 ms** (~33 px/sec) only when the line exceeds
  the available width. UTF-8 boundaries are preserved; text that fits stays still.

## Ticker content and feeds

Order: current **BOX** occupants (team, player and penalty minutes/reason),
then the eight most recent **GOAL** scorers (team, player, period/time and PP
when applicable), followed by **SOG**, verified **POWER PLAY**, and any
already-cached **PP / PIM / FACEOFF** totals.

Player-level goal and penalty data come from the same **filtered NHL
`/v1/gamecenter/{id}/landing` summary request** used for penalty verification.
It is not a second HTTPS stream and does not query another API. The visible
game is prioritized for the existing limited one-request-per-20-seconds fetch.
Only the currently visible game's player details are retained. A game switch
shows `UPDATING` rather than names from a different matchup.

Penalties are shown as currently in the box only when the official landing
snapshot is less than 75 seconds old and the live clock is parseable. Expiry
is based on the NHL's period clock (including five-minute regular-season OT).
Power-play goals release the oldest eligible opposing minor, including the
correct double-minor segment. Penalties without a safe time or team identity
are not labeled as currently active. When the data are not fresh, the ticker
shows `BOX: --` rather than guessing. A fresh, verified empty box is
shown as `BOX: EMPTY`. Misconduct records are not
misidentified as a two-minute PP. NHL player names/box snapshots can lag the
scoreboard by one fetch interval.

The other live stats come from the existing score and right-rail cache.
Missing player names and unavailable numerical stats use placeholders; do
not infer identities, scores or percentages.

## Checks

- `g++ -std=c++11 -Wall -Wextra -Werror -Iinclude tests/test_nhl_live_ticker.cpp -o /tmp/gameday-ticker && /tmp/gameday-ticker`
- `python tests/test_nhl_live_ticker_contract.py`
- Firmware builds: `pio run -e cyd` and `pio run -e s3_waveshare_35`

On device, open a live game that has scoring and penalties. Verify the latest
scorer and active penalized player appear; wait for the penalty to expire,
a PP goal, or intermission and confirm the **BOX** list clears appropriately.
Verify that the scoreboard does not flicker while the ticker moves, and that
tap-to-stats, star/follow, pause, left/right navigation and goal overlays
retain their current behavior. No new setting or saved state is introduced.

## Name and animation robustness (Nov 2026)

The penalty parser accepts NHL `committedByPlayer` as a name object or a
string, falls back to `servedBy` on bench minors, and rejects literal
`null`/`undefined`/`n/a` values. If the feed does not identify a penalized
player, the strip shows `PLAYER TBD` rather than `null`; the offense and
minutes remain visible. The marquee advances in individual pixels at a
30ms cadence rather than jumping one full glyph every 180ms. Scrolling is
still tied to normal loop availability, so an interrupted NHL HTTPS fetch
can temporarily delay a frame; no second render buffer or network task is
introduced.
