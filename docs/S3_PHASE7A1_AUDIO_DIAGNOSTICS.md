# Phase 7A.1 — Silent Waveshare S3 speaker diagnostic

> **Phase 7A.2 update:** The user physically confirmed audible output, but the original horn was too quiet. The current firmware now uses a brighter horn and **adjustable 30–80% ES8311 volume (70% default)**. Earlier references below to a fixed 50% level apply only to the original 7A/7A.1 builds. See [the latest horn and volume guide](S3_PHASE7A2_HORN_LOUDNESS.md).

**Reason:** After installing the 7A firmware, the user reported no audible
sound from the connected speaker. The earlier **ready** indicator proved
only that the ES8311 acknowledged I2C initialization; it did **not** prove
that the amplifier was enabled or that I2S PCM samples reached the codec.
The original 220–330 Hz synthesized horn also emphasized bass, which small
enclosure speakers can reproduce poorly.

**Target:** Waveshare ESP32-S3-Touch-LCD-3.5 ST7796, not the distinct
3.5B. The original CYD remains unchanged.

## What changed

No physical wires, LCD colors, touch mapping, NHL events, WLED commands,
Fire TV playback, or stored `horn` preference are altered.

- Added **Test speaker · 1 kHz** (1.2 seconds, 50% ES8311 volume)
  alongside **Preview goal horn · 50%**. The distinct strong mid-tone is
  much easier to hear on a small speaker than the low horn.
- Both previews are S3-only, authenticated, independent of the `horn`
  checkbox, and **never** arm games, alter NVS, send WLED requests or
  enqueue an NHL celebration.
- Added a **Check audio diagnostics** button. The read-only endpoint
  `GET /api/audio/diagnostics` exposes codec register readback (DAC
  mute, codec volume), TCA9554 P7 direction and output, last sound,
  PCM frames sent through I2S and write elapsed time.
- The ES8311 initialization now mirrors the reference driver's analog
  microphone-path settings and verifies **volume register 0x32=0x7F**
  with mute bits 5/6 clear. This does not turn on recording or change
  the microphone power beyond the official codec initialization.
- When enabling/disabling the speaker amplifier, read its expander
  output latch back, rather than assuming a successful I2C write also
  proves the pin is HIGH or LOW.
- I2S streaming logs failure reason and last complete frame count,
  and reports a clear failure if any frame transfer is incomplete.
- The original 900 ms GOAL horn is unchanged in this diagnostic
  revision; actual goals still play it only when `horn` is enabled.

**Reference:** Waveshare
[Arduino ES8311 examples](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-3.5/tree/main/examples/arduino/examples)
and
[factory BSP](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-3.5/blob/main/examples/factory/components/esp32_s3_touch_lcd_3_5/bsp_board.cpp)
confirm I2S MCLK 12 / BCLK 13 / LRCK 15 / DOUT 16,
ES8311 codec 0x18 on shared I2C 8/7, and P7 as the amplifier-enable
output on the TCA9554 at 0x20. P1 LCD reset remains untouched.
No CYD GPIO26 or legacy rear-LED GPIO is used on S3.

## Physical acceptance

1. Install the **S3** firmware artifact from the successful Phase 7A.1
   Actions run using the device's established OTA process. **Do not**
   use a CYD `firmware.bin` or a Waveshare 3.5B image.
2. Open the S3 web dashboard → **Settings → Onboard speaker**. The
   card must say **Codec initialized**; this alone does not prove sound.
3. Click **Test speaker · 1 kHz**. Listen for a clear, short mid-pitched
   note. Test this with a securely seated known-working speaker.
4. Click **Check audio diagnostics** after at least two seconds and
   note the displayed report. A correct successful test will show:
   - ES8311 I2C **OK**
   - Volume register **0x7F** (fixed 50%)
   - DAC mute register bits 5/6 clear
   - TCA9554 P7 direction **OUTPUT**, PA **LOW** while idle
   - **57,600** frames sent for the 1.2 s test tone
   - I2S stream complete **true**, with elapsed around the sound length
5. Try the existing **Preview goal horn · 50%**. If the diagnostic tone
   sounds but the horn is inaudible, the cause is likely low-frequency
   speaker response; tune the horn's harmonic balance in a later audio
   iteration rather than changing the wiring.
6. If **neither** plays, copy the diagnostic text (or collect 115200-baud
   USB serial lines beginning `[S3 Audio]`). These results distinguish
   codec initialization, expander PA control, actual I2S writes, and
   physical speaker path. Do **not** blindly change codec gain or GPIOs.
7. Ensure GOAL/PP overlays and WLED celebrations still work normally
   during and after audio. No device should reboot.

Only real speaker tests can establish that audio actually reaches the
speaker. Automated CI verifies PCM bounds, readback code paths, endpoint
isolation and firmware compilation, not electrical audio output.
