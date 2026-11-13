#!/usr/bin/env python3
"""Contract: QR join -> AP captive HTTP portal, scoped to WPA2 setup subnet.

This is an integration/source contract, not real iOS/Android popup proof.
"""
from pathlib import Path
import re

root=Path(__file__).resolve().parents[1]
s=(root/"src/main.cpp").read_text(encoding="utf-8")
page=(root/"include/S3BroadcastPages.h").read_text(encoding="utf-8")
pio=(root/"platformio.ini").read_text(encoding="utf-8")
installer=(root/"scripts/package_web_release.py").read_text(encoding="utf-8")

# Both display builds use one network loop. Keep their working QR as a plain
# Wi-Fi join payload; an OS captive portal opens the browser *after* Wi-Fi joins.
assert '#include <DNSServer.h>' in s
assert 'DNSServer captiveDns;' in s
assert 'bool captiveDnsReady=false;' in s
assert 'captiveDns.start(53, "*", WiFi.softAPIP())' in s
assert 'if(captiveDnsReady)captiveDns.processNextRequest();' in s
assert 'WiFi.softAP(apSsid.c_str(), apPassword.c_str())' in s
assert '"WIFI:T:WPA;S:"+apSsid+";P:"+apPassword+";;"' in s
assert "drawHotspotJoinQr(198,42,3)" in s
assert "drawHotspotJoinQr(337,73,3)" in page
assert "jnthas/Improv WiFi Library@0.0.4" in pio
assert "GameDayImprovSerial improvSerial(Serial);" in s
assert '"improv": True' in installer

# Captive portal detectors are registered for iOS/Android/Windows, but only
# requests *actually* originating from the AP get the redirect. An external
# Host header must never be used as an auth/redirect shortcut.
registration=s[s.index("void registerRoutes() {"):s.index('  server.on("/", HTTP_GET')]
for probe in ("/generate_204", "/gen_204", "/hotspot-detect.html",
              "/canonical.html", "/ncsi.txt", "/connecttest.txt",
              "/redirect", "/fwlink"):
    assert '"'+probe+'"' in registration
assert "server.on(path, HTTP_GET, sendCaptiveSetupRedirect);" in registration
redirect=s[s.index("void sendCaptiveSetupRedirect() {"):s.index("void sendJson(")]
assert 'if (!requestFromSetupAp())' in redirect
assert 'if (!authorized()) return;' in redirect
assert 'server.send(302, "text/plain"' in redirect
assert 'server.sendHeader("Location", "http://192.168.4.1/?setup=wifi#settings");' in redirect
# iOS may discard the fragment: query parameter remains usable by the UI.
assert 'http://192.168.4.1/?setup=wifi' in redirect
root=s[s.index('  server.on("/", HTTP_GET'):s.index('  // Read-only, authenticated')]
assert 'if (!authorized()) return;' in root
assert 'requestFromSetupAp() && WiFi.status()!=WL_CONNECTED' in root
assert '!server.hasArg("setup")' in root  # guard redirect loop
assert 'server.send_P(200, "text/html; charset=utf-8", WEB_UI);' in root
assert 'http://192.168.4.1/?setup=wifi#settings' in root
assert "NoError" in s

# Existing authenticated dashboard/OTA behavior must be intact on home LAN.
# The offline AP root now redirects to setup, but authenticated LAN still
# receives the unchanged HTML UI rather than a captive redirect.
assert 'if (!authorized()) return;' in root
assert 'server.send_P(200, "text/html; charset=utf-8", WEB_UI);' in root
assert re.search(r'return requestFromSetupAp\\(\\)\\s*\\|\\|\\s*server\\.authenticate\\("admin", cfg\\.adminPassword\\.c_str\\(\\)\\);', s)
notfound=s[s.index("  server.onNotFound([]() {"):s.index("  server.begin();")]
assert "requestFromSetupAp() && server.method()==HTTP_GET" in notfound
assert '!server.uri().startsWith("/api/")' in notfound
assert 'if (!authorized()) return;' in notfound
assert 'errorResponse("Not found", 404);' in notfound
assert 'server.on("/api/update", HTTP_POST' in s
assert "DNSServer" not in page
print("PASS: captive DNS, AP-only detector redirects, preserved authenticated LAN/API, both QR screens")
