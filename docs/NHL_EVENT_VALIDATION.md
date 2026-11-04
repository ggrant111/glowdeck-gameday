# GlowDeck GameDay v0.3.7 — Accurate NHL events and PP / EN status

## Official ANA @ VAN audit — November 3, 2026

The NHL's official [play-by-play](https://api-web.nhle.com/v1/gamecenter/2026020246/play-by-play) and [landing summary](https://api-web.nhle.com/v1/gamecenter/2026020246/landing) confirm a 5–4 Anaheim win.

Third-period verified timeline:

| Game clock elapsed | NHL event |
| --- | --- |
| 04:11 | ANA goal, score 5–2 |
| 04:37 | ANA roughing minor plus coincidental fighting majors |
| 05:30 | VAN power-play goal, score 5–3; minor ends |
| 09:34 | VAN even-strength goal, score 5–4 |
| 20:00 | Game final, ANA 5–4 |

No other third-period penalties occurred after 04:37. Late official play-by-play carries **1560**, which decodes to **Vancouver with six skaters and its goalie pulled**. It is an empty-net situation, not a PP. The former unvalidated 6-on-5 detection caused repeated VAN PP events in the device's event log.

## New evidence-first algorithm

The gamecenter **/landing** summary has penalty period, time, duration, offending team, type and a separate list of confirmed PP goals. The firmware fetches these records through a small filtered HTTPS stream for an armed, followed or currently displayed live matchup. A bounded per-game ledger handles minors and major non-fighting penalties, coincidental calls and cancellation of a minor after an official PP goal.

A power play only becomes an active **PP** event when the current NHL feed explicitly labels the correct team PP **and** the penalty ledger corroborates that team's advantage at the game's live clock. Raw 6-on-5 strength alone never generates a power play. Missing, stale, mismatched or intermission data means no new PP alert, pulse, overlay or WLED command.

This conservative approach may miss certain exceptional penalties rather than fabricate events. The official NHL API can also lag broadcast playback.

## Live-game screen

Two small team badges are placed near the team abbreviations, without creating another carousel slide.

- **PP** — official power play confirmed for that team
- **EN** — that team's own goalie is pulled
- **PP+EN** — both are true
- No badge when not in progress or data is unconfirmed

Left is away, right is home. Empty-net detection uses explicit goalie-pulled signals, six reported skaters, or explicit EN/extra-attacker labels from the NHL live situation. It is distinct from the penalty ledger. Both badges update on partial score redraws. The web game API also exposes the verified PP side and empty-net side.

## Testing and memory

Native CI tests verify 1560/0651 goalie-pull codes, the 1451/1541 true manpower situations, the real ANA-VAN 04:37 penalty and 05:30 power-play goal, no false PP at 09:34, event logging, and independent PP/EN status. The network audit script **scripts/audit_nhl_pbp.py** remains available for a manual recheck; CI event-policy tests do not need live network access.

No full-screen pixel buffer, new image assets, or additional screens are added. The landing summary is streamed through an ArduinoJson filter and each game has a fixed-capacity penalty ledger. Only one additional filtered HTTPS verification request occurs per interval, favoring the currently visible or armed matchups.

## Acceptance

1. During an actual PP, the correct team's badge reads PP and the CYD rear LED, overlay and opted-in WLED behavior agree.
2. The PP badge and pulse end when the underlying penalty expires or a PP goal cancels a minor. A PP must not restart from a temporary feed glitch.
3. When a goalie is pulled, that team's badge reads EN but no PP unless a separate active penalty is independently verified.
4. During 5v5, intermissions or uncertain NHL situations, no unsupported badges should appear.
5. Confirm score polling, event log, goal alerts, carousel, and OTA still work. Monitor free heap and loopTask stack watermark for stability.

**Caveat:** HTTP command acceptance and league feed updates are not optical or frame-perfect confirmations. Real-game hardware validation is still needed.
