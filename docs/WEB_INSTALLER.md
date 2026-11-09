# ESP Web Tools installer and GitHub releases

This repository publishes two distinct images for each supported board:

- **`-factory.bin`**: merged image at flash offset **`0x0`** containing bootloader, partition table, OTA selector and app, used for first-time/recovery USB installs. The padded factory image **overwrites NVS preferences**; it is not a settings-preserving update.
- **`-ota.bin`**: application-only image (based on the PlatformIO `firmware.bin`) for an already-running GameDay's **Device → Firmware & Maintenance** OTA upload. It is not a complete first-install image.

Supported chip/board pairs are original **ESP32-2432S028R / ESP32** and **Waveshare ESP32-S3-Touch-LCD-3.5 (ST7796) / ESP32-S3**. Other variants (including Waveshare 3.5B) must not be flashed.

## Installer for end users

1. After the Pages deployment succeeds, open **https://ggrant111.github.io/glowdeck-gameday/** from **desktop Chrome or Edge** (USB Web Serial is unavailable on iPhone/iPad).
2. Connect the target ESP via a USB **data** cable, close serial monitors, click **Connect & install GameDay**, select its serial port, and approve flashing.
3. The `manifest.json` has two chip families and a separate factory image for each. Verify your **display board model**, because chip auto-detection alone does not recognize incompatible board variants.
4. Accept erase/new-install warning only if this is a new device or you intentionally want to reset its preferences. Do **not** use this path for routine updates.
5. After reboot, connect to the device's password-protected `GlowDeck-XXXXXX` hotspot and open **http://192.168.4.1** to connect the board to home Wi-Fi.
6. For ongoing updates, use the board's existing local dashboard and the release's matching **`-ota.bin`**.

[Upstream ESP Web Tools](https://esphome.github.io/esp-web-tools/) explains required HTTPS/Web Serial, manifests and merged ESP32 images.

## First-time GitHub Pages enablement (repository owner)

The deployment job uses **GitHub Pages** so the installer HTML, manifest and factory firmware are served from **one HTTPS origin**, avoiding cross-origin binary-download problems. GitHub **Releases** independently holds downloadable factory/OTA assets.

**If Pages has not yet been configured**, open repository **Settings → Pages → Build and deployment** and select **GitHub Actions** as the source. This is a **one-time owner setting**. The release workflow's `deploy-installer` job will fail until Pages is enabled. Do not assume the installer URL is live just because the release step passed.

After enabling Pages, open the **failed run** in **Actions → Release GameDay firmware and ESP Web installer** and select **Re-run failed jobs**. This reruns **only** the failed `deploy-installer` job with the already validated `glowdeck-web-installer-site` artifact; it does **not** rebuild or replace the published release. **Do not use Re-run all jobs** for an already published tag—the release job deliberately refuses to overwrite immutable published binaries. For a future release, update `release/VERSION` with a new version.

## Creating the next release

1. Commit all tested firmware, installer, documentation and build changes to `main`.
2. Ensure `.github/workflows/build-firmware.yml` passes its comprehensive tests on the source commit.
3. **As the final commit**, update **`release/VERSION`** to a fresh date-based release identifier such as `v2026.11.09-beta.1`.
4. That version-file push to `main` runs **`.github/workflows/release-firmware.yml`**. Alternatively use its manual workflow dispatch with an exact matching version.
5. The release job installs a pinned toolchain, runs selected contracts, generates NHL logos, builds **both** explicit PlatformIO environments, and merges each board's bootloader, `partitions.bin`, `boot_app0.bin` and firmware app.
6. `scripts/package_web_release.py` validates image header/offsets and generates a Pages manifest and release files, including **SHA256SUMS** and an archive of the installer.
7. `tests/test_web_release.py` verifies the generated factory header, both chip families, offsets, OTA separation, checksums and installer ZIP.
8. **Only after successful build/package checks** the job creates a prerelease tagged at that exact source commit, attaching the factory and OTA files.
9. The separate Pages job publishes the tested installer from the same run. Browser USB flashing remains unverified until tested on actual boards.

### Bootloader/flash layout and defaults

| | Original CYD | Waveshare S3 |
|---|---|---|
| PlatformIO env | `cyd` | `s3_waveshare_35` |
| ESP chip family | `ESP32` | `ESP32-S3` |
| Bootloader offset | `0x1000` | `0x0` |
| Partition table | `0x8000` | `0x8000` |
| OTA selector | `0xe000` | `0xe000` |
| Application | `0x10000` | `0x10000` |
| Merged factory image flashed at | `0x0` | `0x0` |
| Flash size | 4 MB | 16 MB |
| Merge flash mode | DIO | DIO (safe override for browser flashing) |
| Merge flash frequency | 40 MHz | 80 MHz |

S3's source PlatformIO configuration uses QIO flash and OPI PSRAM; ESP Web Tools documentation advises overriding QIO flash mode to DIO in browser merged images. **Verify real hardware boot and PSRAM behavior before treating this as a stable installer.** The merge uses Espressif `esptool` to apply the flash header values; a plain concatenation of binaries would be unsafe.

### Files and release checklist

- `web-installer/index.html` — responsive ESP Web Tools installation UI with explicit settings-erase warning.
- `scripts/package_web_release.py` — production packager and manifest/checksum writer.
- `tests/test_web_release.py` — validates generated binary structure and installer assets.
- `.github/workflows/release-firmware.yml` — compile, test, publish GitHub release, deploy GitHub Pages.
- `release/VERSION` — deliberate trigger and release tag.
- `docs/WEB_FLASH_RELEASE_NOTES.md` — description attached to the release.

**Never** point a Web Tools factory manifest at a plain `.pio/build/<env>/firmware.bin` using flash offset zero. The application lives at `0x10000` and depends on correctly flashed bootloader, partition table and OTA metadata. For a non-USB update, prefer the running board's dashboard with the **`-ota.bin`** image.
