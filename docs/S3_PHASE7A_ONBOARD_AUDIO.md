# Phase 7A — Waveshare ESP32-S3 3.5-inch onboard goal horn

**Status:** Initial software implementation; GitHub Actions firmware compilation and
physical speaker confirmation are independent validation gates.

## Device compatibility and pins

**Only** Waveshare **ESP32-S3-Touch-LCD-3.5**, ST7796 SPI / FT6336 touch;
**not** ESP32-S3-Touch-LCD-3.5B QSPI or the original 2.8-inch CYD.

The board has an onboard **ES8311 codec** (I2C address 0x18), driving the
speaker port through the onboard power amplifier. The correct **speaker
connector is on the board**: do not connect a speaker directly to I2S
GPIOs or attach a second amplifier to the LCD/SD/USB pins.

| Circuit | Connection |
| --- | --- |
| Shared I2C SDA / SCL | GPIO8 / GPIO7 (already initialized for FT6336) |
| Codec ES8311 | 7-bit I2C address 0x18 |
| I2S master clock / bit clock | GPIO12 / GPIO13 |
| I2S left-right clock / speaker data | GPIO15 / GPIO16 |
| TCA9554 I2C expander | 0x20; P7 / EXIO7 enables onboard speaker amplifier |
| LCD reset (must be preserved) | TCA9554 P1 / EXIO1 |
| DAC sample format | 48 kHz, 16-bit stereo, MCLK×256 |

Source: Waveshare's
[official board and Arduino examples](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-3.5),
particularly `examples/arduino/examples/01_audio_out`, and Espressif's
Apache-2.0 ES8311 configuration sequence.

## What Phase 7A does

- Initializes ES8311 after the S3 LCD and touch boot sequence. If the
  I2C codec is absent or clock/DMA setup fails, the NHL app **continues**,
  and the dashboard reports the error instead of restarting.
- Generates an original 900 ms **goal horn** in fixed-point 48 kHz PCM,
  using three low triangle-wave voices and click-safe attack/release.
  Peak PCM samples stay below full scale; the codec hardware is fixed at
  **50% volume** (`ES8311 0x32 = 0x7F`).
- Streams the short synthesized clip from a dedicated FreeRTOS I2S task,
  without storing a large audio file, blocking NHL HTTPS, or changing the
  WLED and event screen paths. The main loop retains exclusive control
  of the shared I2C bus.
- Powers the amplifier **only during playback**, leaving TCA9554 P7
  low while idle. Existing P1 LCD reset, touch mapping and colors stay
  untouched.
- Adds **Settings → Onboard speaker → Preview goal horn · 50%** to the
  web companion. The new, authenticated POST endpoint
  `/api/audio/preview` is S3-only; it returns an error if the codec is
  unavailable or audio is busy. It never starts an NHL event, changes
  follow selections, sends WLED commands, or writes NVS.
- For **actual armed/followed NHL goals**, the existing saved
  `horn` preference still controls whether the onboard S3 horn plays.
  Enable **Enable onboard S3 goal horn (ES8311)** and **Save settings**.
  The Fire TV horn and CYD's separate GPIO26 tone path are untouched.
- Repeated previews have a 1.2 s start-to-start cooldown to keep the
  speaker from continuous rapid retriggering. No background audio on
  boot; no microphone recording, SD-card access or custom WAV upload.

## Firmware/OTA installation

1. Open the successful GitHub Actions run for Phase 7A and download
   `glowdeck-gameday-s3-waveshare-35-firmware`.
2. Extract `firmware.bin`, open your Waveshare S3's GlowDeck dashboard,
   and use **Firmware → Update** for your existing S3 device.
3. Reload the dashboard after restart. Under **Settings**, the ES8311
   card should report `ES8311 ready · 50% volume · idle`.
4. Select **Preview goal horn · 50%**. Confirm a short horn through the
   connected speaker; the LED/WLED controllers and game display should
   not react to this isolated audio preview.
5. If correct, check the **Enable onboard S3 goal horn** option and
   **Save settings**. A genuine goal for an armed game now uses the same
   sound and still triggers its original WLED/screens independently.
6. Run two or three previews spaced over 1.2 seconds. Confirm touch,
   carousel, scoreboard, Wi-Fi and OTA responsiveness during/after audio.
7. Monitor serial USB at **115200** for lines beginning `[S3 Audio]`.
   On silence, check codec status, speaker connector seating and the
   `[S3 Audio] unavailable: ...` message. Do not increase gain or
   change pins before confirming the correct board revision.

## Acceptance / limitations

- CI checks the 50% ceiling, 900 ms 48 kHz sample generation, 16-bit
  range and envelope, cooldown at `millis()` wraparound, existing
  TCA9554 P1 isolation, read-only codec state, nonblocking task, auth,
  web preview separation from WLED and unchanged original CYD build.
- **CI does not produce audible sound.** Real-hardware speaker output,
  perceived loudness, hum/pops, accurate I2S clocking and simultaneous
  Wi-Fi/goal events must be confirmed on the user's connected board.
- Codec register configuration is intentionally constrained to the
  48 kHz playback profile only. **Phase 7B** will add custom WAV files,
  format validation, playback scheduling, and safe upload storage.
- Phase 7C will handle independent case LEDs and must use a separate,
  verified Waveshare pinout rather than CYD rear RGB GPIO.
