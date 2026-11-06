# ESP32-S3 hockey-goal GIF overlay

This S3-only feature plays the hockey GOAL + siren-light animation in sync with each goal overlay on the Waveshare ESP32-S3-Touch-LCD-3.5. The screen draws the looping GIF plus a color-coded goal border and the scoring team's live game score. The WLED celebration, priority queue and automatic return to the previous scoreboard are unchanged.

## Install the animation

1. Flash an ESP32-S3 build from this branch / a released main build that includes `GoalGifPlayer.h`. The **first** flash still requires USB; subsequent releases support OTA.
2. Connect your PC/phone to the board's network and open its GameDay dashboard, then open **Setup → Hockey goal GIF · ESP32-S3**.
3. Upload the provided **`GlowDeck_Goal_320x240.gif`** (320×240, 9 frames, ~1.26 seconds per loop). Maximum supported file size is 1MB. The optimized original user GIF can be downloaded from the ChatGPT conversation where the integration was created.
4. Tap **Preview GOAL on device** to verify playback, team-colored border and return to the previous screen. A real NHL goal uses the same animation automatically when goal overlays are enabled.
5. You can remove or replace the GIF from the dashboard without reflashing firmware. The GIF resides in the S3 LittleFS partition, separate from the application image, so ordinary OTA updates leave it in place.

If you only have the original 478×357 GIF, convert it first with Pillow:

```python
from PIL import Image, ImageOps
source = Image.open("Hockey Goal Siren Celebration Animation.gif")
frames = []
for i in range(source.n_frames):
    source.seek(i)
    image = ImageOps.fit(source.convert("RGB"), (320,240),
                         method=Image.Resampling.LANCZOS)
    frames.append(image.quantize(colors=64, method=Image.Quantize.MEDIANCUT,
                                 dither=Image.Dither.NONE).convert("RGB"))
frames[0].save("GlowDeck_Goal_320x240.gif", save_all=True,
               append_images=frames[1:], duration=140, loop=0,
               optimize=True, colors=64)
```

## Why the upload step?

The original GIF is a separate user-supplied asset and is not checked into the public repository. GIF upload allows the device to use that exact artwork without shipping it to every GitHub repo subscriber or enlarging every OTA firmware binary. The animation is retained across restarts and ordinary OTA updates. A full flash erase or LittleFS reformat will clear it.

## Support and safety

- Hardware: **Waveshare ESP32-S3-Touch-LCD-3.5 ST7796**, **not** 3.5B, and not the original CYD.
- Canvas: 320×240, scaled to 480×320 physical landscape orientation. GIF89a and GIF87a, file size max 1 MB.
- Storage: ESP32 LittleFS; upload is restricted to the authenticated admin dashboard and uses a temporary file that is validated before replacing the current GIF.
- Runtime: AnimatedGIF decoder handles scanlines and frame delays without blocking `loop()` between frames. The controller continues polling NHL, servicing web requests and restoring WLED states; any synchronous NHL request can still briefly delay GIF frames.
- Fallback: if no valid GIF is installed, the existing vector goal overlay still renders.
- This GIF is **visual only**. Siren lights in the GIF animate, but ESP32-S3 physical sound through the ES8311 codec is **not** enabled by this change, and existing S3 hardware horn limitations are unchanged. WLED lights continue independently.
- Testing: compile both PlatformIO environments, verify web syntax and policy tests in CI. Physical display, smoothness and touch need verification on the arrival-day hardware.

