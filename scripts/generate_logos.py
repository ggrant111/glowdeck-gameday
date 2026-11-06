#!/usr/bin/env python3
"""Fetch NHL SVG marks and pack native 4-bit assets for CYD and S3.

Prerequisites: pip install Pillow cairosvg
Generate:     python scripts/generate_logos.py
Output:       include/TeamLogos.h (generated; never hand-edit)

No logo is downloaded at runtime. CYD uses 56px and S3 uses 80px alpha-keyed
marks from the same JSON/CDN SVG source, selected at compile time.
Logos remain the trademarks of their respective NHL teams, not project artwork.
"""
from __future__ import annotations
import json
from io import BytesIO
from pathlib import Path
from urllib.request import Request, urlopen
from PIL import Image
import cairosvg

TEAMS = """ANA BOS BUF CGY CAR CHI COL CBJ DAL DET EDM FLA LAK MIN MTL NJD
NSH NYI NYR OTT PHI PIT SEA SJS STL TBL TOR UTA VAN VGK WSH WPG""".split()
SIZE = 56
S3_SIZE = 80
OUTPUT = Path(__file__).resolve().parent.parent / "include" / "TeamLogos.h"
BG = (4, 12, 22)  # color used to preblend translucent antialiasing edges


def scoreboard_logo_urls() -> dict[str, str]:
    """Prefer each team darkLogo (the variant for a dark-mode display)."""
    urls: dict[str, str] = {}
    req = Request("https://api-web.nhle.com/v1/score/now",
                  headers={"User-Agent": "GlowDeck-GameDay/0.2.3"})
    try:
        with urlopen(req, timeout=14) as response:
            data = json.loads(response.read(1_500_000))
        for game in data.get("games", []):
            for side in ("awayTeam", "homeTeam"):
                entry = game.get(side, {})
                abbr = entry.get("abbrev", "")
                logo = entry.get("darkLogo") or entry.get("logo", "")
                if (abbr in TEAMS and isinstance(logo, str)
                    and logo.startswith("https://assets.nhle.com/logos/nhl/svg/")
                    and logo.endswith(".svg")):
                    urls[abbr] = logo
        print(f"[NHL scoreboard] {len(urls)} official game dark-logo URLs available")
    except Exception as error:
        print(f"[NHL scoreboard] Logo lookup unavailable; use official CDN fallback: {error}")
    return urls


def load_logo(team: str, returned_logo: str | None) -> Image.Image:
    last_error = None
    # Prefer the NHL dark-mode artwork: score JSON "logo" usually points to
    # a *_light.svg asset intended for white backgrounds, making some logos
    # look inverted or lose detail against this CYD's dark arena UI.
    dark = f"https://assets.nhle.com/logos/nhl/svg/{team}_dark.svg"
    light = f"https://assets.nhle.com/logos/nhl/svg/{team}_light.svg"
    candidates = ([returned_logo] if returned_logo and "_dark.svg" in returned_logo else []) + [dark]
    if returned_logo and "_dark.svg" not in returned_logo:
        candidates.append(returned_logo)
    candidates.append(light)
    for url in dict.fromkeys(candidates):
        try:
            req = Request(url, headers={
                "User-Agent": "GlowDeck-GameDay/0.2 (hobby display; GitHub Actions)",
                "Accept": "image/svg+xml,*/*",
            })
            with urlopen(req, timeout=16) as response:
                source = response.read(200_000)
            if not source.lstrip().startswith((b"<svg", b"<?xml")):
                raise ValueError("Not an SVG")
            png = cairosvg.svg2png(bytestring=source, output_width=S3_SIZE * 4,
                                   output_height=S3_SIZE * 4)
            return Image.open(BytesIO(png)).convert("RGBA")
        except Exception as exc:
            last_error = exc
    raise RuntimeError(f"No NHL mark for {team}: {last_error}")


def encode(logo: Image.Image, size: int) -> tuple[list[int], bytes]:
    rgba = logo.resize((size, size), Image.Resampling.LANCZOS).convert("RGBA")
    # Build a 15-color palette plus a true transparent index zero. The old
    # renderer painted the transparent pixels as a solid blue/purple rectangle;
    # the 4-bit pushImage transparency path below leaves the arena visible.
    composed = Image.new("RGB", rgba.size, BG)
    composed.paste(rgba, (0, 0), rgba)
    quant = composed.quantize(colors=15, method=Image.Quantize.MEDIANCUT)
    native_palette = quant.getpalette() or []
    pal = [0]  # palette index zero is transparent, never a background fill
    for i in range(15):
        p = i * 3
        if p + 2 < len(native_palette):
            r, g, b = native_palette[p:p + 3]
            pal.append(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3))
        else:
            pal.append(0)
    alpha = list(rgba.getchannel("A").getdata())
    indexes = list(quant.getdata())
    packed = bytearray()
    for i in range(0, size * size, 2):
        hi = int(indexes[i]) + 1 if alpha[i] > 20 else 0
        lo = int(indexes[i + 1]) + 1 if alpha[i + 1] > 20 else 0
        packed.append((hi << 4) | lo)
    return pal, bytes(packed)


def main():
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    rows = []
    failed = []
    scoreboard_urls = scoreboard_logo_urls()
    for team in TEAMS:
        try:
            logo = load_logo(team, scoreboard_urls.get(team))
            pal, packed = encode(logo, SIZE)
            s3pal, s3packed = encode(logo, S3_SIZE)
            rows.append((team, pal, packed, s3pal, s3packed))
            print(f"[NHL logo] {team}: official SVG -> "
                  f"{len(packed)} CYD bytes, {len(s3packed)} S3 bytes")
        except Exception as e:
            failed.append((team, str(e)))
            print(f"[NHL logo] {team}: FAILED: {e}")
    if len(rows) != len(TEAMS):
        raise SystemExit(f"Missing {len(failed)} required logos: {[x[0] for x in failed]}")
    lines = [
        "// AUTOGENERATED by scripts/generate_logos.py - DO NOT EDIT.",
        "// Source: score JSON official logo URLs and assets.nhle.com; NHL/team trademarks.",
        "#pragma once",
        "#include <Arduino.h>",
        "#include <TFT_eSPI.h>",
        "#include <pgmspace.h>",
    ]
    def add_assets(size: int, palette_col: int, pixels_col: int) -> None:
        lines.extend([
            f"static constexpr uint8_t TEAM_LOGO_SIZE = {size};",
            "struct TeamLogoAsset {",
            "  char abbrev[4];",
            "  uint16_t palette[16];",
            f"  uint8_t pixels[{size * size // 2}];",
            "};",
            "static const TeamLogoAsset TEAM_LOGOS[] PROGMEM = {",
        ])
        for row in rows:
            team, pal, packed = row[0], row[palette_col], row[pixels_col]
            lines.append("  {\"" + team + "\",")
            lines.append("    {" + ", ".join("0x%04X" % c for c in pal) + "},")
            lines.append("    {")
            for i in range(0, len(packed), 32):
                lines.append("      " + ", ".join("0x%02X" % b for b in packed[i:i+32]) + ",")
            lines.append("    }},")
        lines.extend([
            "};",
            "static_assert(TEAM_LOGO_SIZE * TEAM_LOGO_SIZE == sizeof(TEAM_LOGOS[0].pixels) * 2, \"Logo pixel stride mismatch\");",
        ])

    # Each target compiles only its own resolution, preserving the original
    # CYD asset size and fast non-sprite renderer.
    lines.append("#ifdef GLOWDECK_WAVESHARE_S3")
    add_assets(S3_SIZE, 3, 4)
    lines.append("#else")
    add_assets(SIZE, 1, 2)
    lines.append("#endif")
    lines.extend([
        "inline void drawTeamLogo(TFT_eSPI &display, const String &abbr, int16_t x, int16_t y) {",
        "  for (uint8_t i = 0; i < sizeof(TEAM_LOGOS) / sizeof(TEAM_LOGOS[0]); ++i) {",
        "    const TeamLogoAsset &logo = TEAM_LOGOS[i];",
        "    if (abbr != String(logo.abbrev)) continue;",
        "    // Correctly unpack each nibble from flash and convert palette entries", 
        "    // to RGB565. The old 4-bpp pushImage overload rendered horizontal",
        "    // stripes on the actual CYD. Ordinary RGB565 one-row transfers work.",
        "    uint16_t rowPixels[TEAM_LOGO_SIZE];",
        "    // RGB565 palette values are ordinary host-endian uint16_t words.",
        "    // On ESP32, pushImage() needs swapping to transmit those words",
        "    // in display byte order. fillRect()/drawPixel() are unaffected.",
        "    const bool previousSwapBytes = display.getSwapBytes();",
        "    display.setSwapBytes(true);",
        "    display.startWrite();",
        "    for (uint8_t py = 0; py < TEAM_LOGO_SIZE; ++py) {",
        "      int16_t runStart = -1;",
        "      uint8_t runLen = 0;",
        "      for (uint8_t px = 0; px <= TEAM_LOGO_SIZE; ++px) {",
        "        uint8_t index = 0;",
        "        if (px < TEAM_LOGO_SIZE) {",
        "          const uint16_t linear = uint16_t(py) * TEAM_LOGO_SIZE + px;",
        "          const uint8_t packed = pgm_read_byte(&logo.pixels[linear >> 1]);",
        "          index = (linear & 1) ? (packed & 0x0F) : (packed >> 4);",
        "        }",
        "        if (index > 0 && index < 16) {",
        "          if (runStart < 0) runStart = px;",
        "          rowPixels[runLen++] = pgm_read_word(&logo.palette[index]);",
        "        } else if (runStart >= 0) {",
        "          display.pushImage(x + runStart, y + py, runLen, 1, rowPixels);",
        "          runStart = -1;",
        "          runLen = 0;",
        "        }",
        "      }",
        "    }",
        "    display.endWrite();",
        "    display.setSwapBytes(previousSwapBytes);",
        "    return;",
        "  }",
        "  display.drawRoundRect(x, y, TEAM_LOGO_SIZE, TEAM_LOGO_SIZE, 8, TFT_DARKGREY);",
        "  display.setTextDatum(MC_DATUM);",
        "  display.setTextColor(TFT_WHITE);",
        "  display.drawString(abbr, x + TEAM_LOGO_SIZE / 2, y + TEAM_LOGO_SIZE / 2, 2);",
        "  display.setTextDatum(TL_DATUM);",
        "}",
        "#ifdef GLOWDECK_WAVESHARE_S3",
        "// On S3 the framebuffer is a TFT_eSprite. Base-class TFT_eSPI::",
        "// pushImage() writes directly to LCD and is overwritten by pushSprite().",
        "// This overload draws *inside* the framebuffer using sprite drawPixel.",
        "// A destination size permits native 80px hero marks and smaller list logos.",
        "inline void drawTeamLogo(TFT_eSprite &canvas, const String &abbr,",
        "                         int16_t x, int16_t y, uint8_t size = TEAM_LOGO_SIZE) {",
        "  if (!size) return;",
        "  for (uint8_t i = 0; i < sizeof(TEAM_LOGOS)/sizeof(TEAM_LOGOS[0]); ++i) {",
        "    const TeamLogoAsset &logo = TEAM_LOGOS[i];",
        "    if (abbr != String(logo.abbrev)) continue;",
        "    for (uint16_t dy = 0; dy < size; ++dy) {",
        "      const uint16_t sy = uint32_t(dy) * TEAM_LOGO_SIZE / size;",
        "      for (uint16_t dx = 0; dx < size; ++dx) {",
        "        const uint16_t sx = uint32_t(dx) * TEAM_LOGO_SIZE / size;",
        "        const uint16_t n = sy * TEAM_LOGO_SIZE + sx;",
        "        const uint8_t packed = pgm_read_byte(&logo.pixels[n >> 1]);",
        "        const uint8_t index = (n & 1) ? (packed & 15) : (packed >> 4);",
        "        if (index > 0 && index < 16)",
        "          canvas.drawPixel(x + dx, y + dy,",
        "                           pgm_read_word(&logo.palette[index]));",
        "      }",
        "    }",
        "    return;",
        "  }",
        "  // Unknown/renamed team code: make the failure visible, not a blank box.",
        "  canvas.drawRoundRect(x, y, size, size, 5, TFT_DARKGREY);",
        "  canvas.setTextDatum(MC_DATUM);",
        "  canvas.setTextColor(TFT_WHITE);",
        "  canvas.drawString(abbr, x + size/2, y + size/2, size >= 35 ? 2 : 1);",
        "  canvas.setTextDatum(TL_DATUM);",
        "}",
        "#endif",
    ])
    OUTPUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Generated {OUTPUT}: {len(rows)} logos, {OUTPUT.stat().st_size} bytes C++ source")


if __name__ == "__main__":
    main()
