# Power-Play Tracker 2.0 — NHL Session State, LEDs & WLED

## Why this version exists

The previous /score/{date} parser reset g.ppSide on every NHL update.
A missing NHL `situation`, a televised timeout or a stale landing fetch could
silence the PP badge and LEDs while the penalty was still running. Logging
had a separate debouncer, so the event log disagreed with physical output.

PP2 replaces these independent decisions with **one game-ID-scoped Watch**
(`include/NhlPowerPlayTracker2.h`), shared by S3/CYD PP badges, CYD rear
RGB, S3 case LEDs, WLED PP dispatch, the live Activity log, and the local API.

## States, evidence and transitions

- **IDLE:** confirmed even strength or no current session.
- **VERIFYING:** active penalty or a game situation not yet corroborated.
  No new PP sound or hardware celebration is generated.
- **ACTIVE:** official NHL manpower/PP label corroborated by an active NHL
  gamecenter/landing penalty for the same side. Emits one start transition.
- **HELD:** same official serving penalty but the NHL situation label
  temporarily disappeared, or evidence is incomplete. Keeps the same session
  ID and PP output within a bounded grace; no duplicate sound/overlay.
- **INTERMISSION:** session preserved, PP pulsing stopped, CYD/S3 display
  `PP NEXT`; no PP_END event. Carryover resumes on the same session.
- **UNKNOWN:** trusted evidence has been absent for 120 seconds. LEDs
  release to their prior state and badge disappears, but no fictitious
  NHL PP_END event is emitted. Reverification may resume same session.

A confirmed official PP goal on a releasable minor, expiration at the game
clock, or at least two independent end samples over 10 seconds terminates
the PP. Offsetting calls, fighting-only majors, pulled goalies, and delayed
extra attackers must not produce false PP starts.

Regular-season period 4 is 5 minutes (300s). Playoff OT remains 20 minutes.
A minor starting in regulation can carry into OT while retaining its NHL
period-based elapsed offset.

The PP start still requires two independent sources: an NHL PP label or
valid two-goalie 5v4/4v3 (a *candidate*) **and** the official landing-summary
penalty ledger. A raw 6v5 never creates a PP by itself. A short-lived
missing situation can no longer cancel a previously established session.

## Per-game identity and polling fairness

NHL game ID keys the Watch and penalty ledger; it is not tied to carousel
position or the arm index. Within the game, the session serial increments
only when a truly new confirmed PP follows a confirmed end. A stable penalty
signature (team, time, duration, source ordinal) supports debugging; a
continued manpower advantage is one session even if another overlapping
penalty changes its anchor.

The gamecenter/landing fetch remains at most **one bounded HTTPS request
every 20 seconds**, with an age-weighted queue to stop a continuously
visible game from starving the NHL evidence for other armed/followed games.
A fetched landing situation may supplement a missing scoreboard situation.
No new always-on play-by-play requests or unfiltered JSON payloads.

## Outputs

- CYD rear RGB, S3 external case strips, WLED, and screen use
  `syncPowerPlayOutput` from the same session. Intermission suppresses
  physical pulsing without wiping the underlying opportunity.
- WLED remains owned by the highest-priority eligible armed PP; another
  game's PP cannot be stopped accidentally by a PP_END overlay.
- The manual Stop/restore suppresses only the **current session serial**,
  not every future PP in that game. A TV timeout cannot clear suppression.
- `POWER_PLAY_START` and `POWER_PLAY_END` Activity records are emitted
  once from the tracker transition queue. A resume does not replay an audio
  clip, send another overlay or create a duplicate NHL start event.
- Event ownership is deliberately different at each edge: `POWER_PLAY_START`
  names the team **with the advantage**, whereas `POWER_PLAY_END` names the
  team **that was shorthanded and returns to full strength**. Both CYD and
  Waveshare S3 use that team's logo/colors/message. The Activity `team`
  field and PP2 serial line match the screen; no NHL eligibility or lights
  are changed by this presentation correction.
- If NHL network evidence is stale for more than 120s, output is safely
  released, while the true session identity can survive reconnect.

## Diagnostics and acceptance

The Activity page contains a `Power-Play Tracker 2.0` card showing the
game ID, session, side, phase, reason, penalty key/end clock, raw NHL label,
ledger side/age and WLED owner. The same fields are available in the local
authenticated GET `/api/pp-tracker` endpoint and compact `[PP2]` serial
transition lines. Physical WLED output cannot be optically verified by
these software signals.

Host tests:
```sh
g++ -std=c++11 -Wall -Wextra -Werror -Iinclude tests/test_nhl_powerplay_tracker2.cpp -o /tmp/glowdeck-pp2
/tmp/glowdeck-pp2
python tests/test_power_play_tracker2_contract.py
```

Real hardware regression scenarios: penalty starts while TV timeout is
coming, NHL situation label vanishes but penalties remain active, carryover
across a period break, PP goal ends minor but not a major, back-to-back
different penalties for the same team, simultaneous armed PP games, user
manual stop and subsequent new PP, dropped Wi-Fi, WLED restore ownership,
and original goal/final priority, sound and DDP behavior.

No changes to NHL arm/follow eligibility or the score/goal celebration
rules. Builds and host tests cannot substitute for live NHL/game hardware
validation.
