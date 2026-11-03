# Recovery and diagnostics

## USB / COM port

On Windows, plug the CYD into the computer with a **data-capable USB cable**. Open PlatformIO Devices, or run:

\`\`\`powershell
pio device list
pio run -e cyd -t upload --upload-port COM6
pio device monitor -p COM6 -b 115200
\`\`\`

COM6 was the most recently reported port, but confirm in Device Manager after reconnecting. Some CYDs need a USB-A to USB-C cable or a USB-UART driver.

## Repeated \`rst:0x3 (SW_RESET)\` or black screen

1. Unplug all external peripherals (including optional speaker) and use stable USB power.
2. Check serial at **115200 baud** and record the first error **before** the reset line.
3. Verify the PCB silkscreen is **ESP32-2432S028R** and uses ILI9341; an ST7789 variant will need a different display driver.
4. Flash again with \`pio run -e cyd -t upload --upload-port COM6\`. If the app partition layout changed, use \`pio run -e cyd -t erase --upload-port COM6\` first (this deletes saved Wi-Fi settings), then upload.
5. For isolated testing, temporarily replace \`src/main.cpp\` with a very small sketch that prints \`alive\` and waits. If the simple sketch survives, add display, touch, then networking in that order.

Unlike earlier builds, this firmware starts the local screen and hotspot without blocking on Wi-Fi, WLED or NHL requests. A new reboot loop still requires the actual serial error trace; do not assume it was caused by the network.

## Pale blue background / pink buttons / video flicker

Some 2.8-inch CYDs need the controller's display-inversion command to render the intended dark interface. Starting in v0.1.1 the default is corrected for the reported panel and remains user-adjustable.

- Tap **SETUP → INVERT** on the physical touchscreen to switch the screen color mode.
- Alternatively open the web dashboard, choose **Setup → Correct inverted CYD display colors**, then save. The preference is stored in NVS.
- Expected GlowDeck appearance: a **dark navy background**, **white text**, **teal selected buttons**.
- If the screen only appears to pulse when viewed *through a phone camera*, it can be camera/display refresh interference. If flicker is visible to the naked eye, check power quality and update the firmware; the code now skips unchanged NHL scoreboard redraws.
- If actual screen tearing remains after the fix, try the hardware SPI speed diagnostic or a lower TFT SPI frequency instead of changing several variables at once.

## Touch inaccurate

Use **SETUP → CALIBRATE** on the CYD screen and tap the top-left then bottom-right crosses precisely. Your calibration is saved.

## Setup website cannot be reached

- Connect your phone to Wi-Fi \`GlowDeck-XXXXXX\` with the password on the device's SETUP page.
- Open **http://192.168.4.1** (use http, not https). The AP has no internet, which is expected.
- Browser sign-in: username \`admin\`, password initially the same as the setup hotspot.
- If connected to the home network instead, use the CYD's displayed LAN IP address.
- There is no captive-portal DNS redirect in this release.

## WLED test doesn't work

- The CYD and WLED controllers must be connected to the same accessible LAN.
- Enter a numeric WLED IPv4 address (e.g. \`192.168.1.71\`), enable that slot, and save.
- Confirm \`http://WLED-IP/json/state\` opens in your browser.
- Game-triggered lighting requires a **selected** NHL game and **HOME**, **AWAY**, or **BOTH** mode.
- Scoreboard events may lag real broadcasts; this is not a video/audio analysis app.

## Firmware update from browser fails

For an initial install, use USB. Browser updates require a successfully booted copy of the firmware and the matching OTA-capable \`min_spiffs.csv\` partition layout. Use \`.pio/build/cyd/firmware.bin\`, **not** the entire project ZIP.
