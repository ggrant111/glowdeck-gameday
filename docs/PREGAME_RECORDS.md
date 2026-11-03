# v0.2.7 — Pregame records and automatic live stats

## User experience on the 320x240 CYD

Tap a game in the full-screen daily carousel:

**Before puck drop (FUT/PRE)**, the matchup's detail view shows:

| Metric | Away team | Home team |
|---|---|---|
| SEASON | Season W-L-OT record | Season W-L-OT record |
| VS CONF | W-L-OT in completed regular-season games against other teams in its conference | Same for the home team |
| CONF RANK | Current rank within its conference, e.g., #3 | Current rank |
| POINTS | Current standings points | Current standings points |

These are **not** the same statistic: "conference rank" is official standings position, while "VS CONF" is the actual W-L-OT result against East/West conference opponents. Do not substitute one for the other.

The first screen paints league standings quickly; **VS CONF** initially displays \`--\` and fills in as the two team season schedules are processed. The optional schedule downloads run one at a time, only when the pregame detail view is open (or a web preview has requested them), and defer during WLED restoration or rear goal effects. Results are cached for 15 minutes to minimize repeated downloads.

**During LIVE/CRIT and after FINAL/OFF**, the same detail page uses existing NHL game stats such as shots, faceoffs, power play, penalty minutes, and hits. When the game changes from PRE to LIVE while details are open, the firmware detects the state change and replaces the season preview with live game statistics on its next successful poll, bypassing the old pregame-cache timeout. Existing fast 10-second polling for an armed/followed live game remains in place.

**Carousel footer:** Upcoming matchups now show the two team season W-L-OT records in separate cells, with "W-L-OT" centered. This avoids the old \`REC 11-2-2|9-5-0\` text overflow on the 320px display.

## Source APIs and data integrity

- \`GET /v1/standings/YYYY-MM-DD\` provides league-wide wins, losses, overtime losses, points, and \`conferenceSequence\` rank. Fetch once across all pregame matchup previews and cache for 15 minutes.
- \`GET /v1/club-schedule-season/{TEAM}/{YYYYYYYY}\` provides each team's complete regular season and postseason schedule. Apply an ArduinoJson streaming filter and count **completed regular-season only** games for that club where the opponent is in the same conference.
- A win adds W; a regulation loss adds L; an OT or shootout loss adds OTL. Games outside the conference, in progress, scheduled, preseason, or playoffs do not count.
- The NHL schedule payload is not kept in full RAM: parsed fields are filtered and temporary after computing a small cached \`W-L-OT\` string.
- 2026-27 conference alignment is encoded in \`include/ConferenceRecords.h\`. If future realignment occurs, update that list.
- Missing/failed NHL data shows \`--\`, **not zeroes**, until it can be verified. Valid zero-game records display \`0-0-0\`.

## Browser parity

The self-hosted Games page labels upcoming details **Preview** rather than Stats. It shows the same Season / Vs Conf / Conf Rank / Points rows, requesting refreshed conference results as they finish loading. Live games retain **Live stats**. 

## Regression and hardware verification

GitHub CI runs the native C++ \`test_conference_records.cpp\` (opponent conference filtering, REG/OT/SO outcomes, exclusions) and \`test_detail_mode.cpp\` (PRE/FUT -> LIVE switch), then builds the ESP32 firmware and OTA artifact.

On the CYD: open an upcoming game such as Colorado at Columbus. You should see each team's season record and conference rank immediately when NHL standings are available. VS CONF should populate after brief schedule processing. Leave that game's detail page open until puck drop: the view should switch to live stats automatically. Confirm carousel rotation, touchscreen back navigation, goal lights, and OTA still work. Real device verification is still required.

Relevant community documentation: https://github.com/pseudo-r/Public-NHL-API/blob/main/docs/web-api/standings.md , https://github.com/pseudo-r/Public-NHL-API/blob/main/docs/web-api/schedule.md
