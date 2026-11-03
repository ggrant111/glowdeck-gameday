# v0.2.4 — Automatic NHL day rollover

## Why yesterday stayed on the CYD

The prior firmware requested `https://api-web.nhle.com/v1/score/now` indefinitely. On the morning of November 3, 2026, the league's own redirect still returned `/v1/score/2026-11-02` with four completed games. The CYD was correctly displaying that stale endpoint.

## New behavior

1. When Wi-Fi connects, the ESP32 starts nonblocking NTP synchronization using `pool.ntp.org` and `time.nist.gov`.
2. Every **5 seconds**, it checks its calendar date in the configured Display timezone (ET, CT, MT, PT, or AZ).
3. As the local date changes at midnight, it immediately retires the previous day's carousel and queues a fresh scoreboard download.
4. The NHL request is now `/v1/score/YYYY-MM-DD` **with the actual local date**, never the ambiguous `/now` alias once NTP is available.
5. Empty `games: []` is a valid daily schedule. The display says **No NHL games scheduled** instead of continuing to show yesterday's matchups.
6. Failed requests retain a clear error/retry state. The device continues trying on its ordinary schedule; a new day doesn't require rebooting or web interaction.
7. Previous-day team statistics are cleared when the date rolls over.
8. The previously armed game is cleared when its game ID isn't part of the new calendar day. This avoids accidentally celebrating a goal based on old game scores. The user can arm a new game from GAMES or STATS.

If NTP has not synchronized yet, the device temporarily falls back to the NHL `/now` endpoint. When the clock becomes valid it automatically switches to the explicit local-date URL. This design ensures the initial startup remains nonblocking.

## Web dashboard diagnostics

**Display** shows the expected local game date, fetched scoreboard date, and network-clock synchronization state. The **Refresh list** control on **Games** now requests an actual NHL re-fetch instead of merely reloading locally cached web data.

If the clock remains unsynchronized, check the home network's access to NTP servers. The device continues retrying while connected.

## Regression testing

The CI workflow now compiles and executes `tests/test_day_rollover.cpp` before building the ESP32 firmware. It verifies:
- November 2 at **11:59:59 PM Central**;
- transition to **November 3 at 12:00 AM Central**;
- the corresponding **Mountain** timezone difference;
- rejection of an unsynchronized epoch-zero clock.

**Physical overnight test:** Set a valid time zone under Display, confirm `clockSynced = true` in the dashboard, keep the CYD powered through midnight, and check that the date and schedule update without a reboot. For a quicker check after flashing, the device should immediately display today's schedule and today's date rather than yesterday's four completed games.

## OTA

Use the successful GitHub Actions firmware artifact, extract `firmware.bin`, and upload through the CYD's existing Update tab. Wi-Fi, touchscreen inversion, WLED settings and other saved Preferences remain intact.
