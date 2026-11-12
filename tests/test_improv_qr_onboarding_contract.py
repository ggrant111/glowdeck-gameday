#!/usr/bin/env python3
"""CYD Improv UART regression + offline QR fallback contract, preserving S3."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
source = (root / "src/main.cpp").read_text(encoding="utf-8")
pio = (root / "platformio.ini").read_text(encoding="utf-8")
host = (root / "include/GameDayImprovSerial.h").read_text(encoding="utf-8")
installer = (root / "web-installer/index.html").read_text(encoding="utf-8")
packer = (root / "scripts/package_web_release.py").read_text(encoding="utf-8")
release_test = (root / "tests/test_web_release.py").read_text(encoding="utf-8")

# Keep the already-working Waveshare S3 library and USB CDC unchanged.
assert '#ifdef GLOWDECK_WAVESHARE_S3\n// Preserve' in source
assert "#include <ImprovWiFiLibrary.h>" in source
assert "ImprovWiFi improvSerial(&Serial);" in source
assert "ImprovTypes::CF_ESP32_S3" in source
s3_section = pio.split("[env:s3_waveshare_35]")[1]
cyd_section = pio.split("[env:s3_waveshare_35]")[0]
assert "jnthas/Improv WiFi Library@0.0.4" in s3_section
assert "jnthas/Improv WiFi Library@0.0.4" not in cyd_section

# Corrected CYD USB/UART parser: no longer swallows the first byte of the
# second RPC when ESP Web Tools asks state, info, networks, then Wi-Fi.
assert '#include "GameDayImprovSerial.h"' in source
assert "GameDayImprovSerial improvSerial(Serial);" in source
assert "GameDayImprovSerial::ESP32" in source
assert "position_=0;" in host and "handleRpc(&buffer_[9],buffer_[8]);" in host
assert "void handleSerial()" in host
assert "case 2:" in host and "case 3:" in host and "case 4:" in host
assert "case 1:" in host and "localUrl(1)" in host
assert "uint8_t checksum" in host
assert "improvSerial.handleSerial();" in source
assert "setCustomConnectWiFi(connectImprovWifi)" in source
assert "onImprovConnected(savedImprovWifi)" in source
assert 'prefs.putString("ssid",cfg.ssid);' in source
assert 'prefs.putString("wpass",cfg.wifiPassword);' in source
assert "if(WiFi.status()==WL_CONNECTED && WiFi.SSID()==String(ssid))" in source
assert "if(oldSsid.length())WiFi.begin(oldSsid.c_str(),oldPassword.c_str())" in source
assert "wledActiveEvent!=CelebrationEngine::Event::NONE" in source
assert source.find("setupImprovSerial();") < source.find("void loop()")

# Original CYD must never display empty carousel screens while STA is offline.
assert "screen=Screen::SETUP;" in source
assert "if(WiFi.status()!=WL_CONNECTED && screen!=Screen::CALIBRATE)" in source
assert 'if(WiFi.status()!=WL_CONNECTED){screen=Screen::SETUP;hotspotSetupShown=true;}' in source
assert "uint32_t(now-wifiOfflineSince)>=20000UL" in source
assert source.count("drawHotspotJoinQr(") == 3
assert "WIFI:T:WPA;S:" in source and ";P:" in source
assert "qrcode_initText(&qr,modules,3,ECC_LOW" in source
assert "left+(x+4)*scale" in source
assert "top+(y+4)*scale" in source
assert "drawHotspotJoinQr(337,73,3)" in source
assert "drawHotspotJoinQr(198,42,3)" in source

assert pio.count("https://github.com/ricmoo/QRCode.git#") == 2
assert '"improv": True' in packer
assert '"new_install_improv_wait_time": 20' in packer
assert 'assert build["improv"] is True' in release_test
assert "Configure Wi-Fi" in installer and "Visit device" in installer
assert "QR" in installer and "192.168.4.1" in installer

# Setup AP stays password protected; client-side browser credentials remain
# separate from and do not overwrite existing GameDay user settings.
assert "WiFi.softAP(apSsid.c_str(), apPassword.c_str())" in source
callback = re.search(r"void savedImprovWifi\(.*?\n\}", source, re.DOTALL)
assert callback
assert 'prefs.putString("wpass",cfg.wifiPassword)' in callback.group(0)
log = callback.group(0).split("Serial.printf",1)[1].split(");",1)[0]
assert "wifiPassword" not in log and "password)" not in log
print("PASS: CYD Improv UART parser, S3 intact, offline QR and credentials policy")
