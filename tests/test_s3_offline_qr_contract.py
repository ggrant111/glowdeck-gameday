#!/usr/bin/env python3
"""Regression: Waveshare S3's *active* Phase 6C setup must draw hotspot QR.

The legacy S3 Phase 5 setup screen already had a QR, but the production
Phase 6C renderer did not. This contract protects the actual render route.
"""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
main = (root / "src/main.cpp").read_text(encoding="utf-8")
pages = (root / "include/S3BroadcastPages.h").read_text(encoding="utf-8")
pio = (root / "platformio.ini").read_text(encoding="utf-8")

s3_flags = pio.split("[env:s3_waveshare_35]", 1)[1]
assert "-D GLOWDECK_S3_PHASE6C_UI=1" in s3_flags, "Production S3 must enable Phase 6C"
assert '#include "S3BroadcastPages.h"' in main
assert re.search(r'#if defined\(GLOWDECK_S3_PHASE6C_UI\).*?'
                 r'if\(screen==Screen::SETUP\) \{ drawS3CSetup\(\); return; \}',
                 main, re.S), "Active S3 setup must route to Phase 6C"

start = pages.index("void drawS3CSetup(){")
end = pages.index("void drawS3CTouchCheck(){", start)
setup = pages[start:end]
offline = re.search(r'if\(!setupWifiOnline\(\)\)\{(.*?)\}\s*else\s*\{',
                    setup, re.S)
assert offline, "S3 Phase 6C setup requires disconnected Wi-Fi branch"
assert 'drawHotspotJoinQr(337,73,3);' in offline.group(1), (
    "Offline QR must be drawn on active S3 Phase 6C setup")
assert '"AP: "+apSsid' in offline.group(1)
assert '"Open 192.168.4.1"' in offline.group(1)
assert '"KEY: "+apPassword' in offline.group(1)
connected = setup.split('}else{', 1)[1]
assert '"Wi-Fi: "+ssid' in connected, "Connected layout must keep LAN details"

# Real-world initial boot: don't rotate empty NHL data while waiting for STA.
assert 'if(WiFi.status()!=WL_CONNECTED){screen=Screen::SETUP;hotspotSetupShown=true;}' in main
assert 'uint32_t(now-wifiOfflineSince)>=20000UL' in main, (
    "Offline recovery after connection loss must remain")
assert 'if(screen==Screen::SETUP) { drawS3CSetup(); return; }' in main
# Shared QR is independently confirmed by original CYD hardware testing.
assert '"WIFI:T:WPA;S:"+apSsid+";P:"+apPassword+";;"' in main
assert 'qrcode_initText(&qr,modules,3,ECC_LOW,payload.c_str())' in main
assert 'left+(x+4)*scale' in main and 'top+(y+4)*scale' in main
# Version 3 QR: (29 data modules + four-module quiet border on both sides)*3.
qr_width = (29 + 8) * 3
assert 337 + qr_width <= 468 and 73 + qr_width <= 201, (
    "QR must fit inside S3 Setup card including white quiet zone")
assert "bool setupWifiOnline() { return WiFi.status() == WL_CONNECTED; }" in main
assert "WiFi." not in pages, "S3 renderer should not perform network operations"
assert "ImprovWiFi improvSerial(&Serial);" in main, (
    "Preserve physical S3 USB provisioning confirmed by user")
print("PASS: production S3 Phase 6C offline QR, recovery, geometry and USB Wi-Fi")
