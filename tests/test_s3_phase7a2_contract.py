#!/usr/bin/env python3
"""Phase 7A.2: amplifier gain updates are bounded, stored & S3-only."""
from pathlib import Path
base=Path(__file__).resolve().parents[1]
m=(base/"src/main.cpp").read_text()
a=(base/"include/S3GoalAudio.h").read_text()
p=(base/"include/S3HornPolicy.h").read_text()
ui=(base/"include/WebUi.h").read_text()
assert "kVolumePercent=70" in p and "kMinVolumePercent=30" in p and "kMaxVolumePercent=80" in p
assert "codecVolumeRegister(uint8_t percent)" in p
assert "stepHz(220)" in p and "stepHz(440)" in p
assert "stepHz(660)" in p and "stepHz(880)" in p
assert "30000LL" in p
assert "uint8_t volume()const{return volumePercent_;}" in a
assert "bool setVolume(uint8_t percent)" in a
assert "playing_.load()" in a
assert "S3HornPolicy::validVolume(percent)" in a
assert "readCodec(0x32,readback)" in a and "readback!=next" in a
assert "writeCodec(0x32,old)" in a
assert "volumePercent_=percent;" in a
assert "error_=Error::CODEC_VOLUME;" in a
assert "uint8_t volumePercent_=S3HornPolicy::kVolumePercent;" in a
assert "{0x32,S3HornPolicy::codecVolumeRegister(S3HornPolicy::kVolumePercent)}" in a
assert 'server.on("/api/audio/volume", HTTP_POST, []() {' in m
route=m.split('server.on("/api/audio/volume"',1)[1].split('server.on(',1)[0]
assert "if (!authorized()) return;" in route
assert "request.containsKey(\"percent\")" in route
assert "S3HornPolicy::kMinVolumePercent" in route and "S3HornPolicy::kMaxVolumePercent" in route
assert "s3Audio.setVolume(uint8_t(percent))" in route
assert 'prefs.putUChar("s3Vol",cfg.s3Volume);' in route
for x in ("startWled", "celebrate(", "queueGameOverlay(", "setGameFollow(", "saveSettings("):
  assert x not in route,x
assert 'cfg.s3Volume=constrain((int)prefs.getUChar("s3Vol",' in m
assert "if (!s3Audio.setVolume(cfg.s3Volume))" in m
assert 'doc["s3Volume"] = s3Audio.volume();' in m
assert 'doc["volumePercent"]=s3Audio.volume();' in m
for x in ('id="s3AudioVolume"', 'id="s3AudioVolumeValue"',
          'id="s3AudioVolumeApply"', 'post("/api/audio/volume",{percent:requested})',
          'currentConfig.s3Volume=response.volume', 's.audioVolume'):
  assert x in ui,x
assert "Math.floor((d.volumePercent||70)*256/100)-1" in ui
assert 'doc["audioVolume"] = s3Audio.volume();' in m
assert "S3GoalAudio s3Audio;" in m
assert 'ledcAttachPin(PIN_AUDIO, AUDIO_CH);' in m
assert "PIN_AUDIO = 26" not in a
print("PASS S3 7A.2: safe gain, NVS restore, authenticated S3-only control and unchanged CYD")
