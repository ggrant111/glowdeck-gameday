#!/usr/bin/env python3
"""Phase 7B source contracts: four-slot LittleFS WAV, endpoint auth,
NHL/WLED isolation, file safety, preferences and separate CYD output."""
from pathlib import Path
import re
root=Path(__file__).resolve().parents[1]
m=(root/"src/main.cpp").read_text()
a=(root/"include/S3GoalAudio.h").read_text()
storage=(root/"include/S3SoundStorage.h").read_text()
policy=(root/"include/S3WavPolicy.h").read_text()
ui=(root/"include/WebUi.h").read_text()
cfg=(root/"platformio.ini").read_text()
for key in ("kSlots=4","kMaxBytes=768UL*1024UL","kMaxDurationMs=10000UL",
            "kMaxHeaderScan=4096UL","kOutputRate=48000UL",
            'fourcc(riff,"RIFF")', 'fourcc(riff+8,"WAVE")',
            'fourcc(chunk,"fmt ")','fourcc(chunk,"data")',
            "info.bits!=16", "info.sampleRate<8000",
            "info.sampleRate>kOutputRate","(info.dataBytes%blockAlign)",
            "template<typename Reader>", "class Stream",
            "int64_t(next_)-int64_t(current_)","if(!nextInput(next_))return false"):
    assert key in policy,key
for key in ('"/sound1.wav"','"/sound2.wav"','"/sound3.wav"',
            '"/sound.tmp"','"/sound.bak"','"/sound.slot"',
            "LittleFS.rename(target,kBackup)",
            "LittleFS.rename(kTemp,target)",
            "LittleFS.rename(kBackup,target)",
            "LittleFS.remove(kBackup)",
            "LittleFS.remove(kTemp)",
            "S3WavPolicy::parse(f,info)",
            "S3WavPolicy::validSlot(slot)"):
    assert key in storage,key
assert '"/goal.gif"' not in storage
for key in ("S3WavPolicy::Stream<File> decoder",
            "decoder.begin(wav,meta)", "decoder.next(mono)",
            "S3SoundStorage::path(slot)",
            "playSource(uint8_t source)",
            "setStorageMounted(bool mounted)",
            "setStorageBusy(bool busy)",
            "storageBusy_.load()",
            "selectedSource_", "setSelectedSource(uint8_t source)",
            "activeSource_.load()", "wav.close()",
            "uint8_t chosen=selectedSource_",
            "return playClip(Clip::HORN)"):
    assert key in a,key
for key in ("soundStorage.begin(goalGif.mounted());",
            "s3Audio.setStorageMounted(soundStorage.mounted());",
            "s3Audio.setSelectedSource(cfg.s3Sound);",
            'prefs.getUChar("s3Sound",0)',
            'prefs.putUChar("s3Sound",',
            'prefs.putString(("s3Name"+String(soundUploadSlot)).c_str(),soundUploadTitle)',
            "cfg.s3Sound=0;","if(!s3Audio.play())",
            'doc["audioSelectedSource"]',
            'doc["s3Sound"] = s3Audio.selectedSource()'):
    assert key in m,key
routes={
    "/api/audio/library":"HTTP_GET",
    "/api/audio/select":"HTTP_POST",
    "/api/audio/delete":"HTTP_POST",
    "/api/audio/upload":"HTTP_POST",
    "/api/audio/preview":"HTTP_POST",
}
for path,verb in routes.items():
    start=f'server.on("{path}", {verb}, []() {{'
    assert start in m,path
    route=m.split(start,1)[1].split('server.on(',1)[0]
    assert re.search(r"if\s*\(\s*!authorized\(\)\s*\)",route),path
    for forbidden in ("startWledEvent", "celebrate(", "queueGameOverlay(",
                      "addPreviewEvent(", "toggleGameFollow(", "drawS3ArenaScoreboard("):
        assert forbidden not in route,(path,forbidden)
upload=m.split('server.on("/api/audio/upload"',1)[1].split('server.on("/api/audio/diagnostics"',1)[0]
for key in ("UPLOAD_FILE_START","UPLOAD_FILE_WRITE","UPLOAD_FILE_END",
            "UPLOAD_FILE_ABORTED","hasDashboardAccess()",
            "soundUploadBytes+=", "S3WavPolicy::kMaxBytes-soundUploadBytes",
            "soundStorage.install(soundUploadSlot)","soundUploadTitle.trim()",
            "soundUploadFailed","soundUploadFinished",
            "soundUpload.close()","soundStorage.mounted()",
            "s3Audio.playing()","s3Audio.setStorageBusy(true)",
            "s3Audio.setStorageBusy(false)"):
    assert key in upload,key
assert 's3SoundUploadForm' in ui
for key in ('id="s3SoundLibrarySlots"','id="s3SoundUploadForm"',
            'id="s3SoundUploadSlot"','id="s3SoundUploadFile"',
            'id="s3SoundBuiltInUse"','id="s3SoundBuiltInPreview"',
            '/api/audio/upload?slot=', '/api/audio/library',
            '/api/audio/select','/api/audio/delete',
            '/api/audio/preview', "lib.clips", 'textContent=',
            "file.size>786432","await loadSoundLibrary()"):
    assert key in ui,key
assert "innerHTML=" not in ui.split("async function loadSoundLibrary()",1)[1].split('$("s3AudioPreview").onclick',1)[0]
assert "GLOWDECK_WAVESHARE_S3" not in cfg.split("[env:s3_waveshare_35]")[0]
assert '#ifdef GLOWDECK_WAVESHARE_S3' in storage
assert '#ifdef GLOWDECK_WAVESHARE_S3' in a
assert 'ledcWriteTone(AUDIO_CH, nextFreq);' in m
assert 'ledcAttachPin(PIN_AUDIO, AUDIO_CH);' in m
print("PASS Phase 7B: safe WAV upload, persistent library, selection, preview, fallback and CYD isolation")
