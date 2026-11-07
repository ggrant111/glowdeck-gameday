#!/usr/bin/env python3
"""7A source contracts: fail-safe ES8311 output and S3-only web preview."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
main=(root/"src/main.cpp").read_text()
audio=(root/"include/S3GoalAudio.h").read_text()
policy=(root/"include/S3HornPolicy.h").read_text()
html=(root/"include/WebUi.h").read_text()
pins=(root/"include/GameDayS3Hardware.h").read_text()
conf=(root/"platformio.ini").read_text()
assert "#ifdef GLOWDECK_WAVESHARE_S3\n#include \"GameDayS3Hardware.h\"\n#include \"S3GoalAudio.h\"" in main
assert "S3GoalAudio s3Audio;" in main
assert "s3Audio.begin();" in main and "s3Audio.tick();" in main
assert "if (!cfg.horn) {" in main
assert "entry->audio=GameEventLog::Audio::HORN_DISABLED" in main
assert "if(!s3Audio.play())" in main
assert "entry->audio=GameEventLog::Audio::STARTED" in main
assert "ledcWriteTone(AUDIO_CH, nextFreq);" in main
assert "ledcAttachPin(PIN_AUDIO, AUDIO_CH);" in main
assert "#ifndef GLOWDECK_WAVESHARE_S3" in main
assert 'server.on("/api/audio/preview", HTTP_POST, []() {' in main
route=main.split('server.on("/api/audio/preview"',1)[1].split('server.on(',1)[0]
assert 'if (!authorized()) return;' in route
assert "s3Audio.ready()" in route and "s3Audio.play()" in route
assert "errorResponse(s3Audio.error(),503)" in route
assert "doc[\"volume\"]=s3Audio.volume();" in route
for forbidden in ("celebrate(", "startWled", "queueGameOverlay(", "addPreviewEvent(",
                  "setGameFollow(", "saveSettings(", "prefs.put"):
    assert forbidden not in route, forbidden
for field in ('doc["audioSupported"] = true;', 'doc["audioReady"]',
              'doc["audioPlaying"]','doc["audioVolume"]','doc["audioError"]'):
    assert field in main,field
assert 'id="s3AudioCard"' in html and 'id="s3AudioPreview"' in html
assert 'id="s3AudioStatus"' in html and 'id="hornLabel"' in html
assert 'await post("/api/audio/preview",{})' in html
assert "s.audioSupported" in html and "s.audioPlaying" in html
assert "GLOWDECK_WAVESHARE_S3" not in conf.split("[env:s3_waveshare_35]")[0]
assert "#ifdef GLOWDECK_WAVESHARE_S3" in audio
assert 'GameDayS3::readExpander(0x03,dir)' in audio
assert 'GameDayS3::readExpander(0x01,out)' in audio
assert "out & ~0x80U" in audio
assert "dir & ~0x80U" in audio
assert "out|0x80U" in audio
assert "uint8_t next=enabled?" in audio
assert "kCodecAddress=0x18" in audio
assert "pins.mck_io_num=12;" in audio
assert "pins.bck_io_num=13;" in audio
assert "pins.ws_io_num=15;" in audio
assert "pins.data_out_num=16;" in audio
assert 'Wire.beginTransmission(kCodecAddress);' in audio
assert "kSampleRate*256" not in audio
assert "I2S_MCLK_MULTIPLE_256" in audio
assert "i2s_driver_install(I2S_NUM_0" in audio
assert "i2s_write(I2S_NUM_0" in audio
assert "xTaskCreatePinnedToCore" in audio
assert "ulTaskNotifyTake(pdTRUE,portMAX_DELAY)" in audio
assert "vTaskDelay(pdMS_TO_TICKS(85))" in audio  # Drain extended 64ms DMA queue
assert "i2s_zero_dma_buffer(I2S_NUM_0)" in audio
assert "kVolumePercent=70" in policy
assert "kMaxVolumePercent=80" in policy
assert "{0x32,S3HornPolicy::codecVolumeRegister(S3HornPolicy::kVolumePercent)}" in audio
assert "S3HornPolicy::canPlay(" in audio
assert "bool streamError_" not in audio
assert "ES8311" in audio
assert "SDA_PIN = 8" in pins and "SCL_PIN = 7" in pins
assert "PIN_AUDIO = 26" in main
assert "PIN_AUDIO = 26" not in audio and "PIN_AUDIO = 26" not in policy
for forbidden in ("server.", "HTTPClient", "String(", "malloc(", "new "):
    assert forbidden not in policy, forbidden
print("PASS S3 Phase 7A: correct codec pins/PA, safe async preview, CYD isolation")
