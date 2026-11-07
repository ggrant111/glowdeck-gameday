#!/usr/bin/env python3
"""S3 Phase 7A.1 diagnostic regression: avoid no-op previews and wrong pins."""
from pathlib import Path
base=Path(__file__).resolve().parents[1]
main=(base/"src/main.cpp").read_text()
audio=(base/"include/S3GoalAudio.h").read_text()
policy=(base/"include/S3HornPolicy.h").read_text()
ui=(base/"include/WebUi.h").read_text()
for field in ("audioLastFrames","audioLastStreamOk","audioLastDurationMs",
              "audioAmpEnabled","audioLastClip","audioReady","audioError"):
    assert 'doc["'+field+'"]' in main,field
for endpoint in ('server.on("/api/audio/preview", HTTP_POST, []() {',
                 'server.on("/api/audio/test-tone", HTTP_POST, []() {',
                 'server.on("/api/audio/diagnostics", HTTP_GET, []() {'):
    assert endpoint in main,endpoint
for name in ("/api/audio/preview", "/api/audio/test-tone", "/api/audio/diagnostics"):
    route=main.split('server.on("'+name+'"',1)[1].split('server.on(',1)[0]
    assert "if (!authorized()) return;" in route,name
    for forbidden in ("celebrate(", "startWled", "queueGameOverlay(",
                      "addPreviewEvent(", "saveSettings(", "prefs.put"):
        assert forbidden not in route, (name,forbidden)
assert "s3Audio.testTone()" in main
assert "s3Audio.probe()" in main
for required in ("readCodec(0x00,p.codecReset)","readCodec(0x31,p.codecMute)",
                 "readCodec(0x32,p.codecVolume)",
                 "GameDayS3::readExpander(0x03,p.ampDirection)",
                 "GameDayS3::readExpander(0x01,p.ampOutput)",
                 "bool(verify&0x80U)!=enabled", "lastStreamOk_.store",
                 "writtenFrames_.store(cursor)", "lastDurationMs_.store",
                 "i2s_write(I2S_NUM_0", "S3HornPolicy::diagnosticSample"):
    assert required in audio,required
assert "{0x32,S3HornPolicy::codecVolumeRegister(S3HornPolicy::kVolumePercent)}" in audio
assert "{0x17,0xC8},{0x14,0x1A}" in audio
assert "kDiagnosticHz=1000" in policy
assert "kTestDurationMs=1200" in policy
assert "kVolumePercent=70" in policy
assert "kMaxVolumePercent=80" in policy
for elem in ('id="s3AudioTone"','id="s3AudioProbe"',
             'id="s3AudioDiagnostics"','readS3AudioProbe()',
             'post("/api/audio/test-tone",{})'):
    assert elem in ui,elem
assert "TCA9554" in audio and "ES8311" in audio
assert "#ifdef GLOWDECK_WAVESHARE_S3" in audio
assert "S3GoalAudio s3Audio;" in main
assert "s3Audio.begin();" in main and "s3Audio.tick();" in main
assert "if (!cfg.horn) {" in main
assert "GameEventLog::Audio::HORN_DISABLED" in main
assert "ledcWriteTone(AUDIO_CH, nextFreq);" in main
assert "ledcAttachPin(PIN_AUDIO, AUDIO_CH);" in main
print("PASS S3 7A.1: tone & readback diagnostics, no NHL/WLED side effects, CYD parity")
