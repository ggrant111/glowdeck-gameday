# Waveshare ESP32-S3 3.5-inch GlowDeck GameDay (first-install beta)

**Hardware target:** Waveshare **ESP32-S3-Touch-LCD-3.5** with **ST7796 SPI** LCD and **FT6336/FT6X36 I2C capacitive touch**. This is the model associated with Amazon ASIN `B0F92CWMPT`. **Do not** install on the separate `ESP32-S3-Touch-LCD-3.5B` (AXS15231B QSPI) or the original ESP32-2432S028R CYD; those have different display hardware.

## What this port preserves

This is the **same `src/main.cpp` GameDay core** as the CYD: date-scoped NHL scoreboard, event logging, PP and pulled-goalie indicators, all-live event overlays, favorites, on-screen follow stars and multiple followed games, team colors, live carousel, standings, Preferences, SoftAP and authenticated web companion, WLED configuration, celebration triggers and state restoration, and local web OTA endpoint.

The initial S3 display driver provides a full-screen **landscape 480×320** picture by scaling the existing 320×240 UI in a 16-bit frame buffer. Capacitive touch is converted to the established virtual hit targets, preserving stars, menus and swipe gestures. The boot screen or touch may need orientation/color tuning after physical testing.

**Hardware not yet enabled:** rear RGB LED GPIO and horn. The original CYD GPIO4/17/16 RGB pins and GPIO26 horn input are *not portable* to Waveshare. S3 has an ES8311 audio codec (requires an I2S codec driver) and no CYD-style rear LED. Software follow stars, NHL events and WLED celebrations still work. Do not wire lights to the CYD pin list! Rear LED is hard-disabled on the S3 port until a verified external LED expansion mapping exists; the horn is silent.

## Pin mapping, Waveshare ST7796 V1/V2

| Hardware | ESP32-S3 GPIO / interface |
| --- | --- |
| LCD ST7796 SPI MOSI / MISO / CLK / DC | GPIO1 / GPIO2 / GPIO5 / GPIO3 |
| LCD chip select | hardwired (-1) |
| LCD reset | TCA9554 I2C expander at 0x20, P1 |
| Backlight | GPIO6, active HIGH |
| Touch FT6X36 | I2C address 0x38 |
| I2C SDA / SCL | GPIO8 / GPIO7 |
| USB | onboard USB-C (CDC) |
| WLED | Wi-Fi, no GPIO needed |

These assignments come from [Waveshare official Arduino examples](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-3.5/tree/main/examples/arduino). Check PCB revision labeling when the physical unit arrives.

## First USB install (Windows / PlatformIO)

1. Install [Visual Studio Code](https://code.visualstudio.com/) and the PlatformIO extension. Clone/download this repository and open its root folder.
2. Connect the S3 using a **USB-C data cable**. If there is no COM port, hold **BOOT**, briefly tap **RESET**, then release **BOOT** and reconnect; inspect Device Manager.
3. Run `python scripts/generate_logos.py` if `include/TeamLogos.h` is not present. This script needs `Pillow` and `CairoSVG`, installed with `python -m pip install Pillow==11.3.0 CairoSVG==2.8.2`.
4. In PlatformIO terminal, run `pio run -e s3_waveshare_35` to compile, then `pio run -e s3_waveshare_35 -t upload --upload-port COM6` (replace `COM6` with the **actual** new board port).
5. Run `pio device monitor -p COM6 -b 115200` and verify S3 startup, LCD initialization, touch detection, and the `GlowDeck-XXXXXX` Wi-Fi network.
6. Connect a phone to the displayed `GlowDeck-XXXXXX` access point and open `http://192.168.4.1`; set home Wi-Fi and WLED destinations there. After the first USB flash, future S3 updates can use the web dashboard's existing OTA firmware-upload flow (with an S3 firmware.bin file, **not** a CYD file).

The GitHub Actions workflow builds and uploads **`glowdeck-gameday-s3-waveshare-35-firmware`** as a downloadable build artifact (when the branch checks pass). The file inside is `firmware.bin`. Use **PlatformIO upload** for the first flash; do not assume that an app-only `firmware.bin` is an all-in-one `0x0` factory image for a blank board.

## S3 startup crash fix — TFT_eSPI 2.5.43

If the serial monitor shows `StoreProhibited`, `EXCVADDR: 0x00000010`, and a
backtrace pointing to `TFT_eSPI::begin_tft_write()` during `tft.init()`,
the default ESP32-S3 SPI port definition in TFT_eSPI 2.5.43 may be invalid.
The S3 PlatformIO environment adds `-D USE_FSPI_PORT` to force the valid SPI
register port index. This is **S3-only** and leaves the CYD build unchanged.

To apply the fix, pull the updated main branch, build and flash the
`s3_waveshare_35` environment over USB. If an older local binary was built,
run `pio run -e s3_waveshare_35 -t clean` before rebuilding. Then reset the
board and inspect serial output at 115200 baud. A successful compile is not
equivalent to a verified physical display or touch test.

Reference: https://github.com/Bodmer/TFT_eSPI/issues/3925

## First-arrival smoke test

- Look for a correctly colored 480×320 landscape screen and a GameDay welcome view.
- Touch the menu, choose GAMES, return to TODAY and swipe between game slides; tap a game star to follow.
- Open the web dashboard, connect Wi-Fi, and check NHL games and standings.
- Run a WLED TEST on a reachable controller and confirm it returns to its previous state (do not assume external LED GPIO output).
- Trigger a simulated celebration and watch the on-device event animation; confirm team colors against the dashboard event log.
- Observe serial monitor and note failures: LCD dark, touch reversed, USB enumeration, unexpected reboot, PSRAM allocation failure.
- Before any OTA, confirm that the downloaded binary is the **S3** build and the board label ends in `3.5` rather than `3.5B`.

## Engineering notes / known limitations

- S3 uses `esp32-s3-devkitc-1` / Arduino 2.x on Espressif32 6.9.0 with 16MB Flash, 8MB OPI PSRAM, and a two-slot OTA partition layout.
- Shared GameDay core is compiled unmodified for CYD by default (`default_envs = cyd`); S3 is a separately selected build environment.
- Screen pixel scaling is nearest-neighbor; a future native 480×320 or 320×480 layout can use higher-resolution typography and game cards.
- FT6336 first-contact registers are polled over I2C at up to ~45 Hz. Coordinates use native portrait 320×480 and map into the old 320×240 canvas with rotation 1. After checking real hardware, tune orientation if necessary.
- This is a **compile/test candidate**, not hardware-certified until verified on a physical board. In particular, the LCD's color order/inversion and FT6336 orientation require arrival-day smoke testing.
