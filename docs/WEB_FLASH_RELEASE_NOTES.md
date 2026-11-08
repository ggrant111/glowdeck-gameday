# GlowDeck GameDay — USB Web Installer Beta

This release makes it possible to install the GameDay firmware on an **original ESP32-2432S028R (CYD)** or **Waveshare ESP32-S3-Touch-LCD-3.5 (ST7796)** using a USB cable and a desktop browser with ESP Web Tools.

## Which binary should you use?

| Device | Factory flash over USB | Existing device web OTA |
| --- | --- | --- |
| Original ESP32 CYD | `glowdeck-gameday-cyd-factory.bin` | `glowdeck-gameday-cyd-ota.bin` |
| Waveshare S3 (ST7796) | `glowdeck-gameday-s3-waveshare-35-factory.bin` | `glowdeck-gameday-s3-waveshare-35-ota.bin` |

**Do not switch board binaries. Do not flash a Waveshare 3.5B QSPI device with this S3 image.**

For a brand-new board, use the hosted [ESP Web Tools installer](https://ggrant111.github.io/glowdeck-gameday/) after the GitHub Pages deployment is enabled and successful. ESP Web Tools reads the connected chip and selects the correct full firmware image. Desktop **Chrome or Edge** with Web Serial and a data-capable USB cable are required; this does not work on an iPhone/iPad.

**Factory images reset configuration:** Merged factory files are meant for a fresh flash and include the bootloader, partition table, OTA selector and firmware. The padded factory image writes over the NVS preferences area; saved Wi-Fi, GameDay selections, controller IPs and credentials may be lost. Back up settings before factory flashing. For upgrades, use the **app-only `-ota.bin`** file in the on-device **Device → Firmware & Maintenance** page. Normal app-only OTA is intended to preserve preferences and LittleFS media when the partition layout is unchanged.

## Release assets

- Two **merged factory images** (one per chip/board) for first-time USB install or deliberately resetting/recovering a board.
- Two **app-only OTA images** for browser updating an existing running device.
- `glowdeck-gameday-<version>-web-installer.zip` containing an HTML ESP Web Tools page, `manifest.json`, and both factory firmware images (must be served from HTTPS for USB Web Serial).
- `SHA256SUMS.txt` for verifying downloaded assets.

Published firmware binaries are built and validated in GitHub Actions from the tagged commit; artifacts are never made from an arbitrary local binary.

## Highlights of the current GameDay firmware

- Live NHL scores and ticker, favorites, multi-game following and separately armed celebrations.
- Power-Play Tracker 2.0 and the Recent Activity / serial event audit.
- Native WLED JSON effects, discovery for up to 8 controllers, Home/Away/Neutral zones and scenes, and optional validated DDP RGB streaming.
- Local Philips Hue Bridge pairing and light selection.
- WAV and speaker diagnostics on Waveshare S3, optional case LEDs when correctly wired.

**Beta limitations:** Full optical LED, speaker-audibility, and touch tests on your physical hardware are not implied by a green CI build. Philips Hue requires a physical Bridge; neither external LED strips nor the Waveshare speaker can be validated by GitHub Actions alone.

See the [full installation and troubleshooting guide](https://github.com/ggrant111/glowdeck-gameday/blob/main/README.md), [Waveshare board guide](https://github.com/ggrant111/glowdeck-gameday/blob/main/docs/WAVESHARE_ESP32_S3_3P5.md), and [project security notes](https://github.com/ggrant111/glowdeck-gameday/blob/main/docs/SECURITY.md).
