#!/usr/bin/env python3
"""Phase 7B.4: regression for live goal WAV observability and opt-in isolation."""
from pathlib import Path
s=Path("src/main.cpp").read_text(encoding="utf-8")
ui=Path("include/WebUi.h").read_text(encoding="utf-8")
log=Path("include/GameEventLog.h").read_text(encoding="utf-8")
audio=Path("include/S3GoalAudio.h").read_text(encoding="utf-8")
detect=s.split("void detectRearGoalChanges() {",1)[1].split(
  "// Track PP changes",1)[0]
assert 'const bool armedForGoal = isArmedGame(g.id)' in detect
assert 'if (armedForGoal) celebrate(teamRgb(scorer),entry.sequence,' in detect
assert 'else entry.audio=GameEventLog::Audio::NOT_ARMED;' in detect
assert 'if (followed || cfg.displayAllLiveEvents)' in detect
horn=s.split('void startHorn(uint32_t eventSequence=0) {',1)[1].split(
  'void tickHorn() {',1)[0]
for key in (
    'gameEventHistory.find(eventSequence)',
    'if (!cfg.horn) {',
    'GameEventLog::Audio::HORN_DISABLED',
    's3Audio.selectedSource()',
    'if(!s3Audio.ready())',
    'if(!s3Audio.play())',
    's3Audio.busyOrCoolingDown()',
    'GameEventLog::Audio::BUSY',
    'GameEventLog::Audio::UNAVAILABLE',
    'GameEventLog::Audio::STARTED',
    'strcmp(s3Audio.clipName(),"custom-wav")',
    'hornLogSequence=eventSequence',
    'hornPlaying = true;',
):
    assert key in horn,key
for key in (
    'if(hornLogSequence && !s3Audio.playing())',
    's3Audio.lastStreamOk()?',
    'GameEventLog::Audio::COMPLETED:GameEventLog::Audio::STREAM_FAILED',
    'hornLogSequence=0;'
):
    assert key in s,key
assert 'startHorn(eventSequence);' in s
assert '"Playback started"' in log
assert '"Stream failed"' in log
assert '"Playback busy or in cooldown"' in log
assert 'Audio audio = Audio::NA;' in log
assert 'uint8_t audioSource = 0;' in log
assert 'bool audioFallback = false;' in log
for key in (
    'doc["hornEnabled"] = cfg.horn',
    'row["audio"] = GameEventLog::audioLabel(e->audio)',
    'row["audioSource"] = e->audioSource',
    'row["audioFallback"] = e->audioFallback'
):
    assert key in s,key
for key in (
    'row.type==="Goal"',
    'goalAudio?"Goal horn"',
    'row.audioFallback',
    'Live NHL goal horn: ',
    's.audioSelectedSource',
    's.hornEnabled'
):
    assert key in ui,key
assert 'busyOrCoolingDown()const' in audio
assert 'S3HornPolicy::kRetriggerMs' in audio
preview=s.split('server.on("/api/audio/preview"',1)[1].split(
    'server.on(',1)[0]
for forbidden in ('addGameEvent(', 'addPreviewEvent(', 'startWledEvent(',
                  'startHorn(', 'celebrate(', 'cfg.horn='):
    assert forbidden not in preview,forbidden
assert 'CelebrationEngine::eligible(cfg.wledEnabled[i], cfg.wledEvents[i], event)' in s
print("PASS: goal-horn consent, real-game admission, WAV accepted/failed and event-log UX")
