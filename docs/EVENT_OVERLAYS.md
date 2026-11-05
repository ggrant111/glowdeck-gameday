# GlowDeck GameDay v0.3.5 — Animated on-screen NHL celebrations

## Scope

NHL event detection and lighting commands now also drive **temporary full-screen 320×240 event overlays** on the ESP32 CYD. Each overlay includes the scoring/advantaged team's embedded 56px official logo, primary team color, a large event title, concise description, current scoreboard score, period and remaining game clock when known. A muted dark center preserves readability. A small progress bar indicates when the CYD will return to the previous screen.

The display is independent of WLED: **no WLED devices are required**. The onboard rear RGB goal flash and PP pulse continue exactly as before. When selected WLED controllers exist, they receive the same NHL event and team color; timing is best-effort because WLED state snapshots/HTTP calls are synchronous, not frame-precise.

## NHL event visuals

| Type | On-screen presentation | Duration / behavior |
|---|---|---|
| Goal | Large **GOAL!**, scoring team/logo and updated score; fast team-color **border** flashes for 1.1 seconds, then steady color | 3–12 seconds, default 8 |
| Power play begins | **POWER PLAY**, team logo and **MAN ADVANTAGE**, slow breathing color perimeter | 4.5 seconds |
| Power play ends | **FULL STRENGTH**, calm breathing perimeter | 2.5 seconds |
| Puck drop | **PUCK DROP**, both matchup abbreviations, team color | 4.5 seconds |
| Final win | **FINAL - WIN**, favored team logo, final score and gentle pulse | 3–12 seconds, default 7 |
| Final loss | **FINAL - LOSS**, favored team logo, final score and softer pulse | 3–12 seconds, default 7 |

Flashing is confined to a **5px border**, not the entire TFT. The web Display tab can turn off goal flashing for a calmer version; all overlays, logos and each event class also have independent switches. The goal and final screen durations are configurable. The video-free animation uses direct TFT primitive drawing, RGB565, integer math and the already embedded logo sprites. No new framebuffer, filesystem assets, Wi-Fi calls or floating-point rendering.

**Eligibility:** A goal/PP overlay is shown for the armed game or one of the additional CYD LED-followed games; unselected games still enter the web Event Log but do not interrupt the screen. Game-start/final overlays are for the **armed game**. The first NHL score/state observation establishes a baseline, so booting mid-game never fabricates a celebration.

## Queue and return behavior

- The display state machine holds **one active event + up to four queued**. Goal has top priority, final second, puck drop third, PP events fourth. A goal can interrupt PP/start, and a final can queue behind a same-poll game-ending goal. Repeated events with the same event-log sequence are ignored.
- A running event freezes carousel rotation and defers optional standings/pregame downloads; **NHL scoreboard polling, WLED state restoration, rear RGB and horn remain active**.
- Only a thin border and progress bar are animated every ~140–240ms. The static score/team logo backdrop is painted once per event change. Network requests may temporarily delay an animation frame; there are no busy loops or timers.
- After the event ends, the CYD redraws whichever display was already open: the same game slide, conference or division ranking, stats, setup or game list. It does not change the armed matchup, carousel slide index or other settings. A tap in the top-right **X** dismisses the event without modifying the underlying screen.
- A midnight date rollover drops any stale event queue.

## Web dashboard & preview

Open **Display → Live event overlays** on the existing local CYD dashboard. Switch on/off overlay events, flashing and logos, select goal/final durations, and tap **Save display settings**. The preferences persist in NVS.

The same section offers **Preview goal, Preview power play, Preview PP end, Preview puck drop, Preview final win, Preview final loss and Dismiss overlay** buttons. These visual-only previews are sent to the CYD over authenticated **POST /api/overlay/preview**. They do **not** activate WLED, the onboard RGB or the horn, so they can be safely tested without any external controller. The existing WLED and rear LED preview endpoints also request the corresponding on-screen overlay, coordinating those manually triggered effects.

Dashboard **GET /api/status** now includes the active overlay, team, pending queue count, remaining duration, and total events displayed in this boot session.

## Testing

GitHub Actions runs pure C++ priority-queue regression tests (goal preemption, final ordering, duplicate suppression, queue bounds, timed return) and a JavaScript control-binding check before compiling the ESP32 OTA artifact.

On the physical CYD:

1. Update over OTA, open the web dashboard's Display tab, and save the default enabled overlay options.
2. Swipe to a **division standings slide**; tap **Preview goal**. You should see the selected matchup's team logo, **GOAL!**, score, a quick colored border flash and a progress stripe. When it finishes, the **same division slide** should return.
3. Test Preview PP and PP end; the animation should breathe rather than strobe. Test puck drop and final.
4. Tap **X** in the top-right during an overlay: it should dismiss the current event (or advance to a queued event) without changing the armed game.
5. Arm a real NHL matchup; wait for an actual scored goal/PP. The event overlay should be triggered by the **same score observation** as the CYD LED and (when selected) WLED controller. Verify the event log still shows accurate device results.
6. Disable **Flash border**, save settings and preview again. Confirm no high-frequency flash.
7. Restart to check NVS preferences and monitor free heap / stack headroom. The ring buffer contains no new flash-write traffic and the LED behavior should remain unchanged.

A successful PlatformIO build does not replace physical touchscreen and live NHL verification.
## v0.4.0 — All live NHL events, favorite teams and one-tap follow

### Display and light policies are intentionally separate

- **All live event display (default):** Goal-score deltas, confirmed power-play
  start/end transitions, newly observed puck drops and final transitions across
  the NHL daily slate can generate CYD screen overlays. A game is eligible even
  when no WLED hardware is configured. Each game has its own score and state
  baseline, keyed by NHL game ID; booting into a game does not replay history.
- **Favorite teams:** On **Favorites & alerts** in the redesigned web companion,
  select any NHL teams, then click **Save favorite teams**. Favorites persist in
  NVS through overnight date rollover. Their live games lead the carousel and
  their overlays outrank other games, whether they are armed or not.
- **Follow game (star):** On a live CYD game slide, tap the star in the top
  header, or click the matching star in **Live games** on the web dashboard.
  A filled amber star means the game is followed. Followed games get stronger
  screen alerts and onboard rear RGB goal/PP behavior. This is limited to
  **four additional** games plus the armed matchup; the follow list resets
  daily. The selected/armed game always stays followed.
- **WLED remains armed-only:** Merely starring a game or choosing a permanent
  favorite team **does not arm a new WLED game**. WLED celebrations and horn
  triggers keep the existing armed-game, side and per-controller event settings.
  The current rear RGB mapping (R=4, G=17, B=16) is unchanged.

### Flood protection and fairness

The screen overlay controller holds one active event and four queued entries.
Priority is favorite matchups, then followed, then other live games; within a
tier, goals outrank final results, starts and verified power plays.

Other-live-game goals show for 3s; favorite goals show 5.5s; followed game
goals retain the configured 3–12s period. Ordinary PP/start/final notices
last 1.6–2.4s and are admitted no more than once every 12s. Ordinary
notifications older than 15s are discarded from the queue, rather than
replaying stale plays after the score has already moved forward.

Power plays are never inferred from an unverified 6-on-5. Because the
firmware verifies official penalty/PP-goal evidence via **one extra**
gamecenter request per 20s, league-wide PP events are best effort and
may arrive later or be missed during crowded slates. Goals are observed
from the one daily NHL score request; there is no extra goal polling
endpoint per game. No additional heap framebuffer was introduced.

### Redesigned embedded companion dashboard

The self-hosted, no-cloud interface has desktop side navigation and a
horizontal mobile navigation rail with improved focus targets. The new
sections are **Overview**, **Live games**, **Favorites & alerts**,
**WLED celebrations**, **Rear RGB LED**, **Activity**, **Device settings** and
**Firmware update**. Live games support search and filters (all/live/
favorite-team/followed), one-tap stars, explicit arm controls, and team stats.
Favorite-team selections use the same 32-color palette as actual celebrations.
System status distinguishes league-wide screen notifications from the armed
WLED matchup and rear RGB follows.

The new authenticated endpoint `POST /api/display/favorites` accepts an
unsigned 32-bit palette mask and saves only that preference, preserving other
unsaved settings. `GET /api/status` and `GET /api/config` expose
`favoriteTeams` and `displayAllLiveEvents`. `/api/rear/follow`
remains the shared source for CYD and web star actions.

### Device test after OTA

1. Select two favorite NHL teams and save. Confirm that they appear first
   among live carousel slides and stay selected after reboot.
2. In **Live games**, star a live matchup without arming it; the star should
   appear filled on the CYD and its rear RGB goal alerts should be enabled.
   Confirm WLED does **not** change armed matchup.
3. Set **All live NHL games** as the screen alert source. Verify a scored goal
   in another live matchup shows a short screen-only notification. A favorite
   goal should preempt a nonfavorite notification.
4. Test simultaneous goals/PPs with a busy schedule. Confirm the scoreboard
   returns promptly and the overlay queue remains bounded.
5. Disable league-wide mode to confirm the previous armed/followed-only
   display behavior. Restart and verify all persistent settings.
6. Check actual RGB hardware colors and WLED restore behavior. CI cannot
   confirm physical lights or live API timing.
