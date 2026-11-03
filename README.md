# GlowDeck GameDay — ESP32 CYD edition

**A standalone NHL game-day controller for the ESP32-2432S028R ("Cheap Yellow Display", 2.8" resistive touch).** Pick a game on the touchscreen and use WLED lights to celebrate goals. Configure it from a mobile-friendly website hosted **by the ESP32**.

**Status: v0.2.3 NHL logo and auto-stats fix.** PlatformIO compilation and NHL logo generation pass in CI. The previous v0.1.1 color correction and web OTA have been confirmed on a physical ESP32 CYD; new carousel and stats features still need on-device validation.

## Touchscreen

- **TODAY:** Full-screen NHL broadcast-style scoreboard with corrected 16-bit RGB565 logos, larger scores, live clock/period and SOG. Swipe left/right to rotate games, tap the matchup for stats, and tap the small top-right menu to reach Games/Setup. No persistent bottom navigation.
- **GAMES:** Choose the matchup that is armed for goal celebrations. Use prev / next to page.
- **SETUP:** IP address, Wi-Fi hotspot credentials, test lights and touch calibration.

The first score after selecting a game is used as a baseline and never replays an earlier goal.

**v0.2.2 display correction:** Fixes horizontal striped team logos seen in physical CYD photos, darkens the electric-blue arena background, and formats faceoff fractions as percentages. Hardware confirmation is still needed after OTA.

**v0.2.3:** Fixed the proven 56px-versus-48px logo row-stride mismatch. The build now extracts official team SVG URLs from the NHL score JSON when available, converts them into on-device assets, and loads PP/PIM/FO stats for carousel games automatically (without requiring a visit to the stats page). See [the carousel guide](docs/CAROUSEL.md).

**Carousel does not arm games:** Browsing the daily schedule, manually or automatically, does not change which game triggers goal celebrations. Select a game using ARM GOALS on TODAY, ARM on the stats page, or the GAMES list.

**Display appearance:** The screen is intended to be dark navy with white text and teal controls. If your particular CYD displays a pale blue background and pink buttons, toggle **SETUP → INVERT** on the touchscreen, or use **Setup → Correct inverted CYD display colors** in the web dashboard. This preference survives reboots. Unchanged NHL polls no longer force a full-screen redraw.

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
