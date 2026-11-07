#!/usr/bin/env python3
"""Phase 8D guardrails: exclusive WLED realtime ownership, opt-in
migration, bounded WiFiUDP, no new NHL events and safe HTTP restoration."""
from pathlib import Path
s=Path("src/main.cpp").read_text()
ui=Path("include/WebUi.h").read_text()
policy=Path("include/WledDdpPolicy.h").read_text()
for required in (
  "kPort=4048","kMaxPixels=4096","kChunkPixels=480",
  "kMaxPackets=","kMaxConcurrent=2",
  "kMinFps=10,kMaxFps=20","kPreviewMs=3000UL",
  "kMaxLiveMs=30000UL","kHeaderBytes=10",
  "out[0]=(firstPixel+count==layout.pixels)?0x41:0x40",
  "out[1]=uint8_t(sequence%15U+1U)",
  "out[2]=0x0B","out[3]=0x01",
  "out[8]=uint8_t(bytes>>8)","out[9]=uint8_t(bytes)",
  "uint32_t(firstPixel)*3U","used=kHeaderBytes+bytes",
  "GAP_OR_OVERLAP","NOT_TARGETED","UNASSIGNED",
  "WledZonePolicy::target(true,",
  "r.start<prior.stop && prior.start<r.stop",
  "covered==layout.pixels?OK:GAP_OR_OVERLAP",
):
    assert required in policy,required
for required in (
  '#include <WiFiUdp.h>',
  '#include "WledDdpPolicy.h"',
  "bool wledDdpEnabled[MAX_WLED]={}",
  "uint8_t wledDdpFps[MAX_WLED]={}",
  'prefs.getBool(("ddpOn"+String(i)).c_str(),false)',
  'prefs.getUChar(("ddpFps"+String(i)).c_str(),',
  'd["ddpEnabled"] = cfg.wledDdpEnabled[i]',
  'd["ddpFps"] = cfg.wledDdpFps[i]',
  "struct WledDdpSession {","WiFiUDP wledDdpUdp;",
  "uint8_t wledDdpPacket[WledDdpPolicy::kPacketBytes]",
  "bool prepareWledDdp(","bool sendWledDdpFrame(",
  "bool releaseWledDdp(","void tickWledDdpStreams()",
  "if(info.live||info.rgbw||info.ledCount>WledDdpPolicy::kMaxPixels)",
  "!cfg.wledDdpEnabled[slot]",
  "event==CelebrationEngine::Event::POWER_PLAY",
  "WledDdpPolicy::validate(layout,uint8_t(event),scorerSide)",
  'wledDdp[slot].status="Native JSON (different zone durations)"',
  "wledDdpUdp.beginPacket(target,WledDdpPolicy::kPort)",
  "WledDdpPolicy::frameChunk(session.layout,rendered,elapsed,",
  "first=uint16_t(first+WledDdpPolicy::kChunkPixels)",
  "wledDdpUdp.write(wledDdpPacket,bytes)==bytes",
  "wledDdpUdp.endPacket()==1",
  'result["chunksPerFrame"]=WledDdpPolicy::packetCount(',
  'result["packets"]=wledDdp[slot].packets',
  'postWled(ip,"{',
  "const bool ddpReleased=releaseWledDdp(i)",
  "wledDdp[i].active && !releaseWledDdp(i)",
  "if(cfg.wledDdpEnabled[i] && cfg.wledZoneEnabled[i])",
  "if(prepareWledDdp(i,event,color,scorerSide,savedState[i],candidate))",
  "if(sendWledDdpFrame(i,millis()))",
  "tickWledDdpStreams();",
  "if(!restoring[i] || wledDdp[i].active || !wledRestoreMapped[i]",
  "wledDdp[i].status=\"Disabled: controller IP changed\"",
):
    assert required in s,required

# During native JSON phase updates, a controller owned by an active DDP
# session must always be skipped. Check the relevant branch semantically,
# rather than requiring one particular whitespace style in the condition.
import re
phase=s.split("void tickWledCelebrations() {",1)[1].split(
    "// Compact score/state fingerprint:",1)[0]
assert re.search(
    r"if\s*\(\s*!restoring\[i\]\s*\|\|\s*wledDdp\[i\]\.active\s*\|\|",
    phase), "DDP realtime controllers must not receive native JSON phase writes"
assert 'wledEffectBody(savedState[i],wledActiveEvent,' in phase
for route,method in (
  ("/api/wled/ddp","GET"),
  ("/api/wled/ddp/config","POST"),
  ("/api/wled/ddp/test","POST"),
  ("/api/wled/ddp/stop","POST"),
):
    anchor=f'server.on("{route}", HTTP_{method}, []() {{'
    assert anchor in s,route
    body=s.split(anchor,1)[1].split('server.on(',1)[0]
    assert "if(!authorized())return;" in body,route
    for forbidden in ("queueGameOverlay(", "startHorn(", "s3Audio.play(",
                      "startWledEvent(", "celebrate(", "setGameFollow("):
        assert forbidden not in body,(route,forbidden)
config=s.split('server.on("/api/wled/ddp/config"',1)[1].split('server.on(',1)[0]
assert 'request["exclusiveConfirmed"]|false' in config
assert 'WledDdpPolicy::validFps(rate)' in config
assert 'cfg.wledZoneEnabled[slot]' in config
assert 'prefs.putBool(("ddpOn"+String(slot)).c_str(),enabled)' in config
assert 'prefs.putUChar(("ddpFps"+String(slot)).c_str(),uint8_t(rate))' in config
preview=s.split('server.on("/api/wled/ddp/test"',1)[1].split('server.on(',1)[0]
assert 'const String snapshot=getWledState(cfg.wled[slot]);' in preview
assert 'if(wledActiveEvent!=CelebrationEngine::Event::NONE || restoring[slot]' in preview
assert 'wledRestoreMapped[slot]=true' in preview
assert 'wledEventOwner[slot]=0' in preview
assert 'WledDdpPolicy::kPreviewMs' in preview
assert 'restoreAt[slot]=millis()' in preview
stop=s.split('server.on("/api/wled/ddp/stop"',1)[1].split('server.on(',1)[0]
assert "scheduleWledRestoreAll();" in stop
assert 'const bool armedForGoal = isArmedGame(g.id)' in s
assert 'if (armedForGoal) celebrate(teamRgb(scorer)' in s
assert 'CelebrationEngine::eligible(cfg.wledEnabled[i], cfg.wledEvents[i], event)' in s
assert 'const bool followed = rearFollows(g.id);' in s
assert 'wledEffectBody(savedState[i],event,color,0,' in s
for ui_key in (
  'id="wledDdpTestCard"','id="wledDdpLabSlot"',
  'id="wledDdpLabRun"','id="wledDdpLabStop"',
  'id="wledDdpLabStatus"',
  'async function openWledDdpSettings(slot,host)',
  "function renderDdpTestOptions()",
  "async function refreshDdpTestStatus()",
  '"/api/wled/ddp/config"',
  '"/api/wled/ddp/test"',
  '"/api/wled/ddp/stop"',
  'exclusiveConfirmed:true',
  'if(!confirm.checked)',
  '"/api/wled/ddp/setup?slot="',
  '"/api/wled/ddp/quick-setup"',
):
    assert ui_key in ui,ui_key
print("PASS Phase 8D: protocol, exclusive coverage, safe defaults, endpoint auth, "+
      "bounded transport and unchanged NHL arm/follow policy")
