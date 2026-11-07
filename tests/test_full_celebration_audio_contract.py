#!/usr/bin/env python3
"""Contract: combined goal preview exercises live horn gating and reports audio."""
from pathlib import Path
s=Path("src/main.cpp").read_text(encoding="utf-8")
ui=Path("include/WebUi.h").read_text(encoding="utf-8")
preview=s.split('server.on("/api/wled/preview", HTTP_POST, []() {',1)[1].split(
    'server.on("/api/test", HTTP_POST, []() {',1)[0]
assert 'if (combined && event == CelebrationEngine::Event::GOAL)' in preview
assert 'startHorn(previewRow.sequence);' in preview
assert 'previewRow.audio == GameEventLog::Audio::STARTED' in preview
assert preview.count('startHorn(')==1
for value in (
    'response["audioStarted"] = audioStarted;',
    'response["audioStatus"] = GameEventLog::audioLabel(previewRow.audio);',
    'response["audioSource"] = previewRow.audioSource;',
    'response["audioFallback"] = previewRow.audioFallback;',
    'wledStarted || rearStarted || overlayStarted || audioStarted',
):
    assert value in preview, value
assert 'const goalAudio=row.type==="Goal"||row.type==="Goal preview";' in ui
assert 'const ppAudio=row.type==="Power play started"||row.type==="Power-play preview";' in ui
assert 'row.audioExpectedFrames>0' in ui, "Live and test events share audio metrics"
assert 'result.audioStatus||"not started"' in ui
assert '"playback started (speaker not verified)"' in ui
assert 'post("/api/wled/preview",{event,side,team,combined:true})' in ui
print("PASS: full goal preview sound trigger, diagnostics, log and WLED isolation")
