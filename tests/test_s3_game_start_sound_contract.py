from pathlib import Path
s=Path("src/main.cpp").read_text()
ui=Path("include/WebUi.h").read_text()
assert 'uint8_t s3StartSound = 3;' in s
assert 'prefs.getUChar("s3StartSound",3)' in s
assert 'server.on("/api/audio/gamestart-select", HTTP_POST' in s
assert 'if(started)startGameStartSound(log.sequence);' in s
assert 'if(secondaryArmed){' in s
assert 'if(isArmedGame(g->id)){' in s
assert 'if(transition==CelebrationEngine::Event::GAME_START)' in s
assert 'startGameStartSound(logged.sequence);' in s
assert 'startGameStartSound(previewRow.sequence);' in s
assert 's3Audio.playSource(source)' in s
assert 'if(cfg.s3StartSound==slot)' in s
assert 'id="s3StartSoundStatus"' in ui
assert '"/api/audio/gamestart-select"' in ui
assert '"Use for game start"' in ui
assert 'startAudio?"Game-start audio"' in ui
print("PASS game start WAV contract")
assert 'id="rearFullStart"' in ui
assert '$("rearFullStart").onclick=()=>fullPreview("start","home");' in ui
assert 'startGameStartSound(previewRow.sequence);' in s
assert 'audioStarted = previewRow.audio == GameEventLog::Audio::STARTED;' in s
