# GlowDeck GameDay

**A standalone NHL scoreboard, live game tracker, and team-color celebration controller for ESP32 touch displays.** GameDay displays today's games and standings, follows live events, and can coordinate the screen, onboard audio/LEDs, WLED controllers, and Philips Hue lights. The ESP32 hosts its own mobile-friendly setup dashboard; no Raspberry Pi, separate web server, or Fire TV app is required.

> **New here?** Follow [1. Choose your hardware](#1-choose-your-hardware), [2. Install firmware over USB](#2-install-firmware-over-usb-first-time), and [3. Connect Wi-Fi and open the dashboard](#3-connect-wi-fi-and-open-the-dashboard). External lighting and custom audio are optional. **Do not flash the wrong board's firmware.**

## Install with ESP Web Tools (USB browser installer)

**Fastest route for a brand-new board:** The release workflow produces a board-matched **merged factory image** and an HTTPS [GlowDeck USB installer](https://ggrant111.github.io/glowdeck-gameday/) built with [ESP Web Tools](https://esphome.github.io/esp-web-tools/). **The GitHub Pages installer link is available only after the Pages deployment is enabled and succeeds.** If it is unavailable, use [GitHub Releases](https://github.com/ggrant111/glowdeck-gameday/releases) to download a matching factory image or follow the PlatformIO steps below.

1. Connect the **original CYD** or the **Waveshare S3 ST7796** via USB data cable to a computer running desktop **Chrome or Edge**. This will **not** work from an iPhone or iPad.
2. Open the HTTPS installer, select **Connect & install GameDay**, choose the correct serial port, and approve the browser prompt. The firmware manifest selects **ESP32** or **ESP32-S3** automatically from the detected chip, but cannot detect an incompatible display variant using the same chip.
3. Confirm any installation/erase prompt. **A full factory image overwrites stored NVS settings**, even if you skip a separate full-chip erase. Do not use it to upgrade a configured device.
4. On the touchscreen, open **SETUP**, connect to the displayed `GlowDeck-XXXXXX` Wi-Fi, and open **http://192.168.4.1** to complete home Wi-Fi setup using the steps below.

For devices **already running GameDay**, download the matching **`-ota.bin`** from the release and upload it in **Device → Firmware & Maintenance** instead of choosing a USB factory reinstall. The published release includes both USB factory and OTA files plus checksums. See [release/Pages instructions](docs/WEB_INSTALLER.md) and [GitHub Releases](https://github.com/ggrant111/glowdeck-gameday/releases).

## What you can do

- View NHL games, live scores, game clock, shots on goal, team logos/colors, pregame records, standings, and a live scrolling goal/penalty/stat ticker.
- Follow games from the touchscreen or **Live games** dashboard and separately **arm** the games and team sides eligible for external celebrations.
- Display live goal, verified power-play, game-start, and game-finish event overlays. League-wide display-only events do **not** automatically activate external lights or sounds.
- Trigger team-color celebrations across **up to eight WLED controllers**, with discovery, per-controller event permissions, segment zones, custom scenes, native JSON state restoration, and optional dedicated-controller DDP streaming.
- Integrate a local **Philips Hue Bridge** and up to **four selected Hue lights** with opt-in event controls and restoration.
- Use board-specific LEDs and sound, including the Waveshare S3's ES8311 speaker, custom WAV slots, and optional independently wired case-light strips.
- Inspect the **Activity** event audit, verify power-play state, perform isolated previews in **Test Lab**, and apply subsequent firmware updates through the browser.

The project uses the public NHL API and needs an internet-connected Wi-Fi network for live data. NHL data can lag a broadcast: it is **not** audio/video capture or guaranteed real-time scoring.

## 1. Choose your hardware

| Supported device | Build environment | Display | Audio / local lighting |
| --- | --- | --- | --- |
| **Waveshare ESP32-S3-Touch-LCD-3.5** (ST7796 SPI, FT6336 capacitive touch) | `s3_waveshare_35` | Native **480 × 320 landscape**; 16 MB flash, 8 MB PSRAM | ES8311 speaker/audio; optional two externally powered case LED strips |
| **ESP32-2432S028R** (2.8-inch CYD, ILI9341, XPT2046 resistive touch) | `cyd` | **320 × 240 landscape** | Built-in rear RGB indicator; optional GPIO26 speaker output |

**Compatibility matters:**

- The Waveshare **3.5** ST7796 board is **not** the **3.5B** QSPI/AXS15231B model. Their firmware is not interchangeable.
- The CYD firmware targets the original **ESP32-2432S028R** wiring. Other "CYD" variants may have different controllers/pins.
- The repository's `platformio.ini` currently defaults to **`s3_waveshare_35`**. Always supply `-e s3_waveshare_35` or `-e cyd` explicitly instead of assuming the default.
- You need a **USB data cable** appropriate to your board, a USB-capable computer for the initial flash, and 2.4 GHz Wi-Fi. Some Windows systems also need a USB/serial driver.

**Optional hardware:** WLED-ready controllers/LED strips; Philips Hue Bridge v2 and supported Hue bulbs; a compatible speaker for your board; or the S3 case-light components described in [the wiring guide](docs/S3_PHASE7C_CASE_LEDS.md). Neither WLED nor Hue is needed to run the scoreboard.

For details, see [CYD pin map](docs/HARDWARE.md) and [Waveshare S3 board guide](docs/WAVESHARE_ESP32_S3_3P5.md).

## 2. Install firmware over USB (first time)

**Important:** An app-only `firmware.bin` (or `-ota.bin`) is **not** a complete blank-chip factory flasher. For the first install, either use the ESP Web Tools **factory** image above, or use the matching PlatformIO environment to build **and upload** the bootloader, partition information, and application correctly. Later updates can use browser OTA.

### A. Install the development tools

1. Install [Git](https://git-scm.com/downloads) and [Python 3](https://www.python.org/downloads/) (Python 3.11 is used in CI).
2. Install [Visual Studio Code](https://code.visualstudio.com/) and its [PlatformIO IDE extension](https://platformio.org/install/ide?install=vscode). Run commands below in a **PlatformIO terminal**. Alternatively install the PlatformIO CLI with `python -m pip install platformio==6.1.18`.
3. Clone and open the **repository root** (the folder containing `platformio.ini`).

~~~sh
git clone https://github.com/ggrant111/glowdeck-gameday.git
cd glowdeck-gameday
~~~

### B. Generate embedded assets

The official NHL logo header is generated as part of the repository workflow. Before a local build, install the generator's dependencies and run it:

~~~sh
python -m pip install Pillow==11.3.0 CairoSVG==2.8.2
python scripts/generate_logos.py
~~~

The S3 arena background is regenerated automatically by the S3 PlatformIO pre-build hook. If asset generation fails, fix the Python/dependency error **before** attempting to upload firmware.

### C. Identify your USB port

Plug the board into the computer using a **data-capable** USB cable and run:

~~~sh
pio device list
~~~

On Windows, the port will look like `COM6` (example only). On macOS/Linux it may resemble `/dev/cu.usbserial-...` or `/dev/ttyUSB0`. Use the port reported **for your board** in the commands below. Windows Device Manager can also identify it.

If the Waveshare S3 is not detected, try its **BOOT** button while briefly resetting the device, then release BOOT and check the port again. Never choose a port only because it worked with a previous board.

### D. Build, flash, and monitor — choose ONE board

**Waveshare S3 3.5-inch (ST7796):**

~~~sh
pio run -e s3_waveshare_35
pio run -e s3_waveshare_35 -t upload --upload-port COM6
pio device monitor -p COM6 -b 115200
~~~

**Original 2.8-inch ESP32 CYD:**

~~~sh
pio run -e cyd
pio run -e cyd -t upload --upload-port COM6
pio device monitor -p COM6 -b 115200
~~~

Replace `COM6` with the correct port. In VS Code you can select the appropriate PlatformIO environment and run its **Build** and **Upload** tasks instead. First-time compilation downloads platform/framework libraries and may take longer.

**Successful first boot:** The GameDay display initializes, serial output appears at **115200 baud**, and a Wi-Fi network named `GlowDeck-XXXXXX` becomes available. A successful compiler result **does not** establish that the touchscreen, speaker, LEDs, or network has passed a real hardware test.

### E. Alternative: download a ready-built firmware for later OTA

1. Open the repository's [GitHub Actions runs](https://github.com/ggrant111/glowdeck-gameday/actions/workflows/build-firmware.yml).
2. Open a **successful** run of **Build GlowDeck firmware (CYD + ESP32-S3)** for the commit you want.
3. Download and extract **the matching artifact**:
   - Waveshare S3: `glowdeck-gameday-s3-waveshare-35-firmware`
   - CYD: `glowdeck-gameday-cyd-firmware`
4. Inside the downloaded artifact is `firmware.bin`, used for **subsequent browser OTA**, not as a raw `0x0` factory image.

Do not flash the S3 binary onto a CYD or vice versa. If a device boot-loops and OTA is unavailable, recover with the matching **PlatformIO USB upload** rather than repeatedly retrying OTA.

## 3. Connect Wi-Fi and open the dashboard

1. On the ESP32 display, open **SETUP** and locate the device's **`GlowDeck-XXXXXX`** Wi-Fi hotspot and its **unique password**.
2. Connect a phone or computer to that hotspot. The phone may report "No Internet" while connected; that is expected.
3. Open **http://192.168.4.1** in a browser. Use **HTTP**, not HTTPS. When connecting directly through the device's protected setup hotspot, there is **no additional browser/admin sign-in**.
4. In **Device → Wi-Fi & Access**, select your normal 2.4 GHz home network and enter its password. Save/apply the connection settings.
5. After the ESP32 joins your home network, read its **LAN IP address** from the device setup/status screen. Reconnect your phone to your normal home Wi-Fi, then browse to **http://YOUR-ESP32-IP/**.
6. On the **home LAN**, authenticate as username **`admin`**. The **initial admin password** is the unique setup-hotspot password displayed on the device; change the admin password under device settings.
7. Confirm **Overview** shows a connected device and live NHL information. Check the date, local time zone, and carousel preferences under **Customize → Screen**.

The setup hotspot remains available for recovery when home Wi-Fi credentials change. Keeping the ESP32 and lighting devices on the same trusted local network avoids most discovery and communication failures. Reserving device IP addresses in your router is recommended.

### Dashboard map

| Page | Use it for |
| --- | --- |
| **Overview** | Current game, connection/health, activity summary, shortcuts |
| **Live games** | Daily games, favorites/follow stars, arm/team choices, game details |
| **Customize** | Favorites, Screen, WLED, Philips Hue, Sound, board LEDs |
| **Test Lab** | Isolated screen, audio, WLED, Hue, LEDs, and full-celebration tests |
| **Device** | Wi-Fi, admin/security, firmware upload, restart, diagnostics |
| **Activity** | Recent NHL/preview events, per-output results, audio and power-play diagnostics |

Some controls have **their own Save/Apply action** (for example, Hue, WLED scenes/zones, S3 WAV selection, volume, and case LEDs). Watch the unsaved-change message and confirm the relevant action before navigating away.

## 4. Choose games to display, follow, and celebrate

1. Go to **Live games**, or open **GAMES** on the touchscreen.
2. Tap a game's **star** to **follow** it and prioritize its game information. You can follow multiple games; this does not itself authorize WLED/Hue celebrations.
3. **Arm** the desired matchup for celebrations and choose **Home**, **Away**, or **Both** team eligibility as offered by the device/dashboard. Save the selection.
4. In **Customize → Screen**, set the display rotation and time-zone preferences. The carousel prioritizes live games; game event overlays may show NHL events from across the league.
5. In **Test Lab**, preview a display event without changing real game scores or follow/arm state. Once your audio and light devices are configured, use a combined celebration test to check each output.

**Two different policies:** All-live **screen display** events can appear for unfollowed games; **external lighting and audio** remain governed by the separately saved arm/follow and event-output eligibility settings. The first score sample after arming establishes a baseline; it should not replay an earlier goal. Goal timing depends on when the NHL feed reports the event.

**Power-play Tracker 2.0** verifies penalty evidence by game ID, retains sessions through brief feed gaps and intermissions, and records distinct start/end transitions. A **PP start** names the team on the power play; **PP end** identifies the previously shorthanded team returning to full strength. See [power-play tracker details](docs/POWER_PLAY_TRACKER_2.md) and [live ticker behavior](docs/LIVE_GAME_TICKER.md).

## 5. Connect WLED lights (optional)

GameDay supports **up to eight** independent WLED controllers, each with its own enabled flag and NHL event permissions. WLED normally communicates through its local **JSON API**; optional DDP is described separately below.

### Basic WLED setup

1. Ensure the ESP32 and WLED devices are running on the same accessible LAN and that each WLED controller is online.
2. Open **Customize → WLED → Discover & connect** and choose **Scan Wi-Fi for WLED**.
3. Select a verified result and **Add to GameDay**. Discovered controllers are deliberately **staged disabled**. If discovery finds nothing, use **Manual IPv4 → Verify IP**, or check the WLED device's own web dashboard.
4. For each saved controller, choose the **goal / power play / game start / final** permissions, turn on participation, and select **Save celebration settings**.
5. Open **Test Lab → WLED**. Run home and away team-color tests, confirm colors and brightness, then use **Stop & restore**. Check the controller's previous WLED state returned after the preview.
6. Repeat for additional WLED controllers. Reserve their LAN IPs so saved destinations remain valid.

**Prefer leaving advanced options at their defaults until the basic native-JSON test works.** A successful HTTP command means GameDay sent/received a response, not that it can physically see the LEDs.

### Optional: assign existing WLED segments

Under **Customize → WLED → Saved controllers → Manage segments**, read the controller's existing segment names/IDs/ranges, use **Identify** to flash a single segment temporarily, and assign each to **Home**, **Away**, **Neutral**, or **Unassigned**. Enable segment-based routing and **Save zone assignments**. GameDay manages up to **16 existing segment IDs per controller**; it does **not** create or resize segments or change WLED's saved presets.

- **Home:** responds to matching home-side events.
- **Away:** responds to matching away-side events.
- **Neutral:** responds to either team with the event/team color.
- **Unassigned:** should remain untouched by mapped GameDay effects.

The controller must already be **ON** for segment-scoped previews. After verifying zones, the **Celebration scene builder** supports independent event/zone effect, color, brightness, intensity, speed, and permitted duration overrides. Leave templates on **Inherit** to preserve the default behavior. Save scenes before previewing them.

### Optional: DDP light streaming for dedicated RGB strips

For a compatible dedicated controller, open **Set up light streaming** on its saved WLED card. The guided check verifies full RGB pixel coverage and existing zone roles. Choose **Both teams / entire strip** for a dedicated standalone strip, or keep existing zones; choose **10, 15, or 20 FPS**, confirm exclusive ownership, and enable with or without a **3-second** test. **10 FPS** is recommended for **1,000+ LEDs**.

DDP is **OFF by default**. It can temporarily take over **all LEDs** on that controller, so avoid enabling it for a WLED device also serving TV ambient lighting or unrelated effects. Current firmware supports **up to 4,096 RGB LEDs per controller**, **480-pixel UDP chunks**, and **two simultaneous DDP streams**; it rejects or falls back when ownership or format is unsafe. **Verified power plays continue to use native JSON**, not DDP. Use **Test Lab → WLED** or **Advanced settings & safety** for emergency stop/restore.

More detail: [discovery](docs/WLED_PHASE8A_DISCOVERY.md), [segment mapping](docs/WLED_PHASE8B_SEGMENT_ZONES.md), [scene builder](docs/WLED_PHASE8C_SCENE_BUILDER.md), [DDP safety](docs/WLED_PHASE8D_OPTIONAL_DDP.md), and [guided streaming setup](docs/WLED_PHASE8E_QUICK_SETUP.md).

## 6. Connect Philips Hue (optional)

This requires a **Philips Hue Bridge v2** and compatible light resources connected to the same trusted LAN as GameDay. Bluetooth-only Hue bulbs without a Bridge are not supported.

1. Open **Customize → Philips Hue** and choose **Find bridges**, or enter the Hue Bridge's local IPv4 address manually.
2. Press the **physical button** on the Hue Bridge and immediately select **Pair bridge**. GameDay stores its local application key on the ESP32.
3. Select **Refresh lights** and choose up to **four** individual Hue light resources.
4. Set each light's event permissions and brightness, **Enable Hue**, then choose **Save Hue lighting**.
5. Open **Test Lab → Hue** and test team colors and the power-play pulse. Use **Stop & restore** and verify that the previous light settings return.

Hue lighting is separate from WLED, remains opt-in, and uses local Hue Bridge HTTPS rather than a Hue account or cloud entertainment stream. Non-color Hue bulbs may provide brightness-only effects. If pairing fails with error **101**, press the Bridge button and retry immediately. See [Hue setup, scope, and troubleshooting](docs/PHILIPS_HUE.md).

## 7. Set up onboard sound and case LEDs (optional)

### Waveshare S3 speaker and custom WAV sounds

The compatible Waveshare board uses its **ES8311 I2S audio codec and speaker**. In **Customize → Sound**, enable the onboard goal horn and choose the desired source; use the separate **Apply volume** action to set **30–80%** (default **70%**). In **Test Lab → Audio**, first run the **1 kHz speaker check**, then try the built-in horn and a full celebration.

The S3 sound library supports **three persistent WAV slots**, with browser upload, individual preview, selection and deletion. Audio must be **uncompressed RIFF/WAVE PCM 16-bit**, mono or stereo, **8–48 kHz**, up to **768 KiB** and **10 seconds per slot**. Renaming MP3/AAC to `.wav` will not work. The S3 may separately assign saved clips to goal, verified power-play, game-start, and game-finish events as provided by the audio controls.

If there is no audible sound, inspect **Activity** and serial output for the same event ID, chosen file, expected/sent audio frames, I2S error, amp state, and fallback result. Software reporting that playback completed does **not** prove the speaker produced audible sound.

See [S3 onboard sound](docs/S3_PHASE7A_ONBOARD_AUDIO.md), [custom WAV library](docs/S3_PHASE7B_WAV_LIBRARY.md), and [live sound diagnostics](docs/S3_LIVE_AUDIO_RELIABILITY_AND_EVENT_AUDIT.md).

### Waveshare S3 case LED strips — wiring required

The optional left and right RGB strips are **not** plug-and-play board power outputs. The planned wiring uses **GPIO17 = LEFT/HOME** and **GPIO18 = RIGHT/AWAY**, a **74AHCT125 3.3 V-to-5 V level shifter**, series resistors, **an appropriately fused external regulated 5 V supply**, and **common ground**. Do **not** connect LED-strip 5 V to the S3 GPIO pins, USB power header, or battery connector.

**Leave case-light outputs disabled until the actual enclosure and wiring are built and checked.** The firmware requires explicit wiring confirmation before enabling. Supported case strips are 1–16 RGB LEDs per side; firmware's brightness cap does not replace correctly rated power wiring. Read the [full BOM and wiring diagram](docs/S3_PHASE7C_CASE_LEDS.md) before connecting anything.

### Original CYD rear RGB and optional speaker

The original CYD uses its **onboard rear RGB LED** for team-color events; configure and test it under **Customize → On-device LEDs** and **Test Lab**. The optional CYD speaker uses **GPIO26** and is **not** the same ES8311/custom-WAV hardware as the Waveshare S3. Use [CYD hardware details](docs/HARDWARE.md) for pin-specific restrictions and touch calibration.

## 8. Test the complete installation

After setup, check each subsystem in this order:

1. **Device:** reboot once; confirm screen orientation, working touch, correct local time and home Wi-Fi reconnection.
2. **NHL:** open **Live games** and check the current day's schedule, standings and stats. If no games are scheduled, that is normal.
3. **Following/arming:** select a game, tap its star, then separately arm the intended home/away side; reload and verify saved choices.
4. **WLED:** run a single color preview on each enabled controller; verify individual segments and previous-state restoration before testing DDP.
5. **Hue:** if configured, confirm a light changes and then returns to its previous setting.
6. **Sound/LEDs:** preview onboard speaker and each physical output separately; enable S3 case strips only after wiring verification.
7. **Full celebration:** run **Test Lab → Full celebration**, then inspect **Activity** for screen, audio, rear/case LED, WLED and Hue results. Compare it with serial output at **115200 baud**.
8. **Live NHL event:** when an eligible armed game produces a goal/verified PP, verify actual hardware behavior separately from reported software commands.

**Activity keeps the most recent 64 events in RAM**, so the session history clears on reboot. Look for matching `[EVENT AUDIT]` and `[PP2]` serial entries when investigating power-play or audio behavior.

## 9. Update firmware later (OTA)

1. Download a matching **`firmware.bin`** from a successful GitHub Actions run, or build your board's environment locally to obtain `.pio/build/<environment>/firmware.bin`.
2. Keep the ESP32 powered and connected. Open its local web dashboard on the setup hotspot or authenticated home LAN.
3. Go to **Device → Firmware & Maintenance** and use the firmware upload/OTA option.
4. Select **only the firmware.bin for the exact hardware model**, start upload, and allow the board to reboot.
5. Reopen the dashboard and re-check Wi-Fi, NHL data, sound, lighting selection, and the device's previous settings.

Normal firmware-only OTA generally preserves NVS preferences and S3 LittleFS media (such as custom WAVs/GIF), **provided the partition layout is not changed**. Keep your own copies of uploaded media and a note of your saved settings. Full flash erase, filesystem formatting, or partition-table changes can remove data. **Never choose Erase as a routine OTA troubleshooting step.**

If OTA fails or the board continually resets, reconnect by USB and use the appropriate PlatformIO upload. See [troubleshooting](docs/TROUBLESHOOTING.md) and [stack/boot recovery](docs/STACK_RECOVERY.md).

## 10. Troubleshooting

| Problem | First checks |
| --- | --- |
| No serial/COM port | Use a **data** cable, confirm USB drivers and actual port, check Windows Device Manager or `pio device list`, try S3 BOOT/RESET sequence |
| Wrong colors, pink controls, bad CYD touch | Use **SETUP → INVERT** or the display inversion setting; calibrate CYD touch if needed; verify exact board variant |
| No `GlowDeck-XXXXXX` network | Check stable USB power and 115200-baud serial startup; reboot once and look for crash/reset messages |
| Dashboard won't load | Join the device's setup hotspot, use **http://192.168.4.1** (not HTTPS), or browse the displayed LAN IP from the same network |
| Home dashboard asks for credentials | Login **admin**; initially use the setup-hotspot password, or your changed admin password |
| Games or ticker look stale | Check home Wi-Fi/internet, date/time zone, NHL update status and feed retry errors; scoreboard/penalty summary refreshes are not instantaneous |
| No external celebration | Confirm the game is **armed** with the intended team mode, output is enabled, correct event permissions are saved, and the NHL event is verified |
| WLED not found / wrong segment | Verify `http://WLED-IP/json/info` and `/json/state` on the LAN; use manual IPv4 verification, **Identify**, and the assigned zone roles |
| DDP can't enable or changes wrong LEDs | Use native JSON first; check exclusive RGB strip ownership, continuous segment coverage, current realtime use and the DDP readiness reason |
| Hue not found or not changing | Use same-subnet Bridge IPv4, physical pairing button, **Refresh lights**, explicit enable/save, then Test Lab |
| S3 screen works but no sound | Run **1 kHz** test, confirm speaker/volume, WAV format, saved event assignment, amplifier and I2S diagnostics; check whether it was merely a UI-only preview |
| Firmware upload fails / reboot loop | Verify the correct board's image; retry through USB PlatformIO, capture the **first** serial error, and avoid a full erase unless necessary |

For persistent boot issues, copy the first exception/backtrace from the serial monitor, not just repeating `SW_RESET` lines. The [diagnostics guide](docs/TROUBLESHOOTING.md) gives more recovery steps.

## 11. Security and network boundaries

- The built-in dashboard uses **local HTTP**, not HTTPS; **do not port-forward the ESP32's port 80** to the internet or expose it on an untrusted LAN.
- The setup hotspot is WPA2 password-protected, but people with that password can access device configuration on the hotspot without a second login. Change the home-LAN **admin** password after first setup.
- Wi-Fi/admin credentials and Hue application credentials are stored in on-device flash preferences, **not a dedicated encrypted secrets vault**. Do not publish serial logs containing secrets or share your setup password.
- The NHL HTTPS client currently does not enforce CA identity verification; local Hue Bridge TLS certificates also are not independently pinned/validated. Use a **trusted home LAN** and review [security notes](docs/SECURITY.md) and [Hue security scope](docs/PHILIPS_HUE.md).
- Never upload firmware from an untrusted source. Avoid placing WLED/Hue devices behind an inaccessible guest-network isolation boundary.

## 12. Developer notes, builds, and version history

**Repository layout:**

| Path | Purpose |
| --- | --- |
| `src/main.cpp` | Shared NHL/event core, firmware loop, API endpoints and board-specific runtime |
| `include/WebUi.h` | Embedded web dashboard |
| `include/*.h` | Display, event eligibility, WLED, Hue, ticker, power-play, audio and device policies |
| `platformio.ini` | Explicit `cyd` and `s3_waveshare_35` build environments and pin configuration |
| `scripts/generate_logos.py` | Generate embedded NHL logo assets |
| `scripts/generate_s3_arena.py` | Generate S3 arena background (runs at build time) |
| `tests/` | Host-side C++/Python unit and contract checks |
| `.github/workflows/build-firmware.yml` | Automated tests, both firmware builds, and downloadable OTA artifacts |
| `docs/` | Detailed hardware, feature, acceptance and troubleshooting guides |
| `docs/VERSION_HISTORY.md` | **Archived previous README**, preserving prior phase/version announcements verbatim |

**Local verification:**

~~~sh
python tests/check_web_ui.py
python tests/test_web_portal_ux.py
python tests/test_power_play_tracker2_contract.py
python tests/test_wled_phase8e_quick_setup_contract.py
pio run -e cyd
pio run -e s3_waveshare_35
~~~

The [GitHub Actions build workflow](.github/workflows/build-firmware.yml) runs many more C++/Python checks and produces both artifacts on successful runs. Passing CI proves **code/compile checks**, not speaker audibility, optical LED behavior, touch alignment, WLED restoration under packet loss, or real-game timing on physical hardware.

**Documentation policy:** Keep this README focused on **current installation, configuration, updating, and troubleshooting**. The previous development-oriented README is preserved at [docs/VERSION_HISTORY.md](docs/VERSION_HISTORY.md); it contains historical information and may describe features as "planned" that are now implemented. Use the current README and linked feature guides for setup, and Git commits/Actions runs for the exact firmware revision. Add future release notes to a version-history/changelog document rather than turning this installation guide into a chronological patch log.

---

**Helpful references:** [WLED JSON API](https://kno.wled.ge/interfaces/json-api/) · [WLED DDP](https://kno.wled.ge/interfaces/ddp/) · [Official NHL score API](https://api-web.nhle.com/v1/score/now) · [GitHub firmware builds](https://github.com/ggrant111/glowdeck-gameday/actions/workflows/build-firmware.yml).
