#!/usr/bin/env python3
"""Phase 6B.1 native S3 arena asset, palette, RGB565 and cache regression tests.

Run scripts/generate_s3_arena.py first (CI + PlatformIO prebuild do this).
No Pillow, CairoSVG, network or hardware required for these checks.
"""
from pathlib import Path
import hashlib
import re
import struct
import zlib

ROOT = Path(__file__).resolve().parent.parent
H = ROOT / "include" / "S3ArenaEmbeddedGenerated.h"
PNG = ROOT / "assets" / "s3_arena_live_480x320.png"
assert H.exists() and PNG.exists(), "Native arena art was not generated"
header = H.read_text(encoding="utf-8")
assert "SourceW=480;" in header and "SourceH=320;" in header
payload = header.split("static const uint8_t Compressed[] PROGMEM = {", 1)[1].split("};", 1)[0]
compressed = bytes(int(h, 16) for h in re.findall(r"0x([A-Fa-f0-9]{2})", payload))
assert 5000 < len(compressed) < 100000, len(compressed)
raw = zlib.decompress(compressed)
assert len(raw) == 480 * 320 * 2 == 307200
sha = hashlib.sha256(raw).hexdigest()
# The source artwork evolves across Phase 6 releases. Its generated SHA must
# match the header and CRC, but should never pin the old (boxed) artwork.
assert re.fullmatch(r"[0-9a-f]{64}", sha), sha
assert f'RawSha256="{sha}"' in header
crc = zlib.crc32(raw) & 0xFFFFFFFF
assert f"RawCrc32=0x{crc:08X}U" in header

# Validate PNG output and compare color quantization at several geometry points.
png = PNG.read_bytes()
assert png.startswith(b"\x89PNG\r\n\x1a\n")
pos = 8
idat = bytearray()
width = height = 0
while pos < len(png):
    size = struct.unpack(">I", png[pos:pos+4])[0]
    kind = png[pos+4:pos+8]
    data = png[pos+8:pos+8+size]
    assert zlib.crc32(kind + data) & 0xFFFFFFFF == struct.unpack(">I", png[pos+8+size:pos+12+size])[0]
    if kind == b"IHDR":
        width, height, depth, mode, *_ = struct.unpack(">IIBBBBB", data)
        assert (width, height, depth, mode) == (480, 320, 8, 2)
    if kind == b"IDAT":
        idat.extend(data)
    pos += size + 12
rows = zlib.decompress(idat)
assert len(rows) == 320 * (1 + 480 * 3)

def rgb565be(x, y):
    offset = (y * 480 + x) * 2
    return raw[offset] << 8 | raw[offset + 1]

def rgb(x, y):
    offset = y * (480 * 3 + 1) + 1 + x * 3
    return tuple(rows[offset:offset+3])

for x, y in ((0, 0), (240, 144), (85, 142), (395, 142), (240, 237), (479, 319)):
    red, green, blue = rgb(x, y)
    expected = (red >> 3) << 11 | (green >> 2) << 5 | (blue >> 3)
    assert rgb565be(x, y) == expected, (x, y)
    # Underlying ESP32-S3 is little-endian. This exact byte array is already
    # the TFT_eSprite swapped RGB565 framebuffer representation.
    assert raw[(y * 480 + x) * 2:(y * 480 + x) * 2 + 2] == struct.pack(">H", expected)

# First hardware photo was over-bright and blue. Keep fixed artwork dark,
# mostly neutral, with a clean unlit score region for dynamic white text.
samples = [rgb(x,y) for y in range(0,320,5) for x in range(0,480,5)]
mean = [sum(p[i] for p in samples)/len(samples) for i in range(3)]
assert max(mean) < 38, mean
assert mean[2] - mean[0] < 22, mean
assert sum(rgb(240,150))/3 < 35
assert sum(rgb(85,144))/3 < 40
assert rgb(240,150) != rgb(85,144), "Foreground plates must remain separate"

# Phase 6B.2A: every previously hard-bordered panel is now a feathered
# tonal zone. Pixel gradients at former box edges must be smooth, with no
# high-contrast 1px outlines. These checks are against PNG pixels, independent
# of the RGB565 byte-order checks above.
def edge_jump(x, y):
    a, b = rgb(x - 1, y), rgb(x, y)
    return max(abs(a[i] - b[i]) for i in range(3))

for x, y in ((153, 68), (15, 150), (154, 157),
             (327, 157), (465, 150), (45, 246),
             (172, 244), (308, 244), (435, 246),
             (374, 24), (424, 24), (240, 285)):
    assert edge_jump(x, y) < 12, (x, y, edge_jump(x, y))

# The three old outlined SOG cards are now one unified strip; left/center/
# right share the same tonal fill. No separately bordered center PP rectangle.
lower_left = rgb(114, 245)
lower_center = rgb(240, 245)
lower_right = rgb(365, 245)
assert max(abs(lower_left[i] - lower_center[i]) for i in range(3)) < 12
assert max(abs(lower_right[i] - lower_center[i]) for i in range(3)) < 12

# The old crisp logo rings at r=43 are gone; a diffuse plinth remains.
for center in (86, 394):
    for delta in (40, 42, 43, 44, 46):
        assert edge_jump(center + delta, 144) < 12

# Removing the footer frame must preserve an uninterrupted soft gradient.
assert edge_jump(240, 285) < 12
assert edge_jump(240, 286) < 12

generator=(ROOT / "scripts" / "generate_s3_arena.py").read_text()
assert "soft_plate(" in generator and "soft_disc(" in generator
assert "Phase 6B.2A" in generator
assert "ellipse(cx, 144, 43" not in generator
assert "rounded(172, 221, 308, 266" not in generator
assert "line(0, 285, 480, 285" not in generator

# Byte order self-test covers red, green and blue, not just muted navy pixels.
for r, g, b, packed in ((255, 0, 0, 0xF800),
                         (0, 255, 0, 0x07E0),
                         (0, 0, 255, 0x001F)):
    code = (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3)
    assert code == packed
    assert struct.unpack("<H", struct.pack(">H", code))[0] == (
        ((packed << 8) | (packed >> 8)) & 0xFFFF)

source = (ROOT / "src" / "main.cpp").read_text()
loader = (ROOT / "include" / "S3ArenaBackground.h").read_text()
config = (ROOT / "platformio.ini").read_text()
assert 'cfg.invertScreen = prefs.getBool("invert", true);' in source
assert "tft.invertDisplay(cfg.invertScreen);" in source
assert "s3Arena.begin()" in source and "s3Arena.restore(tft)" in source
assert "tinfl_decompress_mem_to_mem(" in loader
assert "S3ArenaEmbedded::RawCrc32" in loader
assert "memcpy(framebuffer+pos,cache_+pos" in loader
assert "mix565(" not in loader, "No 192x128 bilinear resampling should remain"
assert "extra_scripts = pre:scripts/generate_s3_arena.py" in config
assert "-D GLOWDECK_S3_PHASE6_LIVE=1" in config
assert "void drawS3ArenaFollowStar(const Game &g)" in source
assert "drawS3ArenaFollowStar(g);" in source
assert "drawFollowStar(g,399,24,S3_BG);" not in source
# Accent bars and original dynamic labels remain outside the PNG.
assert "tft.fillRoundRect(36,189,105,3,1,away);" in source
assert "tft.fillRoundRect(339,189,105,3,1,home);" in source
assert 'tft.drawString("GAMES",39,300,2);' in source
assert 'tft.drawString("SETUP",443,300,2);' in source
print(f"PASS Phase 6B.2A de-boxed: 480x320 native RGB565, {len(compressed)} B compressed, "
      f"dark mean RGB={tuple(round(v,1) for v in mean)}, CRC={crc:08X}")
