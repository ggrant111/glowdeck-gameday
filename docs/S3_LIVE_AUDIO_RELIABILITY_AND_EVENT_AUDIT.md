# S3 live WAV reliability and event audit

## Symptom captured 2026-11-07

A followed NYR power-play-start event played custom WAV slot 2 and reported
278,361/278,361 PCM frames streamed in 5,821 ms, but no sound was heard.
The subsequent NYR goal selected slot 1 and failed after 3,840/364,800
frames and 110 ms. The old firmware labeled *all* WAV read/decoder failures
as `I2S write timeout` even without a failing `i2s_write` call. The PP
event's sound result was absent from Activity because the UI rendered
sound only for goals.

## Firmware behavior

- The WAV decoder now tolerates partial LittleFS reads and bounded transient
  zero-byte reads, without duplicating or skipping PCM.
- I2S uses twelve 256-frame stereo DMA blocks. A partial I2S write
  continues from its exact byte offset, with a 220 ms bounded wait and
  at most three consecutive no-progress attempts.
- PA enable is verified using TCA9554 P7 readback on the main loop; the
  audio task allows 15 ms for amp startup and 85 ms for the DMA tail before
  the main loop turns the amp off.
- If a **goal** WAV read or decode fails, the S3 attempts the synthesized
  goal horn once, without altering the saved goal slot. The original WAV
  failure and the fact that fallback was used are reported. A stalled
  I2S bus does **not** attempt a misleading fallback.
- Power-play, game-start and finish announcements have independent clip
  assignments; they do **not** substitute the goal horn on file error.
- A complete I2S stream with nonzero PCM peak and a verified PA latch still
  **does not prove audible speaker output**. No acoustic sensor is present.

## Identical per-event diagnostics

`Activity` includes the following for all NHL events and previews:
event ID, NHL game ID, matchup, team, score, period, on-device rear LED,
audio outcome, chosen source, goal fallback, frames sent/expected, elapsed
ms, PCM peak amplitude, I2S write retries, I2S result code, failed WAV
file offset, selected volume, PA latch verification, exact failure reason,
and WLED slot acknowledgements/results. Historical WLED results remain
visible if the controller is no longer configured.

`[EVENT AUDIT] #<ID>` in the serial monitor mirrors the same values
whenever event state changes (up to 4 checks/second), including a final
audio completion and later WLED restoration. Serial and Activity can be
correlated with the same event ID. Only the last 64 events are kept in RAM;
records are cleared on reboot. The observed NHL events, armed/followed
eligibility, WLED zone assignments, sound selections, and CYD speaker
output are unchanged.

## Hardware acceptance

1. Save **Customize → Sound** goal horn enabled and a custom goal WAV.
   Assign a different custom WAV as the power-play announcement.
2. From Test Lab, run speaker-only **1 kHz**, slot 1 audition, slot 2
   audition, combined goal and combined PP tests. Listen for sound.
3. For each combined test, refresh **Activity**; confirm audio source,
   frames, elapsed, PCM peak, amp latch, failure reason and WLED slots.
4. Connect serial at 115200. Filter for `[EVENT AUDIT]`,
   `[S3 Audio]`, `[PP Audio]`, and `[Goal Horn]`. Confirm matching
   event ID and metrics, including any fallback or I2S stall.
5. Observe a real followed/armed goal and real followed PP; the screen,
   WLED, and sound must be verified independently.
6. Recheck after an OTA/reboot; WAV slots/settings must remain intact.
   Repeat solo and combined tests under normal NHL polling and WLED load.

CI runs the WAV parser/short-read fixture, event-logging contracts, web
JavaScript validation, and both firmware builds. Physical audibility is
always a separate acceptance step.
