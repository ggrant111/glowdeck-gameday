# GlowDeck GameDay — ESP32 CYD edition

**A standalone NHL game-day controller for the ESP32-2432S028R ("Cheap Yellow Display", 2.8" resistive touch).** Pick a game on the touchscreen and use WLED lights to celebrate goals. Configure it from a mobile-friendly website hosted **by the ESP32**.

**Status: v0.3.2 team-color NHL game slide backgrounds.** PlatformIO compilation and NHL logo generation pass in CI. The previous v0.1.1 color correction and web OTA have been confirmed on a physical ESP32 CYD; new carousel and stats features still need on-device validation.

## Touchscreen

- **TODAY:** Full-screen NHL broadcast-style scoreboard with corrected 16-bit RGB565 logos, larger scores, live clock/period and SOG. Swipe left/right to rotate games, tap the matchup for stats, and tap the small top-right menu to reach Games/Setup. No persistent bottom navigation.
- **GAMES:** Choose the matchup that is armed for goal celebrations. Use prev / next to page.
- **SETUP:** IP address, Wi-Fi hotspot credentials, test lights and touch calibration.

The first score after selecting a game is used as a baseline and never replays an earlier goal.

**v0.2.2 display correction:** Fixes horizontal striped team logos seen in physical CYD photos, darkens the electric-blue arena background, and formats faceoff fractions as percentages. Hardware confirmation is still needed after OTA.

**v0.2.3:** Fixed the proven 56px-versus-48px logo row-stride mismatch. The build now extracts official team SVG URLs from the NHL score JSON when available, converts them into on-device assets, and loads PP/PIM/FO stats for carousel games automatically (without requiring a visit to the stats page). See [the carousel guide](docs/CAROUSEL.md).

**v0.2.4:** The CYD synchronizes its clock using NTP, checks the configured local date every five seconds, and fetches the NHL scoreboard at `/v1/score/YYYY-MM-DD` rather than the NHL's delayed `/now` redirect. Yesterday's games automatically expire at midnight; empty schedules display "No NHL games scheduled." The Display tab exposes date/time-sync diagnostics. See [Automatic Day Rollover](docs/DAILY_ROLLOVER.md).

**Carousel does not arm games:** Browsing the daily schedule, manually or automatically, does not change which game triggers goal celebrations. Select a game using ARM GOALS on TODAY, ARM on the stats page, or the GAMES list.

**Display appearance:** The screen is intended to be dark navy with white text and teal controls. If your particular CYD displays a pale blue background and pink buttons, toggle **SETUP → INVERT** on the touchscreen, or use **Setup → Correct inverted CYD display colors** in the web dashboard. This preference survives reboots. Unchanged NHL polls no longer force a full-screen redraw.

## Dynamic team-color game slides (v0.3.2)

Every full-screen **pregame/live/final NHL matchup** now uses a bold **away team → home team** primary-color backdrop instead of the former blue arena field. Team colors fade together through a soft dark center, with subtle mesh texture and neutral charcoal overlays for clear white score/time/stat text. The 320×240 CYD renders the gradient directly as 80 fixed-point RGB565 strips; there is no framebuffer, bitmap download, heavy graphics dependency, or extra heap allocation. The backdrop is only redrawn on game-slide changes, never during quick partial NHL score refreshes. Conference/division standings and settings screens remain neutral.

See [Team Gradient](docs/TEAM_GRADIENT.md) for design decisions and hardware visual verification.

## Multi-device WLED Celebration Engine (v0.3.1)

The CYD now triggers built-in WLED effects on **only the configured, enabled WLED controllers**. In **WLED / Lights**, enter up to four IPs and flag each controller for goals, power plays, puck drop and/or final results. All participating devices receive identical team colors and effect parameters from a single event timeline: goal strobe-to-chase (or select a simpler goal style), live power-play breathing, game-start wipe and win/loss finales. Afterward the CYD restores each device's original WLED state, including segments. A WLED state could not be saved? That device is skipped rather than risking loss of its previous lighting.

The dashboard provides previews for every event, a Stop/Restore control, per-device result status, and a home/away favorite-team perspective for final outcomes. The rear CYD RGB remains independent; existing saved WLED IPs and enabled flags are preserved. WLED JSON commands use `udpn.nn` to suppress unintended WLED UDP broadcast to excluded devices.

**Timing:** Effects match across opted-in controllers, but sequential HTTP is not frame-perfect synchronization; optional UDP/DDP real-time streaming could address that in a future release. See [WLED Celebration Engine](docs/WLED_CELEBRATION_ENGINE.md) for safety notes, configuration and tests.

## Full NHL conference and division standings (v0.3.0)

Swipe through every current-day matchup, followed by **eight additional standings slides**: Eastern and Western conference rankings in two readable 8-team halves each, plus full Atlantic, Metropolitan, Central and Pacific division rankings. The compact CYD tables show official team rank, games played, W-L-OT and points. The armed game's two teams receive a subtle accent. Games can still be armed and monitored while a rankings slide is displayed.

Standings use the NHL's official dated rankings and refresh separately from the faster 10-second armed live-game score polling. The web Display tab can enable/disable all rankings slides (default on) and shows the current carousel slide. The display works on days with no scheduled NHL games.

**Logo contrast correction:** During asset generation, prefer each team's NHL `darkLogo` / `*_dark.svg` artwork, rather than using `*_light.svg` intended for light backgrounds. The TFT screen's global invert setting is unchanged; verify the contrast of previously inverted team logos on hardware.

See [Full league standings](docs/FULL_LEAGUE_STANDINGS.md) for slide sequence, source data, hardware tests and details.

## ESP32 crash / stack canary fix (v0.2.9)

If v0.2.8 immediately reboots after connecting Wi-Fi with `Guru Meditation: Stack canary watchpoint triggered (loopTask)`, update to v0.2.9. The ESP32's default loop task uses an 8KB stack; HTTPS requests and a 2KB stack-allocated JSON filter exhausted headroom. v0.2.9 sets a 16KB loop stack, moves the NHL scoreboard filter to heap memory with capacity checks, moves large pregame standings scratch arrays out of the stack, and prints the free-stack low-water mark around NHL fetches. The web dashboard also shows **Stack headroom (minimum)**. This does not remove existing timeouts, retries, the daily date logic, WLED effects or settings.

**A boot loop may prevent OTA upload.** For recovery, connect the CYD by USB to your computer and use the compiled firmware from the successful Actions run. The app binary starts at flash offset `0x10000`; if the bootloader still selects a previously OTA-updated partition, the OTA metadata at `0xE000` can be cleared to force booting the flashed primary application without wiping NVS settings. See `docs/STACK_RECOVERY.md` for exact Windows commands.

## Reliable NHL scoreboard downloads (v0.2.8)

The NHL JSON reader now permits up to 12 seconds for an HTTPS response and for a stalled streamed read, avoiding `NHL JSON: IncompleteInput` caused by packet gaps under the old 4.5-second HTTP and default short Stream timeout. The filtered JSON allocation also supports busy 16-game dates. Partial, invalid or mismatched responses **never replace** a previously valid game list. Failed fetches retry after 5 seconds (first three failures), then back off to 15/30 seconds, or the normal live-game cadence if faster. When the feed recovers, normal 10/20/30/60-second polling resumes.

The local dashboard now displays retry count/HTTP status, last successful NHL update and effective retry interval. Serial logs include expected HTTP Content-Length, elapsed time and free heap. Slow requests are deferred before WLED restoration to preserve lighting recovery. No extra hardware or reset is required.

## Pregame season and conference records (v0.2.7)

Tap an upcoming game on the CYD to see **season W-L-OT, record against conference opponents, conference rank and standings points** for both teams. The league standings feed supplies season totals and conference rank; versus-conference W-L-OT is calculated from each team's completed regular-season schedule and cached. When the matchup switches to LIVE/CRIT, the detail view automatically changes to **live game stats**, and final games continue showing game stats. Pregame carousels now put each team's W-L-OT in separate bottom cells instead of a clipped combined string. Web Games → Preview mirrors the same information. See [Pregame records](docs/PREGAME_RECORDS.md).

## Priority NHL event detection (v0.2.6)

The ESP32 now polls the daily NHL scoreboard every **10 seconds** when an armed or CYD-LED-followed game is LIVE/CRIT. It polls every **20 seconds** if other NHL games are live, **30 seconds** while an armed game is still pregame, and **60 seconds** during idle periods. One date-scoped API request updates every game in the carousel, while armed/followed games determine priority. Arming a matchup queues an immediate score refresh and establishes a safe baseline, rather than replaying earlier goals.

The CYD LED power-play fallback now refreshes missing or aging situation data at most once every **20 seconds** during followed live games, rather than relying on stale cached state. The dashboard displays its actual **NHL score check** interval. These intervals describe the CYD's requests, not guaranteed delays from an event on television; the NHL API itself may publish changes later.

## Rear RGB LED NHL reactions (v0.2.5)

The built-in active-low RGB LED (GPIO4 red, 16 green, 17 blue) can flash the scoring team's color during goals and pulse during power plays. The existing armed game is always followed for rear RGB events, plus up to four additional NHL games that can be selected in the new **CYD LED** web-dashboard tab. Configure brightness, goal duration, effects, and preview animations in a browser. No separate WLED or speaker is required. Settings persist across reboots. See [Rear RGB LED guide](docs/REAR_RGB_LED.md).

## Self-hosted web dashboard

- **Dashboard:** status, current game, mode and home/away light tests.
- **Games:** arm the goal-alert matchup or inspect each game's team statistics from your phone.
- **Display:** choose automatic rotation, interval (5–30 seconds) and local game time zone.
- **Lights:** set up to four WLED IP addresses with individual enable switches.
- **Setup:** scan Wi-Fi networks, enter Wi-Fi credentials, set brightness, duration, optional GPIO26 speaker horn and admin password.
- **Update:** upload a compiled firmware BIN (OTA-capable partition layout).

No separate Fire TV app, Raspberry Pi, PC server, CDN or cloud account is needed for normal operation. The NHL scoreboard API and your home Wi-Fi do require internet connectivity.

## First-time flash (Windows)

1. Install VS Code and the PlatformIO extension, or PlatformIO CLI.
2. Clone this repo, open it in PlatformIO and connect the **ESP32-2432S028R** by a data-capable USB cable.
3. In a PlatformIO terminal, run:

   \`\`\`powershell
   pio run -e cyd
   pio run -e cyd -t upload --upload-port COM6
   pio device monitor -p COM6 -b 115200
   \`\`\`

4. On the CYD, tap **SETUP**. Connect your phone to Wi-Fi **GlowDeck-XXXXXX** and enter the unique password on the CYD screen.
5. In your phone browser open **http://192.168.4.1** (plain HTTP, not HTTPS). For web login use **admin** and the same password shown on the CYD, unless you changed the admin password previously.
6. Under **Setup**, join your home Wi-Fi; under **Lights**, add your WLED controller IP(s), enable them and save. For a stable setup, reserve those IPs in your router.
7. Under **Games**, select an NHL game; press **Test home color** and check that your WLED lights return to their previous state.

The ESP32 keeps its setup hotspot active so you can regain access even if home Wi-Fi credentials change. Once connected to home Wi-Fi, the CYD's **SETUP** screen shows its LAN IP.

## Goal celebration behavior

The CYD checks the NHL scoreboard approximately every 20 seconds after a game is selected. It detects increases in the selected game's home/away score during a live game. When the mode matches the scoring team, it saves the JSON state of each enabled WLED controller, applies a team color to **segment 0**, and restores the saved state after the selected duration (2–30 seconds, default 8). If a controller's state cannot be read, the device will not change it.

Public scoreboard reporting may lag behind broadcasts. This is not video or audio capture.

## Implemented vs planned

| Feature | Status |
| --- | --- |
| Daily game carousel, official embedded logos, scores, live clock, scheduled times | Built, physical validation pending |
| Touch-to-open team stats, game comparison, pregame records | Built, physical validation pending |
| Configurable rotation (5–30 seconds), timezone ET/CT/MT/PT/AZ | Built, physical validation pending |
| Touchscreen game choice, team mode, light test | Implemented |
| Self-hosted mobile dashboard and Wi-Fi setup hotspot | Implemented |
| NHL public API scores and goal detection | Implemented |
| Four WLED IPv4 devices / snapshot and restore | Implemented |
| Duration and brightness settings | Implemented |
| Built-in speaker horn tone on GPIO26 | Implemented; optional attached speaker |
| Saved Wi-Fi and settings | Implemented |
| Browser firmware upload | Implemented; requires OTA partition |
| WLED discovery and segment routing | Planned |
| Per-team effect/palette settings | Planned |
| Custom WAV audio via microSD | Planned |
| Additional sports | Planned |
| NHL HTTPS certificate verification | Planned |

## Project structure

- **platformio.ini** — one supported CYD board target and pin configuration.
- **scripts/generate_logos.py** — generate compact 48x48 team-logo sprites from NHL SVG assets. Requires Pillow and CairoSVG when building locally. GitHub Actions runs this automatically.
- **docs/CAROUSEL.md** — full daily carousel and team-stats guide, controls, time zones and validation steps.
- **src/main.cpp** — firmware, NHL polling, WLED control, touch UI, web API and firmware update.
- **include/WebUi.h** — built-in responsive browser dashboard.
- **docs/HARDWARE.md** — board pins and architecture.
- **docs/TROUBLESHOOTING.md** — boot loop, COM6, touch, Wi-Fi and WLED diagnosis.
- **docs/SECURITY.md** — network security boundaries.
- **.github/workflows/build-firmware.yml** — compile check and downloadable firmware.

For local builds, first run `python -m pip install Pillow==11.3.0 CairoSVG==2.8.2` and `python scripts/generate_logos.py` before `pio run -e cyd`.

For firmware downloads, open **Actions → Build CYD firmware** in this repository, select a **successful** run and download the artifact named **glowdeck-gameday-cyd-firmware**. It contains the PlatformIO-generated firmware.bin; use USB for the initial flash.

Hardware information: [2.8-inch CYD pinout](https://randomnerdtutorials.com/esp32-cheap-yellow-display-cyd-pinout-esp32-2432s028r/). API references: [WLED JSON](https://kno.wled.ge/interfaces/json-api/) and [NHL scoreboard](https://api-web.nhle.com/v1/score/now).
