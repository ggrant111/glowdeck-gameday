#!/usr/bin/env python3
"""Phase 6B bundled arena artwork smoke test (runs without Pillow/network)."""
from pathlib import Path
import base64
import hashlib
import re
import zlib

root=Path(__file__).resolve().parent.parent
text=(root/"include"/"S3ArenaEmbedded.h").read_text(encoding="utf-8")
payload=text.split("static const char CompressedBase64[] PROGMEM =",1)[1].split(";",1)[0]
chunks=re.findall(r'"([A-Za-z0-9+/=]+)"',payload)
assert len(chunks)>3
compressed=base64.b64decode("".join(chunks),validate=True)
assert 4000<len(compressed)<40000, len(compressed)
raw=zlib.decompress(compressed)
assert len(raw)==192*128*2, len(raw)
assert hashlib.sha256(raw).hexdigest()=="2474f1ecfaf02a9df88af79ab4eb13fbec48194626d51c4dc10979489cf8310c"
assert len(set(raw))>30
firmware=(root/"src"/"main.cpp").read_text(encoding="utf-8")
background=(root/"include"/"S3ArenaBackground.h").read_text(encoding="utf-8")
layout=(root/"include"/"S3NativeLayout.h").read_text(encoding="utf-8")
assert "s3Arena.begin()" in firmware
assert "s3Arena.restore(tft)" in firmware
assert "drawS3ArenaReadout" in firmware
assert "broadcastHitTest" in firmware and "broadcastHitTest" in layout
assert "tinfl_decompress_mem_to_mem" in background
assert "TFT_eSprite" not in background or "frameBuffer(1)" in background
assert "memcpy(framebuffer+offset,cache_+offset" in background
assert "-D GLOWDECK_S3_PHASE6_LIVE=1" in (root/"platformio.ini").read_text()
print(f"PASS Phase 6B: compressed arena asset {len(compressed)} B, raw 49152 B, safe restore/scoreboard contract")
