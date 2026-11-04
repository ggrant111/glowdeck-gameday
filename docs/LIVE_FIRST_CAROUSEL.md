# GlowDeck GameDay v0.3.6 — Live-first NHL carousel

## New rotation policy

When **one or more NHL games are LIVE or CRIT**, every live matchup receives its own familiar full-screen team-gradient game slide. All other games (upcoming, pregame and completed/final) are compacted into **four-game summary slides**. Four other games fit in one slide, five through eight need two, and so forth. Each row displays team abbreviations with color accents and the scheduled start time or completed score/final state. Tap a row to see that game's details.

During live play, the eight conference and division standings slides are **excluded from rotation** to keep live games front and center. When **no games are live**, the original carousel returns: every game on its own full-screen slide followed by all eight standings slides when enabled. When no games are scheduled, standings can still cycle.

The layout is recomputed from the existing NHL daily score feed, without extra API requests. It switches modes automatically when live play begins or all games finish. The active NHL game ID is retained across reordering and transitions, including when a game moves from full-screen LIVE to a compact FINAL row.

The carousel's visible page remains **independent of the armed game**. NHL polling frequency, CYD internal RGB, WLED goal celebrations, horn, followed games and v0.3.5 event overlays are unaffected by the layout change. An on-screen event may temporarily cover a summary page and returns to that same page after ending.

A small fixed-size index plan handles up to 20 NHL games plus standings; no extra framebuffer, team-logo images, heap-based game copies or external web services are required. Dashboard Display reports the current live/summary rotation counts.

## Example

A day with 3 live games, 6 upcoming and 2 finished games rotates **5 slides total**: 3 live scoreboards plus 2 four-game summary pages. Previously the same date would rotate 11 separate game screens plus 8 standings screens (19 total). After the last game finishes, the carousel automatically returns to 19 original game and standings pages.

## Physical acceptance

1. Install OTA and check GameDay's **Display → Rotation mode** while multiple NHL games are LIVE: counts should match the number of live games and four-up summary pages.
2. Swipe across live scoreboards into the summary: every scheduled/finished game should be represented exactly once with the correct team and game time/final score. Tap a row to see matchup stats.
3. Verify that standings pages do not rotate **during live NHL games**, and automatically return when no games remain live.
4. Arm one game and browse other slides. A goal should still trigger the rear CYD LED, selected WLED devices and event overlay, returning to the previously displayed slide afterward.
5. Check carousel page identity after NHL start/end transitions and score updates, and confirm there is no flicker caused by stale partial game redraws on summary pages.

The build includes native C++ regression tests covering live classification, up to four summary rows, slide totals, wrapping, NHL game-ID restoration, quiet-days and no-game-day fallback. Physical CYD testing remains necessary.