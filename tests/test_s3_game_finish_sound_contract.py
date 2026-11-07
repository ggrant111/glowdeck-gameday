from pathlib import Path
s=Path("src/main.cpp").read_text()
ui=Path("include/WebUi.h").read_text()
policy=Path("include/S3WavPolicy.h").read_text()
storage=Path("include/S3SoundStorage.h").read_text()
assert "kSlots=4" in policy
assert 'case 4:return "/sound4.wav";' in storage
# Verify that the actual HTTP upload parser accepts Slot 4 (not just the UI/policy).
assert 'slotArg=="4"?4:0' in s
assert 'Select sound slot 1 through 4' in s
assert 'Select sound slot 1 through 3' not in s
assert 'uint8_t s3FinishSound = 4;' in s
assert 'prefs.getUChar("s3FinishSound",4)' in s
assert 'server.on("/api/audio/gamefinish-select", HTTP_POST' in s
assert 'startGameFinishSound(log.sequence);' in s
assert 'startGameFinishSound(logged.sequence);' in s
assert 'startGameFinishSound(previewRow.sequence);' in s
assert 's3Audio.playSource(source)' in s
assert 'if(cfg.s3FinishSound==slot)' in s
assert 'id="rearFullFinishWin"' in ui
assert 'id="rearFullFinishLoss"' in ui
assert 'fullPreview("win","home")' in ui
assert 'fullPreview("loss","away")' in ui
assert 'id="s3FinishSoundStatus"' in ui
assert '"/api/audio/gamefinish-select"' in ui
assert '"Use for game finish"' in ui
assert 'Game-finish audio: ' in ui
print("PASS S3 game-finish audio contract")
