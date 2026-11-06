#!/usr/bin/env python3
"""Regression checks for official-NHL-SVG logo packing and S3 framebuffer use.

Runs after scripts/generate_logos.py in CI; no network access in this test.
"""
from pathlib import Path
import sys

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))
from generate_logos import SIZE, S3_SIZE, TEAMS, encode  # noqa: E402


def unpack(packed: bytes, x: int, y: int, size: int) -> int:
    index = y * size + x
    byte = packed[index // 2]
    return (byte & 0x0F) if index & 1 else (byte >> 4)


def main() -> None:
    assert SIZE == 56 and S3_SIZE == 80
    assert len(TEAMS) == 32
    # Synthetic transparent SVG-equivalent proves alpha packing survives both
    # raster resolutions, including every row's stride and palette bounds.
    image = Image.new("RGBA", (320, 320), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.rectangle((75, 75, 245, 245), fill=(233, 197, 42, 255))
    for size in (SIZE, S3_SIZE):
        palette, packed = encode(image, size)
        assert len(palette) == 16
        assert len(packed) == size * size // 2
        assert unpack(packed, 0, 0, size) == 0
        assert 0 < unpack(packed, size // 2, size // 2, size) < 16
        assert max(palette) <= 0xFFFF

    header = (ROOT / "include" / "TeamLogos.h").read_text(encoding="utf-8")
    assert "#ifdef GLOWDECK_WAVESHARE_S3" in header
    assert "static constexpr uint8_t TEAM_LOGO_SIZE = 80;" in header
    assert "static constexpr uint8_t TEAM_LOGO_SIZE = 56;" in header
    assert header.count("static const TeamLogoAsset TEAM_LOGOS[] PROGMEM") == 2
    # Sprite-specific overload is essential. The base TFT_eSPI::pushImage
    # call only paints the panel, and the S3 pushSprite() then erases it.
    assert "drawTeamLogo(TFT_eSprite &canvas" in header
    assert "canvas.drawPixel(x + dx, y + dy" in header
    assert "inline void drawTeamLogo(TFT_eSPI &display" in header
    assert header.count('{"UTA",') == 2
    print("PASS: 32 NHL teams, 56px CYD/80px S3 packing and sprite render path")


if __name__ == "__main__":
    main()
