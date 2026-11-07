#!/usr/bin/env python3
"""Regression: live WAV reliability and identical Activity/serial event audit."""
from pathlib import Path
main = Path("src/main.cpp").read_text(encoding="utf-8")
driver = Path("include/S3GoalAudio.h").read_text(encoding="utf-8")
wav = Path("include/S3WavPolicy.h").read_text(encoding="utf-8")
log = Path("include/GameEventLog.h").read_text(encoding="utf-8")
ui = Path("include/WebUi.h").read_text(encoding="utf-8")

for term in (
    "config.dma_buf_count=12;",
    "pdMS_TO_TICKS(220)",
    "static bool writeAudioBlock(",
    "while(offset<total)",
    "written>total-offset",
    "stalled>=3",
    "vTaskDelay(pdMS_TO_TICKS(15))",
    "vTaskDelay(pdMS_TO_TICKS(85))",
    "if(custom && self->allowFallback_.load()",
    "failure!=Failure::I2S_WRITE",
    "lastFailure_.store(uint8_t(failure))",
    "lastStreamOk_.store(complete)",
    "readFailureOffset_.store(readOffset)",
    "peakSample_.store(peak)",
    "ampVerifiedAtStart_.store(true)",
    "failureName(lastFailure())",
):
    assert term in driver, term
assert '"I2S write timeout"' not in driver, "Old error falsely labeled WAV reads as I2S"
for term in (
    "while(filled<count)",
    "if(++zeroReads>=3)",
    "readFailed_=true;",
    "lastReadOffset_=meta_.dataOffset+consumedBytes_+filled;",
    "bool readFailed()const",
):
    assert term in wav, term
for term in (
    "AudioFailure audioFailure",
    "audioFrames = 0, audioExpectedFrames = 0;",
    "audioReadOffset = 0, audioRetries = 0;",
    "audioAmpVerified = false;",
    "serialFingerprint = 0;",
    "audioFailureLabel(AudioFailure failure)",
):
    assert term in log, term
for term in (
    "entry->audioFrames=s3Audio.writtenFrames()",
    "entry->audioExpectedFrames=s3Audio.expectedFrames()",
    "entry->audioRetries=s3Audio.writeRetries()",
    "entry->audioFailure=static_cast<GameEventLog::AudioFailure>",
    "row[\"audioFrames\"] = e->audioFrames",
    "row[\"audioFailure\"] = GameEventLog::audioFailureLabel(e->audioFailure)",
    "row[\"speakerVerified\"] = false",
    "void tickEventAudit(){",
    "tickEventAudit();",
    "GameEventLog::audioLabel(e->audio)",
    "GameEventLog::wledLabel(e->wled[i])",
    "e->serialFingerprint=fingerprint",
    "speakerVerified=NO",
):
    assert term in main, term
activity = ui.split("function createEventElement(row){", 1)[1].split(
    "function renderEventRows(", 1)[0]
for term in (
    'const ppAudio=row.type==="Power play started"',
    "Playback diagnostics: ",
    "row.audioExpectedFrames",
    "row.audioFailure",
    "row.audioI2sCode",
    "row.audioAmpVerified",
    "row.audioReadOffset",
    "speaker audibility not verified",
    '" · Event #"+row.id',
):
    assert term in activity, term
assert 'if(row.type==="Goal" || row.type==="Goal preview")' not in activity
print("PASS live S3 streaming recovery, per-event audio audit, PP visibility and serial parity")
