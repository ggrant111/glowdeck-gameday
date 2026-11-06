#!/usr/bin/env python3
"""Generate a de-boxed, dark 480x320 NHL-neutral arena background.

No third-party packages, no logos, no game data. PlatformIO invokes this
automatically for the Waveshare S3 target so the bitmap cannot become stale.
"""
from __future__ import annotations
import hashlib
import math
from pathlib import Path
import struct
import zlib

W, H = 480, 320
# PlatformIO executes pre: extra_scripts via SCons without __file__.
# Its exported construction environment supplies the true project root.
if "__file__" in globals():
    ROOT = Path(__file__).resolve().parent.parent
elif "env" in globals():
    ROOT = Path(env.subst("$PROJECT_DIR")).resolve()
else:
    ROOT = Path.cwd().resolve()
OUT_HEADER = ROOT / "include" / "S3ArenaEmbeddedGenerated.h"
OUT_PNG = ROOT / "assets" / "s3_arena_live_480x320.png"
pixels = bytearray(W * H * 3)


def clamp(v):
    return max(0, min(255, int(round(v))))


def put(x, y, c, a=1.0):
    if not (0 <= x < W and 0 <= y < H):
        return
    i = (y * W + x) * 3
    if a >= 1:
        pixels[i:i + 3] = bytes(c)
    elif a > 0:
        aa = min(1.0, a)
        for k in range(3):
            pixels[i + k] = clamp(pixels[i + k] * (1 - aa) + c[k] * aa)


def rect(x0, y0, x1, y1, c, a=1):
    for y in range(max(0, int(y0)), min(H, int(y1))):
        for x in range(max(0, int(x0)), min(W, int(x1))):
            put(x, y, c, a)


def line(x0, y0, x1, y1, c, a=1, thickness=1):
    n = max(1, math.ceil(max(abs(x1 - x0), abs(y1 - y0))))
    for i in range(n + 1):
        x = round(x0 + (x1 - x0) * i / n)
        y = round(y0 + (y1 - y0) * i / n)
        for dy in range(-(thickness // 2), thickness - thickness // 2):
            for dx in range(-(thickness // 2), thickness - thickness // 2):
                put(x + dx, y + dy, c, a)


def inside_round(x, y, x0, y0, x1, y1, r):
    if not (x0 <= x < x1 and y0 <= y < y1):
        return False
    qx = min(max(x, x0 + r), x1 - r - 1)
    qy = min(max(y, y0 + r), y1 - r - 1)
    return (x - qx)**2 + (y - qy)**2 <= r * r


def rounded(x0, y0, x1, y1, r, fill, border=None, bw=1):
    for y in range(max(0, y0), min(H, y1)):
        for x in range(max(0, x0), min(W, x1)):
            if inside_round(x, y, x0, y0, x1, y1, r):
                edge = border is not None and not inside_round(
                    x, y, x0 + bw, y0 + bw, x1 - bw, y1 - bw,
                    max(0, r - bw))
                put(x, y, border if edge else fill)


def soft_plate(x0, y0, x1, y1, r, color, opacity=0.80, feather=9):
    """Low-contrast filled plate with a feathered edge, NO border stroke.

    Keep the existing information geometry and touch zones, but visually
    integrate the surface into the arena instead of drawing boxed panels.
    """
    for y in range(max(0, y0), min(H, y1)):
        for x in range(max(0, x0), min(W, x1)):
            if not inside_round(x, y, x0, y0, x1, y1, r):
                continue
            d = min(x - x0, x1 - 1 - x, y - y0, y1 - 1 - y)
            edge = min(1.0, (d + 1) / float(feather))
            put(x, y, color, opacity * edge)


def soft_disc(cx, cy, radius, color, opacity=0.32):
    """Diffuse logo plinth with NO circular outline or sharp edge."""
    for y in range(max(0, cy - radius), min(H, cy + radius + 1)):
        for x in range(max(0, cx - radius), min(W, cx + radius + 1)):
            r2 = (x - cx) ** 2 + (y - cy) ** 2
            if r2 < radius * radius:
                fade = 1.0 - r2 / float(radius * radius)
                put(x, y, color, opacity * fade * fade)


def ellipse(cx, cy, rx, ry, color, alpha=1, thick=1):
    n = int(2 * math.pi * max(rx, ry) * 1.5)
    for i in range(n):
        theta = 2 * math.pi * i / n
        x = int(round(cx + math.cos(theta) * rx))
        y = int(round(cy + math.sin(theta) * ry))
        for shift in range(thick):
            put(x, y + shift, color, alpha)


def glow(cx, cy, rad, c, scale):
    for y in range(max(0, cy - rad), min(H, cy + rad)):
        for x in range(max(0, cx - rad), min(W, cx + rad)):
            d = ((x - cx)**2 + (y - cy)**2) / float(rad * rad)
            if d < 1:
                put(x, y, c, scale * (1 - d)**2)


# Neutral midnight-charcoal field. No broad blue haze or fluorescent blooms.
for y in range(H):
    for x in range(W):
        vertical = y / H
        edge = min(x, W - 1 - x) / 240
        hazy = 4 * (1 - edge) * (1 - vertical)
        noise = ((x * 37 + y * 97 + x * y * 3) % 17 - 8) * .14
        c = (7 + hazy + noise + 4 * vertical,
             13 + 2 * hazy + noise + 3 * vertical,
             21 + 2.2 * hazy + noise + 4 * vertical)
        put(x, y, tuple(clamp(v) for v in c))

# Arena presence stays at the edges. Softer beams and rafters avoid
# repeating hard outlines around the NHL content itself.
for x in range(-40, 540, 35):
    line(x, 0, 240 + (x - 240) * .22, 108, (37, 56, 66), .17)
for y in (9, 19, 33):
    line(0, y, 480, y, (43, 65, 72), .14)
for x in (35, 92, 388, 445):
    glow(x, 13, 38, (56, 98, 109), .10)
    line(x, 7, 240 + (x - 240) * .64, 75, (51, 84, 94), .10)
for x in range(10, 470, 19):
    put(x, 15, (60, 86, 99), .28)

# Keep all existing text/logo positions, but replace the outlined boxes with
# feathered dark regions. No decorative border stroke along any of these
# surfaces. The score gets the darkest center; time remains readable.
soft_plate(153, 44, 327, 98, 13, (9, 20, 28), .84, 11)
soft_plate(15, 87, 153, 220, 15, (9, 19, 28), .71, 15)
soft_plate(327, 87, 465, 220, 15, (9, 19, 28), .71, 15)
soft_plate(154, 102, 326, 209, 14, (5, 13, 21), .91, 12)

# Logo plinths are soft radial tonal pockets, NOT outlined rings. Team marks
# will still be rendered sharply from the NHL SVGs in the dynamic sprite.
for cx in (86, 394):
    soft_disc(cx, 144, 48, (30, 48, 58), .26)

# The existing runtime team-primary accent bars at y=189 are retained.
# One unified SOG / PP / status lower third, with only faint dividers.
soft_plate(45, 217, 435, 270, 11, (11, 23, 32), .83, 10)
line(168, 229, 168, 260, (53, 72, 78), .12)
line(311, 229, 311, 260, (53, 72, 78), .12)

# No framed bottom bar. A smooth vignette grounds the original GAMES,
# page indicator and SETUP typography without any visible top border.
for y in range(274, H):
    fade = (y - 274) / float(H - 274)
    rect(0, y, W, y + 1, (4, 10, 17), .32 * fade)

# The top-right follow star remains interactive with its original hit region,
# but it no longer sits in a bright, bordered rounded rectangle.
soft_disc(399, 24, 28, (19, 35, 43), .24)

# Full native RGB565 BIG ENDIAN. ESP32-S3 TFT_eSprite's framebuffer holds
# byte-swapped RGB565: these bytes can be inflated directly, with no upscale
# or per-pixel conversion at runtime.
raw = bytearray(W * H * 2)
for i in range(W * H):
    red, green, blue = pixels[i * 3:i * 3 + 3]
    rgb565 = ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3)
    raw[i * 2] = rgb565 >> 8
    raw[i * 2 + 1] = rgb565 & 255

compressed = zlib.compress(raw, 9)
sha = hashlib.sha256(raw).hexdigest()
crc = zlib.crc32(raw) & 0xffffffff
header = (
    "#pragma once\n"
    "// GENERATED by scripts/generate_s3_arena.py - do not edit.\n"
    "// Phase 6B.2A de-boxed 480x320 RGB565BE - no scores or logos baked in.\n"
    "namespace S3ArenaEmbedded {\n"
    f"constexpr int SourceW={W};\n"
    f"constexpr int SourceH={H};\n"
    "constexpr int RawBytes=SourceW*SourceH*2;\n"
    f"constexpr unsigned RawCrc32=0x{crc:08X}U;\n"
    f'constexpr const char* RawSha256="{sha}";\n'
    "static const uint8_t Compressed[] PROGMEM = {\n"
    + "".join("  " + ", ".join(f"0x{b:02X}" for b in compressed[i:i + 20])
              + ",\n" for i in range(0, len(compressed), 20))
    + "};\n} // namespace S3ArenaEmbedded\n"
)
OUT_HEADER.parent.mkdir(parents=True, exist_ok=True)
OUT_PNG.parent.mkdir(parents=True, exist_ok=True)
OUT_HEADER.write_text(header, encoding="utf-8")


def png_chunk(kind, data):
    chunk = kind + data
    return (struct.pack(">I", len(data)) + chunk
            + struct.pack(">I", zlib.crc32(chunk) & 0xffffffff))


rows = b"".join(b"\x00" + bytes(pixels[y * W * 3:(y + 1) * W * 3])
                for y in range(H))
png = (b"\x89PNG\r\n\x1a\n"
       + png_chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0))
       + png_chunk(b"IDAT", zlib.compress(rows, 9))
       + png_chunk(b"IEND", b""))
OUT_PNG.write_bytes(png)
print(f"[S3 Arena] native {W}x{H}, RGB565 {len(raw)}B, "
      f"flash asset {len(compressed)}B, sha256={sha}, PNG={OUT_PNG}")
