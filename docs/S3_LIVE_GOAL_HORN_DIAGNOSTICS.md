# Waveshare S3 — live goal horn troubleshooting (Phase 7B.4)

A real NHL goal can independently succeed in the on-screen GIF, WLED, and
the on-device speaker. A successful WLED HTTP acknowledgement does not prove
that an ES8311 speaker or a selected custom WAV played.

## What changed

- The real-goal path now checks the result of `s3Audio.play()` instead of
  silently ignoring a rejected playback request.
- The existing **Activity** row displays **Goal horn**, with results:
  **Not armed**, **Disabled in Sound settings**, **Audio device unavailable**,
  **Playback busy or in cooldown**, **Playback started**,
  **Stream completed (speaker not verified)**, or **Stream failed**.
- Activity records the selected built-in horn/custom WAV slot and alerts
  when a missing/invalid custom WAV caused the built-in horn fallback.
- **Customize → Sound** shows the live firmware's horn-enable state
  and currently selected WAV slot, independently of an unsaved checkbox edit.
- The standalone audio preview still works even if live-goal horn is off.
  This is intentional, so preview success alone cannot prove real-goal audio
  is enabled.
- The diagnostic code adds no new NHL events, does not affect which games
  trigger WLED, and does not alter stored WAV audio or its volume setting.
  Existing 30–80% ES8311 codec gain remains unchanged.

## Physical troubleshooting

1. In **Customize → Sound**, confirm that **Live NHL goal horn** reports
   **ENABLED**, and that the selected slot is correct. Use **Save settings**
   after changing the checkbox.
2. Press **Preview goal horn** (selected clip) while the device is idle.
   If you hear it, the DAC/amp and WAV decode can work on demand.
3. Run **Test speaker · 1 kHz**; if silent, use **Check audio diagnostics**
   for codec I2C, DAC mute/volume, amplifier enable, I2S frame count, and
   stream errors.
4. On the next qualifying armed goal, inspect **Activity → Goal horn**.
   **Playback started** and **Stream completed** indicate software streamed
   the file, *not* that physical sound was acoustically verified.
   **Playback busy or in cooldown** means the audio task declined overlap.
   **Stream failed** indicates the WAV read/decoder or I2S did not finish.
   If a custom WAV falls back to built-in, re-upload/verify that slot.
5. If hardware is wired differently from the Waveshare S3 3.5-inch board,
   check its ES8311 codec, I2S GPIO12/13/15/16 and TCA9554 P7 amp path.
   The original CYD instead drives a simple GPIO26 tone, not custom WAV.

Activity is session-only; older events collected before this update will not
have audio outcomes. This is a diagnostic software change and does not claim
a physical horn fix without real-device reproduction.
