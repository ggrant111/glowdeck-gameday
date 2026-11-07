#!/usr/bin/env python3
"""WLED 8A contract: persist first four slots, eight-device readiness,
no NHL eligibility change, authenticated probes, same-subnet scan only."""
from pathlib import Path
m=Path("src/main.cpp").read_text()
ui=Path("include/WebUi.h").read_text()
e=Path("include/GameEventLog.h").read_text()
p=Path("include/WledDiscoveryPolicy.h").read_text()
assert "MAX_WLED = WledDiscoveryPolicy::kMaxDevices" in m
assert "kMaxDevices=8" in p
assert "constexpr uint8_t kDevices = 8" in e
assert 'prefs.getString(("ip" + String(i)).c_str(), "")' in m
assert 'prefs.getBool(("enable" + String(i)).c_str(), false)' in m
assert 'prefs.getUChar(("event" + String(i)).c_str(),' in m
assert 'prefs.getString(("wname" + String(i)).c_str(), "")' in m
assert 'prefs.putString(("wname" + String(i)).c_str(), cfg.wledName[i]);' in m
assert 'd["name"] = cfg.wledName[i];' in m
for route in ('server.on("/api/wled/discovery", HTTP_GET, []() {',
              'server.on("/api/wled/discover", HTTP_POST, []() {',
              'server.on("/api/wled/probe", HTTP_POST, []() {'):
    assert route in m,route
    code=m.split(route,1)[1].split('server.on(',1)[0]
    assert "if(!authorized())return;" in code,route
    for forbidden in ("startWledEvent(", "postWled(", "celebrate(",
                      "queueGameOverlay(", "s3Audio.play()", "selectGame("):
        assert forbidden not in code,(route,forbidden)
assert 'MDNS.queryService("wled","tcp")' in m
assert '"http://"+ip+"/json/info"' in m
assert "validWledLocalIp(addr)" in m and "WledDiscoveryPolicy::sameSubnet(" in m
assert "wledScanPendingMdns=true;" in m
assert "tickWledDiscovery();" in m
assert "wledActiveEvent!=CelebrationEngine::Event::NONE" in m
assert "if(restoring[i])return;" in m
assert 'doc["maxSaved"]=MAX_WLED;' in m
assert 'x["savedSlot"]=-1;' in m
assert 'response["version"]=info.version;' in m
assert "if(!probeWledInfo(ip,info))" in m
assert 'wledGoalStyle' in ui and 'wledGoalStyle' in m
assert 'id="wledScan"' in ui and 'id="wledManualCheck"' in ui
assert 'id="wledDiscovered"' in ui and 'id="wledManualIp"' in ui
assert '/api/wled/discover' in ui and '/api/wled/discovery' in ui
assert '/api/wled/probe' in ui
assert "disabled=false;" in ui # no automatic enable on discovery
assert 'enabled:"+device.enabled' not in ui # avoid unexpected
assert 'if(!target.exists)$("enabled"+i).checked=false;' in ui
assert 'length:(currentConfig?.wled?.length||8)' in ui
assert 'name:$("wname"+i).value.trim()' in ui
assert "function saveConfig()" in ui
assert 'startWledEvent(CelebrationEngine::Event::GOAL' in m
assert 'const bool armedForGoal = g.id == selectedGameId' in m
assert 'const bool followed = rearFollows(g.id)' in m
assert 'bool wledParticipates(uint8_t i, CelebrationEngine::Event event)' in m
assert 'CelebrationEngine::eligible(cfg.wledEnabled[i], cfg.wledEvents[i], event)' in m
print("PASS WLED 8A: verified, local, opt-in discovery; eight migrated "
      "slots; no extra NHL or WLED triggers")
