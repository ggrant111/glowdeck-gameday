#!/usr/bin/env python3
"""WLED 8B: segment targeting and isolation contracts."""
from pathlib import Path
s=Path("src/main.cpp").read_text()
ui=Path("include/WebUi.h").read_text()
p=Path("include/WledZonePolicy.h").read_text()
for key in ("constexpr uint8_t kMaxSegments=16","Role role(uint32_t packed,uint8_t id)",
            "uint32_t assign(uint32_t packed,uint8_t id,Role zone)",
            "if(!mappingEnabled)return true","if(scorerSide==kHomeSide)",
            "if(scorerSide==kAwaySide)","return false; // unknown side",
            "return z==HOME||z==AWAY||z==NEUTRAL","zoneColor(Role zone"):
    assert key in p,key
for key in (
  "bool wledZoneEnabled[MAX_WLED] = {}",
  "uint32_t wledZoneMap[MAX_WLED] = {}",
  'prefs.getBool(("zoneOn"+String(i)).c_str(),false)',
  'prefs.getUInt(("zoneMap"+String(i)).c_str(),0)',
  'd["zoneEnabled"] = cfg.wledZoneEnabled[i];',
  "uint16_t wledTouchedSegments[MAX_WLED] = {}",
  "bool wledRestoreMapped[MAX_WLED] = {}",
  "wledRestoreMapped[i]=cfg.wledZoneEnabled[i]",
  "wledTouchedSegments[i]|=changedMask;",
  "wledTouchedSegments[i]|=phaseTouched;",
  "wledMappedRestoreBody(savedState[i],wledTouchedSegments[i])",
  "wledTouchedSegments[i]=0;wledRestoreMapped[i]=false;",
  "WledZonePolicy::target(mapped,cfg.wledZoneMap[slot]",
  'if(mapped)return "";',
  'if(mapped && !(state["on"]|false))return "";',
  'if(mapped)out["bri"]=cfg.brightness',
  "wledEventSide=scorerSide;",
  "powerPlaySide=currentWledPowerPlay(powerPlayColor);",
  "side==0?WledZonePolicy::kHomeSide:WledZonePolicy::kAwaySide",
  "if(ESP.getFreeHeap()<45000UL)",
):
    assert key in s,key
for route,method in (("/api/wled/segments","GET"),
                     ("/api/wled/segment-config","POST"),
                     ("/api/wled/segment-test","POST")):
    anchor=f'server.on("{route}", HTTP_{method}, []() {{'
    assert anchor in s,route
    body=s.split(anchor,1)[1].split('server.on(',1)[0]
    assert "if(!authorized())return;" in body,route
    for forbidden in ("queueGameOverlay(", "selectGame(", "celebrate(",
                      "s3Audio.play(", "startWledEvent("):
        assert forbidden not in body,(route,forbidden)
inventory=s.split('server.on("/api/wled/segments"',1)[1].split('server.on(',1)[0]
assert "getWledState(cfg.wled[slot])" in inventory
for x in ('row["id"]','row["start"]','row["stop"]',
          'row["name"]','row["role"]','response["truncated"]'):
    assert x in inventory,x
save=s.split('server.on("/api/wled/segment-config"',1)[1].split('server.on(',1)[0]
for x in ('request["roles"].is<JsonArray>()',
          'roles.size()>WledZonePolicy::kMaxSegments',
          'WledZonePolicy::validRole(role)',
          'WledZonePolicy::assign(packed',
          'if(enabled&&!assigned)',
          'prefs.putUInt(("zoneMap"+String(slot)).c_str(),packed)',
          'prefs.putBool(("zoneOn"+String(slot)).c_str(),enabled)',
          "if(restoring[slot]"):
    assert x in save,x
preview=s.split('server.on("/api/wled/segment-test"',1)[1].split('server.on(',1)[0]
for x in ('if(restoring[slot]', 'if(!(state["on"]|false))',
          'if(!exists)return errorResponse("Segment not present',
          'wledRestoreMapped[slot]=true',
          'wledTouchedSegments[slot]=uint16_t(1U<<id)',
          'wledEventOwner[slot]=0',
          'restoreAt[slot]=millis()+1800UL',
          '"udpn"'):
    assert x in preview,x
assert 'const bool armedForGoal = isArmedGame(g.id)' in s
assert 'if (armedForGoal) celebrate(teamRgb(scorer)' in s
assert 'CelebrationEngine::eligible(cfg.wledEnabled[i], cfg.wledEvents[i], event)' in s
assert 'const bool followed = rearFollows(g.id);' in s
assert 'String wledRestoreBody(const String &original)' in s
assert "AsyncUDP" not in s and "ddpWrite" not in s
for x in ('openWledSegments(i,zones)','async function openWledSegments(slot,host)',
          '/api/wled/segments?slot=','/api/wled/segment-config',
          '/api/wled/segment-test','Save zone assignments',
          'Enable segment-based Home / Away / Neutral',
          'Home'):
    assert x in ui,x
assert 'if(currentConfig.wled[i].ip!==$("ip"+i).value.trim())' in ui
assert 'identify.disabled=!data.controllerOn' in ui
print("PASS WLED 8B: mapped native segment IDs, scoped restore, safe preview and unchanged NHL admission")
