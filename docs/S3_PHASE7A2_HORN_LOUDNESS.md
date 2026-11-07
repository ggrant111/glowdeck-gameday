# Phase 7A.2 — Audible goal horn and adjustable S3 speaker level

## Why this changed

The device owner confirmed that both the original Phase 7A.1 GOAL horn
and 1 kHz diagnostic tone are audible on their connected Waveshare
ESP32-S3-Touch-LCD-3.5 speaker, **but the horn was only audible near the
ear**. This confirms that I2S, ES8311, TCA9554 P7 amplifier enable and
the speaker connection work. The issue is now the deliberately
conservative, bass-heavy horn, its PCM level, and its ES8311 gain.

## New sound

- Retains the original 900 ms nonblocking synthesized goal horn, with
  a 25 ms attack and 130 ms release.
- Keeps the lower 220 Hz note but emphasizes upper harmonics at
  **440 / 660 / 880 Hz**, which small speakers reproduce more easily.
- Bounded PCM peak rises from 10,000 to **30,000** (16-bit max 32,767),
  with no clipping. No sample/file allocation or sound library is needed.
- The 1 kHz diagnostic waveform remains unchanged so you can compare
  against a known, repeatable reference.
- Waveform is original; no commercial team goal horn audio is reproduced.

## Speaker control

- **Settings → Onboard speaker → Speaker volume**, from **30% to 80%**
  in 5% increments; start with **70%**, corresponding to Waveshare's
  reference ES8311 volume setting.
- Click **Apply volume**, then **Preview goal horn**. Both the preview
  and real NHL goals use that same saved gain. The 1 kHz diagnostic
  also uses that gain.
- Gain setting is stored as **`s3Vol`** in S3 NVS and restored
  after boot. It does not change the CYD's speaker or the WLED
  brightness preference.
- The authenticated, S3-only
  `POST /api/audio/volume` endpoint accepts an integer
  `{"percent":70}` within the safe 30–80 range. A gain change
  during playback returns an error rather than racing with the codec
  and cannot trigger lights or a scoreboard event.
- The ES8311 register 0x32 is written and **read back** before
  persisting the new setting. The reference formula is
  `(percent * 256 / 100) - 1`; 50% stays available (`0x7F`),
  70% gives `0xB2`, and 80% gives `0xCB`.
- The TCA9554 speaker amplifier remains off when idle. No changes
  to LCD reset P1, touch, OTA, NHL polling, WLED or Fire TV.
  Real goals still respect **Enable onboard S3 goal horn**.

## How to test on your connected speaker

1. Download the S3 artifact from the successful Phase 7A.2 GitHub
   Actions workflow, extract `firmware.bin`, and install through
   the existing GlowDeck S3 web OTA page.
2. Reopen the dashboard → **Settings → Onboard speaker**.
3. Confirm **70%** speaker volume or adjust to **50%** first
   if you want a direct comparison to the previous firmware.
4. Click **Preview goal horn** from normal listening distance.
   **Do not hold the speaker against your ear while testing a
   higher setting.** Increase gradually using the slider if
   necessary; if it distorts, lower the gain.
5. Compare with **Test speaker · 1 kHz**. Both should work
   without starting WLED/overlays or resetting the S3 display.
6. Reboot and confirm the chosen level persists. If there is
   an unexpected issue, use **Check audio diagnostics** and
   record ES8311 register, selected percent and I2S transfer
   status.

**Hardware acceptance:** Both tones were already physically audible
on 7A.1; whether the new horn is sufficiently loud and clean is pending
the next on-device test. CI only proves the gain mapping, signal bounds,
endpoint isolation and successful firmware builds. A small speaker may
still have physical sound-pressure limitations.

**Next:** Once sound level is acceptable, Phase 7B custom WAV
upload/decoding and safe audio scheduling.
