#!/usr/bin/env python3
"""8E DDP Quick Setup contract: safe one-screen setup without changing NHL rules."""
from pathlib import Path
s=Path("src/main.cpp").read_text()
ui=Path("include/WebUi.h").read_text()
start=s.index('  server.on("/api/wled/ddp/setup", HTTP_GET, []() {')
save=s.index('  server.on("/api/wled/ddp/quick-setup", HTTP_POST, []() {')
old=s.index('  server.on("/api/wled/ddp/config", HTTP_POST, []() {')
inspect=s[start:save]
quick=s[save:old]
assert 'if(!authorized())return;' in inspect
assert 'if(!authorized())return;' in quick
for key in (
    'probeWledInfo(cfg.wled[slot],info)',
    'getWledState(cfg.wled[slot])',
    'readWledDdpLayout(snapshot,info.ledCount,0,',
    'WledZonePolicy::NEUTRAL,layout,reason',
    'WledDdpPolicy::validate(',
    'result["ready"]=true;',
    'result["reason"]=reason;',
    'info.live','info.rgbw','WledDdpPolicy::kMaxPixels',
    'restoring[slot]','wledDdp[slot].active',
):
    assert key in inspect,key
for forbidden in ('prefs.put','postWled(', 'sendWledDdpFrame(',
                  'scheduleWledRestoreAll(', 'startWledEvent('):
    assert forbidden not in inspect,forbidden
for required in (
    'request["exclusiveConfirmed"]|false',
    'mode<0||mode>3','WledDdpPolicy::validFps(fps)',
    'mode==0&&!cfg.wledZoneEnabled[slot]',
    'readWledDdpLayout(snapshot,info.ledCount,cfg.wledZoneMap[slot],',
    'WledDdpPolicy::validate(',
    'const uint32_t oldMap=cfg.wledZoneMap[slot];',
    'const bool oldZones=cfg.wledZoneEnabled[slot];',
    'const bool oldDdp=cfg.wledDdpEnabled[slot];',
    'const bool oldEnabled=cfg.wledEnabled[slot];',
    'const uint8_t oldFps=cfg.wledDdpFps[slot];',
    'if(mode!=0){',
    'WledZonePolicy::assign(newMap,layout.ranges[j].id,',
    'cfg.wledZoneMap[slot]=newMap;',
    'cfg.wledZoneEnabled[slot]=true;',
    'cfg.wledEnabled[slot]=true;',
    'prepareWledDdp(uint8_t(slot),',
    'if(!prepared){',
    'prefs.putUInt(("zoneMap"+suffix).c_str(),newMap)',
    'prefs.putBool(("zoneOn"+suffix).c_str(),true)',
    'prefs.putUChar(("ddpFps"+suffix).c_str(),uint8_t(fps))',
    'prefs.putBool(("ddpOn"+suffix).c_str(),true)',
    'prefs.putBool(("enable"+suffix).c_str(),true)',
    'if(!saved){',
    'prefs.putBool(("ddpOn"+suffix).c_str(),oldDdp)',
    'result["chunks"]=WledDdpPolicy::packetCount(info.ledCount);',
):
    assert required in quick,required
for forbidden in ('startWledEvent(', 'sendWledDdpFrame(', 'celebrate(',
                  'setGameFollow(', 'postWled(', 'scheduleWledRestoreAll('):
    assert forbidden not in quick,forbidden
for required in (
    'id="wledDdpTestCard"',
    '"Set up light streaming"',
    'async function openWledDdpSettings(slot,host){',
    '"/api/wled/ddp/setup?slot="',
    '"/api/wled/ddp/quick-setup"',
    'if(!confirm.checked)',
    'exclusiveConfirmed:true',
    'currentConfig.wled[slot].enabled=true',
    'currentConfig.wled[slot].zoneEnabled=true',
    'currentConfig.wled[slot].ddpEnabled=true',
    '["3","Both teams · entire strip (recommended)"]',
    '["0","Keep my existing segment assignments"]',
    'data.mappingEnabled?"0":"3"',
    'data.pixels>=1000?10:15',
    '"Enable & test for 3 seconds"',
    'await post("/api/wled/ddp/test",{slot})',
    '"Turn off DDP"',
    '"Stop & restore lights"',
):
    assert required in ui,required
# Test setup's existing manual DD​​P interface and the NHL pathways remain.
assert 'server.on("/api/wled/ddp/config", HTTP_POST' in s
assert 'server.on("/api/wled/ddp/test", HTTP_POST' in s
assert 'server.on("/api/wled/ddp/stop", HTTP_POST' in s
assert 'CelebrationEngine::eligible(cfg.wledEnabled[i], cfg.wledEvents[i], event)' in s
print("PASS Phase 8E: inspect-only preflight, consent, one-step save, preserved zones, reversible setup and direct test")
