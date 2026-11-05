# Supported board and pin map

Target: **ESP32-2432S028R**, often called the **2.8-inch Cheap Yellow Display (CYD)**, with **320 × 240 ILI9341** in landscape and **XPT2046 resistive touch**.

This is **not** a general ESP32 display firmware. Other variants with ST7789 screens, capacitive touch, RGB parallel panels or different pin maps require separate testing and build configurations.

| Component | CYD GPIO | Notes |
| --- | --- | --- |
| TFT MISO | 12 | ILI9341 display on HSPI |
| TFT MOSI | 13 | |
| TFT SCK | 14 | |
| TFT CS | 15 | |
| TFT DC | 2 | |
| TFT reset | -1 | Hardwired; no GPIO reset |
| Backlight | 21 | Active HIGH |
| Touch SCK | 25 | Separate VSPI |
| Touch MOSI | 32 | |
| Touch MISO | 39 | Input-only |
| Touch CS | 33 | |
| Touch IRQ | 36 | Input-only |
| Speaker amplifier input | 26 | Requires a speaker connected to the CYD's speaker output |
| Rear RGB red | 4 | Active LOW, PWM channel 2 |
| Rear RGB green | 17 | Active LOW, PWM channel 3; corrected in v0.3.9 |
| Rear RGB blue | 16 | Active LOW, PWM channel 4; corrected in v0.3.9 |

**Rear RGB note:** v0.3.8 had the CYD rear green and blue GPIO assignments reversed. Physical diagnostics confirmed the correct mapping above for this board. Other CYD revisions can differ; use the independent RGB diagnostics in the dashboard to verify.

**Pin conflicts:** Never assign GPIO25, 32, 33, 36 or 39 to general-purpose outputs while touch is in use. The speaker input is GPIO26; it does not conflict with the touch SPI bus. On some board revisions, display wiring or color order differ.

The on-device **SETUP → CALIBRATE** button guides through a two-point touch calibration, saved in ESP32 Preferences (NVS). If touch appears inverted or inaccurate, use a stylus. Tap the crosses at the requested locations.

## Architecture

\`\`\`
NHL public scoreboard -- HTTPS --> ESP32-CYD firmware
                                  |-- 2.8" touchscreen (game / mode / test)
                                  |-- Wi-Fi AP + HTTP dashboard on port 80
                                  |-- Preferences (saved device settings)
                                  |-- WLED JSON API to up to 4 IPs
                                  \`-- GPIO26 horn tone (optional)
\`\`\`

No Fire TV, Android app, Home Assistant, MQTT broker or computer-based server is required.

## Memory and reliability

- UI is rendered directly through TFT_eSPI, without LVGL framebuffers.
- Dashboard HTML is in flash (PROGMEM) and uses no web/CDN resources.
- Wi-Fi starts asynchronously after the local access point and web server.
- The first scoreboard sample is a baseline, not a goal celebration.
- WLED changes only occur when the current WLED state is successfully read for later restoration.
- Settings write to NVS only when changed, rather than every poll.
- HTTPS uses the NHL endpoint with certificate verification disabled in this initial version; see [SECURITY.md](SECURITY.md).

Pin-map references: https://randomnerdtutorials.com/esp32-cheap-yellow-display-cyd-pinout-esp32-2432s028r/ and https://lcdwizard.com/boards/cyd-2432s028r.
