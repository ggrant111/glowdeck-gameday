#!/usr/bin/env python3
"""Static integration contract for USB Improv Wi-Fi and on-screen hotspot QR."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
source = (root / "src/main.cpp").read_text(encoding="utf-8")
pio = (root / "platformio.ini").read_text(encoding="utf-8")
installer = (root / "web-installer/index.html").read_text(encoding="utf-8")
packer = (root / "scripts/package_web_release.py").read_text(encoding="utf-8")
release_test = (root / "tests/test_web_release.py").read_text(encoding="utf-8")

assert source.count("#include <ImprovWiFiLibrary.h>") == 1
assert source.count("#include <qrcode.h>") == 1
assert "ImprovWiFi improvSerial(&Serial);" in source
assert "improvSerial.setDeviceInfo(" in source
assert "ImprovTypes::CF_ESP32_S3" in source
assert "ImprovTypes::CF_ESP32" in source
assert 'http://{LOCAL_IPV4}/' in source
assert "improvSerial.handleSerial();" in source
assert "setCustomConnectWiFi(connectImprovWifi)" in source
assert "onImprovConnected(savedImprovWifi)" in source
assert 'prefs.putString("ssid",cfg.ssid);' in source
assert 'prefs.putString("wpass",cfg.wifiPassword);' in source
assert "if(WiFi.status()==WL_CONNECTED && WiFi.SSID()==String(ssid))" in source
assert "if(oldSsid.length())WiFi.begin(oldSsid.c_str(),oldPassword.c_str())" in source
assert "wledActiveEvent!=CelebrationEngine::Event::NONE" in source
assert source.find("setupImprovSerial();") < source.find("void loop()")
assert source.count("drawHotspotJoinQr(") == 3  # declaration + both board renderers
assert "WIFI:T:WPA;S:" in source and ";P:" in source
assert "qrcode_initText(&qr,modules,3,ECC_LOW" in source
assert "left+(x+4)*scale" in source
assert "top+(y+4)*scale" in source
assert "drawHotspotJoinQr(337,73,3)" in source
assert "drawHotspotJoinQr(198,42,3)" in source
assert 'if(cfg.ssid.isEmpty()){screen=Screen::SETUP;hotspotSetupShown=true;}' in source
assert "uint32_t(now-wifiOfflineSince)>=20000UL" in source
assert "screen=Screen::SETUP;" in source
assert "setupImprovSerial();" in source
assert pio.count("jnthas/Improv WiFi Library@0.0.4") == 2
assert pio.count("https://github.com/ricmoo/QRCode.git#") == 2
assert '"improv": True' in packer
assert '"new_install_improv_wait_time": 20' in packer
assert 'assert build["improv"] is True' in release_test
assert "Configure Wi-Fi" in installer and "Visit device" in installer
assert "QR" in installer and "192.168.4.1" in installer

# A QR remains optional, not a requirement to use USB serial, and existing
# SoftAP remains protected. No credential fields are emitted by Improv logging.
assert "WiFi.softAP(apSsid.c_str(), apPassword.c_str())" in source
provision_log = re.search(r"void savedImprovWifi\(.*?\n\}", source, re.DOTALL)
assert provision_log and 'prefs.putString("wpass",cfg.wifiPassword)' in provision_log.group(0)
assert 'password)' not in re.sub(r'prefs\.putString\("wpass",cfg\.wifiPassword\);','',provision_log.group(0).split('Serial.printf')[1])
print("PASS: USB Improv workflow, saved credentials, hotspot QR, responsive setup and recovery")
