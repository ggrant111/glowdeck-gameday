#!/usr/bin/env python3
"""Phase 8C source contracts: no NHL eligibility, geometry, LED pin,
legacy WLED, or DDP changes; authenticated scene CRUD and safe previews."""
from pathlib import Path
s=Path("src/main.cpp").read_text()
ui=Path("include/WebUi.h").read_text()
policy=Path("include/WledScenePolicy.h").read_text()
zone=Path("include/WledZonePolicy.h").read_text()

for required in (
 "kSceneCount=kEvents*kZones","kInherit=255",
 "kBlobBytes=3+kSceneCount*kSceneBytes+4",
 "kAllowedFx[]={0,2,3,12,17,23,28}",
 "duration==0 ||","scene.duration<=30",
 "if(scene.fx==kInherit||scene.colorMode==kAuto)",
 "Scene staged[kSceneCount]","if(want!=stored)return false",
 "for(uint8_t i=0;i<kSceneCount;++i)scenes[i]=staged[i]",
):
 assert required in policy,required
assert "if(!mappingEnabled)return true" in zone

for required in (
 '#include "WledScenePolicy.h"',
 "WledScenePolicy::Scene wledScenes[MAX_WLED][WledScenePolicy::kSceneCount]",
 'const String key="scene"+String(i);',
 'prefs.getBytesLength(key.c_str())==WledScenePolicy::kBlobBytes',
 "WledScenePolicy::decode(raw,sizeof(raw),cfg.wledScenes[i])",
 "WledScenePolicy::encode(next,blob)",
 'prefs.putBytes(key.c_str(),blob,sizeof(blob))!=sizeof(blob)',
 "wledSceneDuration(i,wledActiveEvent,zone)",
 "wledSceneMaxDuration(i,event,changedMask)",
 "tickWledSceneExpirations(now,elapsed)",
 "wledCurrentSceneSegments[i]|=phaseTouched;",
 "wledRestoredSceneSegments[i]",
 "wledTouchedSegments[i]&=~expired",
 "wledRestoredSceneSegments[i]|=expired",
 "wledTouchedSegments[i]=0;wledRestoreMapped[i]=false;",
 'const bool alreadyRestored=wledRestoreMapped[i]',
 "if(mapped && (skipSegments&",
 "WledScenePolicy::effect(scene,effect)",
 "WledScenePolicy::rgb(scene,defaultRgb,zoneTeamRgb)",
 'if(mapped)out["bri"]=cfg.brightness',
 'out["bri"]=scene.brightness',
):
 assert required in s,required

for route,method in (
 ("/api/wled/scenes","GET"),
 ("/api/wled/scenes","POST"),
 ("/api/wled/scene-preview","POST"),
):
 anchor=f'server.on("{route}", HTTP_{method}, []() {{'
 assert anchor in s,(route,method)
 code=s.split(anchor,1)[1].split('server.on(',1)[0]
 assert 'if(!authorized())return;' in code,(route,method)
 for forbidden in ("queueGameOverlay(", "addGameEvent(",
                   "setGameFollow(", "startHorn(", "celebrate(",
                   "startWledEvent(", "s3Audio.play("):
  assert forbidden not in code,(route,forbidden)
config=s.split('server.on("/api/wled/scenes", HTTP_POST',1)[1].split('server.on(',1)[0]
for required in (
 'if(rows.size()!=WledScenePolicy::kSceneCount)',
 'if(!item["custom"].as<bool>())continue',
 'WledScenePolicy::valid(sc,uint8_t(event))',
 'idx<0||(seen&(uint16_t(1U)<<idx))',
 "if(wledActiveEvent!=CelebrationEngine::Event::NONE || restoring[slot])",
 "for(uint8_t i=0;i<WledScenePolicy::kSceneCount;++i)",
):
 assert required in config,required
preview=s.split('server.on("/api/wled/scene-preview"',1)[1].split('server.on(',1)[0]
for required in (
 'if(wledActiveEvent!=CelebrationEngine::Event::NONE || restoring[slot])',
 'cfg.wled[slot].isEmpty() || !cfg.wledZoneEnabled[slot]',
 'const String original=getWledState(cfg.wled[slot]);',
 'if(!mask || command.overflowed())',
 'WledZonePolicy::role(cfg.wledZoneMap[slot],uint8_t(id))!=role',
 "wledRestoreMapped[slot]=true",
 'wledTouchedSegments[slot]=mask',
 "restoreAt[slot]=millis()+3000UL",
 'wledEventOwner[slot]=0',
):
 assert required in preview,required
for x in (
 "async function openWledSceneBuilder(slot,host)",
 "WLED_SCENE_EVENTS","WLED_SCENE_ZONES","WLED_SCENE_EFFECTS",
 "Reset this event to inheritance","Save all scene templates",
 "Preview saved scene","Scene builder",
 "/api/wled/scenes?slot=","/api/wled/scenes",
 "/api/wled/scene-preview",
 "if(save.dataset.dirty",
):
 assert x.lower() in ui.lower(),x

# Old 8B role routing, original armed-only goal dispatch and presets unchanged.
assert 'const bool armedForGoal = g.id == selectedGameId' in s
assert 'if (armedForGoal) celebrate(teamRgb(scorer)' in s
assert 'CelebrationEngine::eligible(cfg.wledEnabled[i], cfg.wledEvents[i], event)' in s
assert 'const bool followed = rearFollows(g.id);' in s
assert 'if(!mapped)' in s and 'command["on"]=true;' in s
assert 'wledRestoreBody(savedState[i])' in s
assert "AsyncUDP" not in s and "ddpWrite" not in s
print("PASS Phase 8C: saved 15-scene builder, scoped timers/previews, "
      "legacy mode/NHL event boundaries and native WLED JSON only")
