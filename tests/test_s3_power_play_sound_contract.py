#!/usr/bin/env python3
"""Regression for a separate, opt-in S3 power-play sound assignment."""
from pathlib import Path
s=Path("src/main.cpp").read_text(encoding="utf-8")
ui=Path("include/WebUi.h").read_text(encoding="utf-8")
audio=Path("include/S3GoalAudio.h").read_text(encoding="utf-8")
pp=s.split("void observePowerPlayEvents() {",1)[1].split("Game *gameById",1)[0]
assert "const bool followed=rearFollows(g.id);" in pp
assert "session->takePending()" in pp
assert "if(change.started){" in pp
start=pp.split("if(change.started){",1)[1]
end=pp.split("if(change.ended){",1)[1].split("if(change.started){",1)[0]
assert "if(followed)startPowerPlaySound(entry.sequence);" in start
assert "else entry.audio=GameEventLog::Audio::NOT_ARMED;" in start
assert "startPowerPlaySound(" not in end
assert "const uint8_t source=cfg.s3PpSound;" in s
announce=s.split("void startPowerPlaySound(uint32_t eventSequence) {",1)[1].split("void tickHorn() {",1)[0]
for required in (
    "#ifdef GLOWDECK_WAVESHARE_S3",
    "if(!source)",
    "if(!s3Audio.ready() || !soundStorage.available(source))",
    "if(!s3Audio.playSource(source))",
    "s3Audio.busyOrCoolingDown()",
    "entry->audio=GameEventLog::Audio::STARTED",
    "hornLogSequence=eventSequence",
):
    assert required in announce,required
assert "s3Audio.playSource(source)" in announce
assert "cfg.horn" not in announce, "PP choice must not depend on goal horn enable"
for required in (
    "uint8_t s3PpSound = 0;",
    'prefs.getUChar("s3PpSound",0)',
    'prefs.putUChar("s3PpSound",cfg.s3PpSound)',
    'prefs.putUChar("s3PpSound",0)',
    'doc["powerPlaySelected"]=cfg.s3PpSound',
    'doc["audioPowerPlaySource"] = cfg.s3PpSound',
    'doc["s3PpSound"] = cfg.s3PpSound',
    'server.on("/api/audio/powerplay-select", HTTP_POST, []() {',
    'if(source && !soundStorage.inspect(uint8_t(source),m))',
    'if(cfg.s3PpSound==slot)',
    'startPowerPlaySound(previewRow.sequence)',
    "audioStarted = previewRow.audio == GameEventLog::Audio::STARTED;",
):
    assert required in s,required
route=s.split('server.on("/api/audio/powerplay-select", HTTP_POST, []() {',1)[1].split('server.on(',1)[0]
assert "if(!authorized())return;" in route
assert "s3Audio.playing() || s3Audio.storageBusy()" in route
for forbidden in ("startHorn(", "celebrate(", "addGameEvent(", "startWledEvent("):
    assert forbidden not in route,forbidden
for required in (
    'id="s3PpSoundStatus"',
    'id="s3PpSoundOff"',
    'id="rearFullPpHome"',
    'id="rearFullPpAway"',
    '"Use for power plays"',
    '"/api/audio/powerplay-select"',
    '$("rearFullPpHome").onclick=()=>fullPreview("pp","home")',
    '$("rearFullPpAway").onclick=()=>fullPreview("pp","away")',
    'ppAudio?"Power-play audio"',
):
    assert required in ui,required
assert "playSource(uint8_t source)" in audio
assert "if(!s3Audio.play())" in s, "Original goal path preserved"
print("PASS S3 PP audio: separate WAV, followed-only, no PP-end audio, authorized UI, original goal horn intact")
