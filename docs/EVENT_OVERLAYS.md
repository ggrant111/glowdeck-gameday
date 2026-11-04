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