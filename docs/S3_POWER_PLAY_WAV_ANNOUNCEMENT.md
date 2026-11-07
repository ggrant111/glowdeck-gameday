# S3 power-play announcement (WAV)

**Hardware:** Waveshare ESP32-S3-Touch-LCD-3.5 with onboard ES8311 and speaker.
The original CYD buzzer has no PCM WAV playback; Fire TV is not altered.

## Setup

1. Install an S3 firmware build containing this feature through the existing OTA process. Do not erase flash or format LittleFS.
2. Download `powerplay_gameday_mono.wav` from the conversation. It is the supplied 5.8-second power-play announcement, downmixed from 44.1 kHz stereo PCM16 (about 999 KiB) to **44.1 kHz mono PCM16 (about 500 KiB)**, within the existing **768 KiB / 10-second** upload limits. No change to the clip's timing or playback speed.
3. Open **Customize → Sound → Sound library**. Upload to an empty WAV slot (for example, slot 2) and tap **Preview**. Do **not** press **Use for goals** unless you want to replace the working goal horn.
4. Tap **Use for power plays** on the uploaded slot. It is saved separately as `s3PpSound` (1–3). **Disable PP announcement** sets it to 0; new installs default OFF. The goal-horn enable checkbox, chosen goal WAV and volume are unchanged.
5. Open **Test Lab → Full celebration → Home power play + sound** (or Away). Confirm **PP audio: playback started** and that screen and selected lights act independently. If sound is not audible, inspect Activity and the S3 codec / PA diagnostics. Audio previews in the library do not fire lights.

## Real-game acceptance

- Follow/arm a matchup before its power play starts; verify a reliable, penalty-validated **Power play started** event occurs. NHL `situation` alone, a pulled goalie, or an unverified 6-on-5 cannot trigger this event.
- On that event, the assigned custom WAV plays once, alongside eligible visual/LED/WLED PP effects. The logged row says **Power-play audio: Playback started**; after completion it should say **Stream completed (speaker not verified)**. Physical audibility requires hardware verification.
- Unfollowed league-wide display-only power plays show no sound (**Not armed for this game**). PP end and brief feed dropouts do not play the announcement; repeated identical snapshots do not retrigger it.
- A full PP rehearsal exercises the audio without arming a game or fabricating a live NHL event.
- If the WAV is missing, unmounted or corrupt, the event reports **Audio device unavailable** (no goal horn fallback). Deleting the assigned slot disables only PP audio, while deleting the goal slot follows the existing built-in goal fallback.
- The S3 ES8311 does not mix two WAVs. When goal and PP sound overlap, the existing single-stream/cooldown guard can reject the second request with **Playback busy or in cooldown**; inspect Activity to distinguish this from a driver failure. Goal horn behavior and source selection are otherwise unchanged.

### Validation status
CI should run the PP audio source contract, the existing full-goal and live-goal contracts, the web JavaScript/DOM checks, and both firmware builds. Real NHL PP audio, physical acoustics, and goals that arrive during a PP announcement remain hardware acceptance tests.
