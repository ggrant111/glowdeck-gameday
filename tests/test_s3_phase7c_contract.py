#!/usr/bin/env python3
"""Phase 7C source contracts: correct Waveshare header, OFF-by-default
safety, independent NHL/PP effects, manual test isolation and CYD parity."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
m=(root/"src/main.cpp").read_text()
p=(root/"include/S3CaseLedPolicy.h").read_text()
d=(root/"include/S3CaseLedDriver.h").read_text()
ui=(root/"include/WebUi.h").read_text()
build=(root/"platformio.ini").read_text()
doc=(root/"docs/S3_PHASE7C_CASE_LEDS.md").read_text()
svg=(root/"docs/assets/s3_phase7c_case_led_wiring.svg").read_text()
assert "#ifdef GLOWDECK_WAVESHARE_S3" in d
assert "kHomePin=17" in d and "kAwayPin=18" in d
assert "NEO_GRB+NEO_KHZ800" in d
assert "home_.begin();away_.begin();" in d
assert "if(!state)" in d and "enabled_=false;" in d
assert "pinMode(kHomePin,INPUT);pinMode(kAwayPin,INPUT);" in d
assert "if(!enabled_ ||" in d and "kFramePeriodMs" in d
assert "Adafruit_NeoPixel home_,away_;" in d
assert "GPIO17 = header pin 14" in d and "GPIO18 = header pin 16" in d
assert "NEO_GRB" not in build.split("[env:s3_waveshare_35]")[0]
assert "adafruit/Adafruit NeoPixel" in build.split("[env:s3_waveshare_35]")[1]
assert "bool caseLedsEnabled = false;" in m
assert 'prefs.getBool("caseOn",false)' in m
assert 'prefs.putBool("caseOn",cfg.caseLedsEnabled)' in m
assert 'cfg.caseLedsEnabled=enabled;' in m
assert 'caseLedDriver.enable(enabled);' in m
assert 'cfg.casePixels=S3CaseLedPolicy' not in m or "casePixels" in m
assert "s3Audio.begin();" in m and "if(!s3Audio.play())" in m
assert 'caseLedFx.trigger(side==0?S3CaseLedPolicy::Side::HOME:' in m
assert "if(cfg.caseLedsEnabled && cfg.caseGoal && followed)" in m
assert "teamRgb(scorer)" in m and "rearFollows(g.id)" in m
assert 'void tickCaseLeds() {' in m and "tickCaseLeds();" in m
assert "candidate->ppDataKnown" in m
assert "candidate->ppSide" in m
assert "candidate->intermission" in m
assert "candidate->ppUpdatedAt" in m
assert "cfg.casePp" in m
assert 'transition==CelebrationEngine::Event::GAME_START && cfg.caseStart' in m
assert 'cfg.caseFinal' in m and 'teamRgb(winnerHome?g->home:g->away)' in m
assert "S3CaseLedPolicy::Side::HOME" in m and "S3CaseLedPolicy::Side::AWAY" in m
assert "kDefaultPixels=8" in p and "kMaxPixels=16" in p
assert "kMaxBrightness=128" in p and "kFramePeriodMs=50" in p
assert "kDefaultGoalMs=6000UL" in p
assert "setPowerPlay" in p and "uint32_t pixel" in p
assert "kPulsePeriodMs" in p and "Event::TEST_PP" in p
for route,method in (
    ("/api/case-led","GET"),
    ("/api/case-led/config","POST"),
    ("/api/case-led/test","POST")):
    anchor='server.on("'+route+'", HTTP_'+method+', []() {'
    assert anchor in m,route
    block=m.split(anchor,1)[1].split("server.on(",1)[0]
    assert "if(!authorized())return;" in block,route
    for bad in ("startWledEvent(", "celebrate(", "queueGameOverlay(",
                "addPreviewEvent(", "startHorn(", "s3Audio.play(",
                "setGameFollow(", "toggleGameFollow("):
        assert bad not in block,(route,bad)
config=m.split('server.on("/api/case-led/config"',1)[1].split('server.on(',1)[0]
assert 'request["wiredConfirmed"]|false' in config
assert 'if(enabled && !cfg.caseLedsEnabled' in config
assert 'caseLedFx.stop();' in config
assert "S3CaseLedPolicy::kMaxPixels" in config
assert "S3CaseLedPolicy::kMaxBrightness" in config
test=m.split('server.on("/api/case-led/test"',1)[1].split('server.on(',1)[0]
assert '!cfg.caseLedsEnabled || !caseLedDriver.enabled()' in test
assert "caseLedFx.trigger(" in test
assert "TeamPalette::find" in test
assert "caseLedFx.stop();" in test
for item in ('id="s3CaseLedCard"','id="s3CaseEnabled"',
             'id="s3CasePixels"','id="s3CaseBrightness"',
             'id="s3CaseSave"','id="s3CaseOff"',
             'data-case-mode="goal"','data-case-mode="red"',
             'data-case-mode="green"','data-case-mode="blue"',
             "/api/case-led/config","/api/case-led/test",
             "/api/case-led", "wiredConfirmed=confirm("):
    assert item in ui,item
assert "await loadS3CaseLeds()" in ui
assert 'physicalOutputVerified' in m
for item in ("GPIO17", "GPIO18", "pin 14", "pin 16",
             "74AHCT125", "330", "GND", "5 V", "fuse",
             "not", "Phase 7C"):
    assert item in doc,item
assert '74AHCT125' in svg and 'GPIO17' in svg and 'GPIO18' in svg
assert "SVG" not in svg[:20] # must be valid literal svg file
assert svg.startswith('<svg xmlns="http://www.w3.org/2000/svg"')
assert "ledcWriteTone(AUDIO_CH, nextFreq);" in m
assert "ledcAttachPin(PIN_AUDIO, AUDIO_CH);" in m
assert "PIN_AUDIO = 26" not in d and "PIN_AUDIO = 26" not in p
print("PASS Phase 7C: Waveshare header GPIO17/18, isolated case LED config, no CYD/WLED/audio regressions")
