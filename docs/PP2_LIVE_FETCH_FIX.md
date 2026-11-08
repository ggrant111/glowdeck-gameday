# PP2: Live clock/stats and PP_END events during WLED pulsing

## Root cause

WLED stores the pre-celebration controller snapshot while a power-play
effect is active, with `restoring[i]=true` and
`restoreAt[i]=UINT32_MAX`. This is an **active, indefinite lease**,
not a scheduled restoration.

Before this fix, the main loop treated every active lease as if a WLED
restore were pending:

- The NHL scoreboard used signed `restoreAt-now` with `UINT32_MAX`.
  After sign conversion this could look overdue for the entire PP,
  suppressing the regular 10-second live scoreboard request.
- The 20-second official gamecenter/landing penalty fetch was skipped
  whenever any controller held a lease. No fresh clock/penalty data meant
  PP2 could not verify the end and had no `POWER_PLAY_END` to log.
- The live right-rail stats and standings fetches were similarly blocked.
  The ticker and SOG relied on stalled snapshots rather than new data.

The change introduces `WledRestoreFetchPolicy::dueSoon()` and routes
the scoreboard, penalty, stats and standings gates through a shared helper.
A `UINT32_MAX` lease is not urgent and **does not block NHL updates**.
Actual finite restoration deadlines still take priority if a new HTTPS
request could overlap them. The existing WLED restore and snapshot
ownership code is untouched.

## Expected behavior

- Scoreboard/clock and shots return to the existing **10s cadence for
  followed/priority live games**, or 20s for other live games. These
  are request intervals, not guarantees that the NHL source has changed
  on each request.
- Official PP evidence continues checking at its existing up-to-20s
  request cadence while pulsing is active, subject to other eligible
  games sharing the landing request.
- Right-rail stats resume normal refresh (currently up to 60s cache TTL
  for live games).
- A verified PP end drains its transition to the existing Activity
  `POWER_PLAY_END` row; restoring lights and selecting another armed PP
  remain WLED's separate responsibility.
- Explicitly scheduled WLED restoration, and hardware overload
  protection, retain priority over expensive HTTPS requests.

## Regression tests

```sh
g++ -std=c++11 -Wall -Wextra -Werror -Iinclude tests/test_wled_restore_fetch_policy.cpp -o /tmp/wled-pp-fetch
/tmp/wled-pp-fetch
python tests/test_wled_restore_fetch_contract.py
```

The host test covers the sentinel, near/overdue restore deadlines,
`millis()` wrap, and a verified PP2 end being categorizable under
Activity / Power plays. The source contract confirms every network
scheduler uses the shared policy.

Hardware acceptance: during a live followed game with WLED PP active,
check whether scoreboard `[NHL]` logs continue approximately every
10s, stats still refresh around the normal interval, and the Activity
tab records `Power play ended` when the NHL clock and penalty ledger
verify the end. NHL API delays, network failures and actual restore
requests can still affect observed timing.
