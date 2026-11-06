#!/usr/bin/env python3
"""Generate a crisp, dark 480x320 NHL-neutral arena broadcast background.

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
ROOT = Path(__file__).resolve().parent.parent
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

# Dim abstract arena rafters and spotlights, leaving all information areas clean.
for x in range(-40, 540, 35):
    line(x, 0, 240 + (x - 240) * .22, 108, (37, 56, 66), .24)
for y in (9, 19, 33):
    line(0, y, 480, y, (43, 65, 72), .26)
for x in (35, 92, 388, 445):
    glow(x, 13, 38, (56, 98, 109), .14)
    line(x, 7, 240 + (x - 240) * .64, 75, (51, 84, 94), .16)
for x in range(10, 470, 19):
    put(x, 15, (60, 86, 99), .44)
    put(x + 1, 15, (34, 52, 64), .3)

line(0, 77, 165, 109, (42, 66, 74), .33)
line(480, 77, 315, 109, (42, 66, 74), .33)
line(0, 256, 167, 210, (34, 60, 73), .24)
line(480, 256, 313, 210, (34, 60, 73), .24)
line(19, 272, 461, 272, (34, 58, 68), .38)
line(33, 284, 447, 284, (23, 47, 58), .35)

# Scoreboard plates are intentionally empty. Team marks, time, scores and SOG
# are rendered live by src/main.cpp; no labels/images are baked into this asset.
rounded(153, 44, 327, 98, 12, (11, 21, 31), (46, 71, 80), 1)
rect(177, 45, 304, 47, (49, 88, 100), .7)
rounded(15, 87, 153, 220, 13, (11, 21, 32), (42, 65, 76), 2)
rounded(327, 87, 465, 220, 13, (11, 21, 32), (42, 65, 76), 2)
rounded(154, 102, 326, 209, 12, (6, 15, 25), (48, 76, 89), 2)
line(168, 103, 312, 103, (51, 82, 96), .65)
line(168, 208, 312, 208, (42, 72, 85), .48)
line(28, 88, 137, 88, (45, 74, 86), .38)
line(342, 88, 452, 88, (45, 74, 86), .38)

# Recessed logo plinths keep official marks crisp and legible.
for cx in (86, 394):
    for y in range(103, 187):
        for x in range(cx - 43, cx + 44):
            if (x - cx)**2 + (y - 144)**2 < 42**2:
                put(x, y, (16, 30, 42), .62)
    ellipse(cx, 144, 43, 43, (49, 75, 85), .45)
    ellipse(cx, 144, 40, 40, (19, 43, 56), .6)
line(38, 194, 134, 194, (48, 78, 88), .45, 2)
line(346, 194, 442, 194, (48, 78, 88), .45, 2)

# Flat, high-contrast lower statistics plate, no busy circuitry.
rounded(45, 217, 435, 270, 11, (11, 22, 31), (42, 70, 81), 1)
rounded(172, 221, 308, 266, 8, (9, 20, 29), (37, 62, 71), 1)
line(170, 226, 170, 261, (49, 71, 81), .6)
line(309, 226, 309, 261, (49, 71, 81), .6)

# Compact, understated footer and follow affordance.
rect(0, 285, 480, 320, (5, 12, 19), .57)
line(0, 285, 480, 285, (28, 46, 55), .38)
line(14, 315, 114, 315, (30, 66, 79), .24)
line(366, 315, 466, 315, (30, 66, 79), .24)
rounded(374, 4, 424, 46, 10, (14, 26, 36), (43, 70, 80), 1)

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
    "// Phase 6B.1 native 480x320 RGB565BE - no scores or logos baked in.\n"
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
