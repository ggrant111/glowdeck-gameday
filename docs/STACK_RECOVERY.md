# v0.2.9 — Recover from loopTask stack canary reboot (CYD)

## The actual crash

v0.2.8 users reported repeated reboot loops beginning immediately after successful Wi-Fi association:

    Guru Meditation Error: Core 1 panic'ed (Unhandled debug exception).
    Debug exception reason: Stack canary watchpoint triggered (loopTask)

This indicates stack overflow/overwrite, **not** a normal network disconnection. The earlier NHL reader moved to a 2 KB stack-allocated \`StaticJsonDocument\`, on the Arduino loopTask's default 8 KB stack, while also using HTTPS and other temporary objects.

v0.2.9 fixes this in several complementary ways:

1. Build flag \`-D ARDUINO_LOOP_STACK_SIZE=16384\` doubles task stack to 16 KB.
2. Replaces the scoreboard's stack \`StaticJsonDocument<2048>\` with a heap \`DynamicJsonDocument filter(2048)\` and rejects failed allocation.
3. Moves the 32-object temporary league standings array to static storage instead of using a stack-local array.
4. Prints stack high-water and heap free before/after NHL requests. The web dashboard shows minimum remaining stack headroom.
5. Preserves NHL partial-response retries, 10-second priority live polling, dated scores, and all existing WLED and rear RGB features.

A successful CI compilation is **not** hardware validation: inspect the serial monitor and run through at least one NHL fetch.

## Recover a continuously rebooting board

An immediate boot loop often prevents reaching the web-based OTA page. Use the USB serial port instead. On Windows:

1. Download the **successful v0.2.9** artifact named \`glowdeck-gameday-cyd-firmware\` from GitHub Actions; unzip \`firmware.bin\` to a local folder.
2. Connect the CYD to your computer by its USB cable and check the COM port in Device Manager (the user's previously known port was COM6; use its current value).
3. Install esptool if needed: \`py -m pip install esptool==4.8.1\`
4. Open PowerShell in the folder containing \`firmware.bin\`.
5. Run \`py -m esptool --chip esp32 --port COM6 --baud 460800 write_flash 0x10000 firmware.bin\` (replace COM6 as appropriate).
6. If the ESP32 **still boots the prior OTA partition**, clear **only OTA selection metadata** at 0xe000: \`py -m esptool --chip esp32 --port COM6 erase_region 0xe000 0x2000\`, then reset. This chooses the primary firmware written at 0x10000. **Do not run erase_flash** unless intentionally discarding all NVS configuration.

The board uses the standard \`min_spiffs.csv\` ESP32 partition layout (primary app at 0x10000 and otadata at 0xe000). If it reports an offset/partition error, stop and inspect the partition table before erasing anything. A USB power-cycling device may require pressing BOOT while reconnecting.

NVS Wi-Fi and WLED setup are left intact when using write_flash and (if needed) erasing only otadata. \`firmware.bin\` is a **partition-specific OTA app image**, not a complete flash dump; do not write it at 0x0.

## Verify once booting

Open 115200 baud serial monitor, reset, and confirm:

- \`GlowDeck GameDay v0.2.9\` appears once without a reboot loop.
- \`Configured loopTask stack: 16384 bytes\` appears.
- \`[NHL] poll entry stackWatermark=...\` and \`[NHL] poll success stackWatermark=...\` or a recoverable HTTP error appear.
- The dashboard shows **NHL connection** and **Stack headroom (minimum)**.
- The score carousel loads, and the CYD responds to touch.
- Open an upcoming game: season/conference data should not crash the board.
- Check that rear RGB and WLED tests still work.

If it still crashes, capture the **first boot's serial log** from 115200 baud, including the last \`[NHL]\` line, stack watermark and full panic backtrace. These readings distinguish an oversized TLS call stack from heap corruption or another task.
