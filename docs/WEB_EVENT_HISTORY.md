# GlowDeck GameDay v0.3.3 — Web-only game event & celebration history

## Where it appears

The CYD-hosted **web dashboard** now has a **Recent game events** card and a dedicated **Event Log** tab. The physical 320x240 CYD, its game carousel, conference/division standings and touch controls are unchanged.

Each entry identifies:
- When the firmware *observed* an NHL event (local browser date/time when the NTP clock was synchronized, or uptime otherwise).
- Away/home matchup, score, relevant team, period and event type (score increase, PP started/ended, puck drop or final result).
- CYD's built-in rear LED: **Triggered**, **Scheduled** (PP eligible but hasn't been selected by the single rear RGB output), **Disabled**, **Not followed** or **Not applicable**.
- Each of up to four WLED controllers/attached strips separately: **Not selected**, **Waiting**, **Command accepted**, **Snapshot failed**, **Command failed**, **Restored**, **Restore retry**, **Restore failed**, **Superseded** or **Not triggered**. A separate HTTP-acknowledged flag remains visible after restoration.

**Important:** "Triggered" for the onboard LED means firmware scheduled/wrote the LED effect; the board has no sensor verifying light output. "Command accepted" for WLED means its HTTP API returned 2xx; this is **not optical confirmation** that a connected LED strip illuminated. The firmware cannot independently report a separately wired strip that is not controlled by its WLED device.

## Scope and memory

- Fixed circular buffer: the **last 64 events since the device last rebooted** (about a few KB of static RAM). Oldest entries are overwritten automatically.
- No LittleFS/NVS/flash logging, frequent writes or per-event firmware allocations.
- History is *not* retained after reboot or power loss; the web dashboard explicitly states this.
- No historical NHL API backfill: the first received score establishes a baseline. Goal records reflect **observed score increases** (and indicate when a jump exceeded one goal), not necessarily the NHL's original wall-clock goal time.
- Power-play start/end entries require actual NHL situation data; missing feeds never fabricate power-play ends.
- WLED event status is updated as controller acknowledgements and subsequent restoration attempts occur. Results for older entries can show **Superseded** if another celebration starts before restoration.
- Test buttons in the WLED and rear LED tabs produce distinct **TEST** entries.
- The browser reads only 12 entries per page; a 5-entry recent summary appears on Dashboard. Filters: all, goals, power plays, start/final, manual tests.
- Event log is authenticated using the existing device admin login. **Clear session log** is a deliberate, confirmed web action.

## Web API

- \`GET /api/events?category=all&offset=0&limit=12\`: JSON with \`events[]\`, \`total\`, \`capacity\`, \`sessionOnly\`, \`timeSynced\`.
- \`POST /api/events/clear\`: clears session history on explicit administrator request.
- Each event includes timestamp, uptime, game ID, team codes, score, period, event type, onboard status, and \`devices[]\` objects with \`slot\`, \`result\` and \`accepted\`.
- All requests require the current Basic Auth credentials; no public endpoint or external logging service.

## Acceptance test on physical ESP32

1. Update firmware and open Dashboard → Recent game events; confirm the view loads and initially says no events recorded.
2. Use WLED tab → Home goal preview with two opted-in WLEDs. Event Log → Manual tests should show **TEST · Goal preview**, the selected game score, WLED 1/2 "Command accepted" when the controllers respond, WLED 3/4 "Not triggered" (or Not selected). Once the original scenes are restored, those controllers should read **Restored**, with \`accepted=true\`.
3. Temporarily disconnect one selected WLED and repeat preview. Only that device should report **Snapshot failed** or **Command failed**, while the other selected WLED still receives the command.
4. Use CYD LED tab → Flash home goal; confirm the TEST entry shows internal LED **Triggered**. This test does not send WLED commands.
5. During an armed live NHL game, confirm a score increase creates a Goal entry with the reported score, CYD LED state and separate WLED controller acknowledgements. When a goal occurs for another game that is not followed, it should show **Not followed / Not triggered** rather than inventing a celebration.
6. Switch to Power plays filter and verify that only real NHL live situation changes populate entries, not cumulative game stats or unknown PP data.
7. Restart the CYD: event history will be empty (intentional RAM-only session design). Confirm that Wi-Fi/WLED settings, OLED/TFT carousel, NHL score polling and OTA still work.

GitHub Actions compiles tests for the event log's bounded ring, overwrite behavior, filters, acknowledgement retention and JS syntax, and builds the ESP32 OTA binary. Hardware verification is still required.
