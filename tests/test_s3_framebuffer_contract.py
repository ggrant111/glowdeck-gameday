#!/usr/bin/env python3
"""Guard against inherited TFT_eSPI functions drawing outside the S3 sprite.

The original standings->stats bleed-through came from fillScreen() (and
rounded-rectangle primitives) reaching the physical ST7796, not the native
480x320 RGB565 TFT_eSprite that gets pushed afterwards.
"""
from pathlib import Path

root=Path(__file__).resolve().parent.parent
hardware=(root/"include/GameDayS3Hardware.h").read_text(encoding="utf-8")
main=(root/"src/main.cpp").read_text(encoding="utf-8")

required=[
    "void fillScreen(uint32_t color) { fillSprite(color); }",
    "void fillRoundRect(",
    "void drawRoundRect(",
    "void fillCircle(",
    "void fillTriangle(",
    "TFT_eSprite::drawFastHLine(",
    "TFT_eSprite::fillRect(",
    "TFT_eSprite::drawPixel(",
    "bool presentRegion(",
    "return pushSprite(region.x,region.y,region.x,region.y,region.w,region.h);",
]
for needle in required:
    assert needle in hardware, f"Missing sprite-safe override: {needle}"

for name in ("drawS3Stats", "drawS3Standings", "drawS3Scoreboard",
             "drawS3GamePicker", "drawS3EventStatic", "drawS3Setup"):
    # Accept normal signatures with arguments, but skip forward declarations.
    import re
    match=re.search(r"void " + name + r"\([^;\n]*\)\s*\{", main)
    assert match, f"Missing renderer body: {name}"
    start=match.start()
    body=main[start:main.index("\n}", start)]
    assert "tft.fillScreen(S3_BG);" in body, f"{name} must clear frame completely"

assert "s3ScoreOnlyFrame" in main
assert "tft.presentRegion(score.x,score.y,score.w,score.h);" in main
assert "tft.presentRegion(shots.x,shots.y,shots.w,shots.h);" in main
assert "else {\n      tft.present();\n    }" in main
print("S3 framebuffer safety: no LCD-only inherited fullscreen/rounded-shape path")
