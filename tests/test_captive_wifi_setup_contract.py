#!/usr/bin/env python3
"""Ensure QR captive sign-in opens actionable Wi-Fi setup, not NHL overview."""
from pathlib import Path

root=Path(__file__).resolve().parents[1]
main=(root/"src/main.cpp").read_text(encoding="utf-8")
ui=(root/"include/WebUi.h").read_text(encoding="utf-8")

# Captive redirect and offline AP homepage both reach the same Wi-Fi setup.
assert main.count('server.sendHeader("Location", "http://192.168.4.1/?setup=wifi#settings");')==2
assert 'requestFromSetupAp() && WiFi.status()!=WL_CONNECTED' in main
assert '!server.hasArg("setup")' in main
assert 'if (!authorized()) return;' in main
assert 'if (!requestFromSetupAp())' in main

# Must navigate to EXISTING form; no second credentials endpoint is introduced.
assert 'settings:{page:"device",anchor:"zone-network"}' in ui
assert 'id="ssid"' in ui and 'id="wifiPassword"' in ui
assert 'id="scanWifi"' in ui and 'id="deviceSave"' in ui
assert 'id="wifiOnboardingPrompt"' in ui
assert 'Select <strong>Find nearby networks</strong>' in ui
assert 'Save device settings' in ui
assert 'new URLSearchParams(location.search).get("setup")==="wifi"' in ui
assert 'tab("settings");' in ui
assert 'banner.hidden=false;' in ui
assert 'banner.style.display="block";' in ui
assert 'const initial=location.hash.slice(1);' in ui
# Startup must select Wi-Fi controls without waiting for optional NHL/Hue
# refreshes, which may fail or take a long time on a disconnected device.
init=ui[ui.index('(async()=>{\n  try{\n    mountPortalModules();'):]
assert init.index('const captiveSetup=')<init.index('await loadConfig();')
assert init.index('tab("settings");')<init.index('await loadConfig();')
assert init.index('await loadConfig();')<init.index('await refresh();')
# Still allow users to navigate to overview and keep the regular save API.
assert '$("deviceSave").onclick=()=>perform(saveConfig);' in ui
assert 'await post("/api/config",data);' in ui
assert 'setPortalDirty(false);' in ui
print("PASS: captive portal Wi-Fi deep link, immediate navigation and existing save flow")
