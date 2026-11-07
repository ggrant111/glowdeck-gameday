# Phase 7B.1 and 7B.2 — S3 custom WAV upload and sound library

**Target:** Waveshare ESP32-S3-Touch-LCD-3.5 ST7796, not Waveshare 3.5B, not CYD.

## Features

The existing Settings → Onboard speaker section now includes **Goal horn
sound library**. It supports three persistent custom audio slots, plus the
unchanged synthesized built-in horn. All work directly from the ESP32-S3
without a server, computer, SD card, or phone required at playback time.

- Upload a WAV from your browser to slot **1**, **2**, or **3**.
- Name shown in the library is a sanitized version of its uploaded filename.
- Preview the built-in horn, any uploaded clip, or the currently selected clip
  *without* NHL goals, WLED commands, display takeovers or Fire TV events.
- Click **Use for goals** on a custom slot, or **Use built-in horn** to restore
  the factory-synthesized sound. The selection persists in S3 NVS
  (`s3Sound`), independent of S3 gain (`s3Vol`) and horn enable (`horn`).
- When **Enable onboard S3 goal horn** is checked, a real eligible followed
  or armed NHL goal plays the selected audio; WLED, goal GIF and existing
  celebrations still follow the original admission policy.
- Delete uploaded clips. Deleting the active clip automatically selects the
  built-in horn; deleting a different clip leaves selection untouched.
- If a selected custom file is missing or corrupted when a goal fires,
  fall back to the built-in horn and emit a serial diagnostic.

## WAV format and limits

Only ordinary uncompressed **RIFF/WAVE PCM, signed 16-bit, mono or stereo**.
Sample rates **8,000–48,000 Hz**, including 22,050 / 44,100 / 48,000 Hz.
Other codecs (MP3, ADPCM, AAC, float WAV, WAVE_FORMAT_EXTENSIBLE) are
intentionally rejected; **renaming an MP3 to .wav will not work**.
Encoded file size **maximum 768 KiB per slot**, maximum **10 seconds**.
These caps keep the 16MB flash / OTA partition usage bounded.

The S3 still runs its ES8311 at **48 kHz 16-bit stereo**. Each PCM clip
streams from LittleFS in 256-frame chunks, downmixing stereo to mono
safely and performing 64-bit fixed-point linear resampling to 48 kHz.
Playback includes short fade-in/out to avoid sharp start/stop clicks.
No full WAV is copied into heap or PSRAM; the NHL loop is free to
continue handling games, screen drawing and WLED operations.

For a file with unsupported encoding, convert it using an audio editor
to 16-bit PCM WAV at 44.1 or 48 kHz, preferably **mono** to save flash.
The upload UI and API explain accepted formats when a file is rejected.

## Flash safety and upgrades

The sound files use LittleFS paths `/sound1.wav`, `/sound2.wav`,
`/sound3.wav`; they do not overwrite the existing `/goal.gif` animated
goal video. Browser uploads stream into a temporary `/sound.tmp`.
The firmware parses and validates the completed upload before replacing
the destination slot. An existing slot is backed up as `/sound.bak`
with a `/sound.slot` recovery marker, then swapped into place;
interrupted updates restore the prior slot on next boot.

**No partition-table migration** is included in 7B: use the existing
Waveshare S3 OTA firmware workflow. Normal firmware-only OTA retains
LittleFS and NVS. Erasing/reformatting the entire flash or changing
partitions can delete uploads, as with the goal GIF; keep copies of
your WAV files on your computer. If the LittleFS partition does not
have enough free space to hold a temporary file during replacement,
the upload reports an error instead of deleting the existing clip.

To avoid file read/write races, upload/replace/delete/select is
blocked while audio is playing. Additional audio starts are blocked
during an upload. WAV file names cannot control filesystem paths;
the stored display name only allows ASCII letters, digits, spaces,
hyphens and underscores (max 24 characters).

## API

All routes are **S3-only** and require the existing GlowDeck dashboard
authentication (home network HTTP Basic or authenticated setup AP).

| Endpoint | Purpose |
| --- | --- |
| GET `/api/audio/library` | Flash capacity, selected source, three slot metadata |
| POST `/api/audio/upload?slot=1` | multipart `sound` WAV upload to slot 1–3 |
| POST `/api/audio/select` | JSON `{"source":0}` built-in, or `1..3` |
| POST `/api/audio/delete` | JSON `{"slot":1}`; resets selection if needed |
| POST `/api/audio/preview` | `{"source":0}` or `{"source":1..3}`; empty JSON plays selected |
| POST `/api/audio/test-tone` | Independent 1kHz output check, unchanged |
| POST `/api/audio/volume` | Existing 30–80% ES8311 volume control |

The library is S3-only; the CYD GPIO26 speaker and its firmware compile,
along with the Fire TV horn, remain unchanged.

## Physical test after a successful CI build

1. Download `glowdeck-gameday-s3-waveshare-35-firmware` from the passing
   GitHub Actions run, extract `firmware.bin` and install via existing S3 OTA.
2. Go to **Settings → Onboard speaker → Goal horn sound library**. Confirm
   it displays your current Flash capacity and the built-in horn selected.
3. Upload an ordinary 16-bit PCM WAV file to slot 1; verify the displayed
   name, duration and sample rate. Preview that slot **without** lights.
4. Click **Use for goals** on slot 1, then **Preview goal horn**. Both should
   play that custom clip at your saved S3 volume (70% unless changed).
5. Upload a different sound into slot 2. Confirm slot 1 is still intact.
   Preview slot 2, then reboot and confirm both slots/selection persist.
6. Delete slot 1 when it is selected and verify that the built-in horn
   becomes the saved default. Reupload to test the recovery/replace path.
7. Try uploading MP3 renamed to .wav, truncated WAV, or a file over 768 KiB:
   all should fail **without replacing** a previously valid sound.
8. Enable the saved horn option and verify an actual followed/armed
   NHL goal uses the selected sound **and still drives the existing GIF
   and WLED outputs**. An unarmed game should not play sound.
9. During audio preview, confirm the scoreboard, touchscreen, web controls,
   WLED restoration and Wi-Fi remain responsive. Check serial at 115200
   for `[S3 WAV]` or `[S3 Audio]` errors.

**Automation disclaimer:** CI can verify parser edge cases, PCM bounds,
endpoint isolation and firmware compilation but **cannot hear speaker
output or simulate a power interruption on physical flash**. Audio
performance, LittleFS available capacity, OTA persistence and no pops
must be confirmed on your actual S3 board.

**Phase 7B.3 (future):** Team-specific sound assignments. Phase 7C:
independent left/right case LEDs. Neither is silently enabled in 7B.1/2.
