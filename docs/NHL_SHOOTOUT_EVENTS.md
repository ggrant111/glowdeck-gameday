# Shootout Signals — GlowDeck GameDay

NHL shootouts are tracked separately from regulation/overtime goals. The NHL awards a single additional game goal to the shootout winner; it is **not** a separate regulation goal horn trigger. The standard score-delta detector explicitly excludes `periodType == "SO"`.

## Live behavior

| Trigger | On-device event | Opted-in WLED / Hue | CYD rear RGB / S3 case LEDs |
| --- | --- | --- | --- |
| OT transitions to shootout | SHOOTOUT! introduction | Team-color Wipe (3s) | Short flash / dual-team chase |
| Official shootout goal | SO GOAL! | Scoring team's fast Chase (4.5s) | Scoring team flash / chase |
| Official saved shot | SO SAVE! | Defending goalie's Twinkle (2.3s) | Goalie team short flash / chase |
| Official missed/failed shot | SO MISSED | Shooting team's Fade (2.3s) | Shooter team short flash / chase |

When an event belongs to an **armed** game, WLED and Hue may participate according to their independent 16-bit shootout event-category flag (stored in the existing uint8 mask). When **followed**, onboard LED effects operate where enabled. All-live display mode can show a shootout overlay without taking over WLED for an unarmed game.

One filtered NHL `/v1/gamecenter/{gameId}/play-by-play` request is issued no more often than every six seconds *only* while the scoreboard reports a live `SO` game, and not near a WLED restore. Accept only official `plays[*].periodDescriptor.periodType = SO` and types `505` (goal), `506` (save), `507` (miss), `537` (failed attempt), with `details.eventOwnerTeamId` matched to top-level home/away team IDs. Keep a per-game `sortOrder` cursor; no repeated event notifications across successive polls. A reboot in the middle of SO seeds the first cursor without replaying older attempts. An OT-to-SO transition seen on this device can announce attempts accumulated before the next fetch.

Existing NVS settings remain unchanged except profiles that opted into **all four** legacy event categories (mask 15) also opt into shootouts on upgrade. Per-device customized masks remain respected. WLED zone mappings remain in place; SO animations use native JSON and existing snapshot/restore behavior, with DDP falling back to native JSON. Existing customizable five-event scene blobs are preserved.

The dashboard offers a **Shootouts** event filter, per-controller and Hue toggles, and WLED preview buttons for start, goal, save, and miss. The Activity log and serial audit report the unique SO event type. No shootout attempt plays the ordinary goal horn.

## Verification

Run the added C++ policy and Python integration contract tests (both in CI). Then build CYD and S3 firmware with PlatformIO and run preview tests. During a real shootout, check the NHL play-by-play feed, team-side routing for saves, WLED restoration, logs, and screen display. Network request acknowledgment and no compile errors do **not** constitute hardware validation.
