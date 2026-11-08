#!/usr/bin/env python3
"""Phase 6D release gate: on-device UI observations + source-level invariants.

Only the test checks are automated. Human hardware visual/touch results are
documented separately; this file cannot measure actual ST7796 frame timing.
"""
from pathlib import Path

repo=Path(__file__).resolve().parent.parent
main=(repo/"src/main.cpp").read_text()
pages=(repo/"include/S3BroadcastPages.h").read_text()
config=(repo/"platformio.ini").read_text()
policy=(repo/"include/S3BroadcastFramePolicy.h").read_text()
layout=(repo/"include/S3NativeLayout.h").read_text()

# Confirm independent reversible flags and no CYD scope drift.
s3=config.split("[env:s3_waveshare_35]",1)[1]
cyd=config.split("[env:cyd]",1)[1].split("[env:s3_waveshare_35]",1)[0]
for flag in ("GLOWDECK_S3_PHASE6_LIVE=1","GLOWDECK_S3_PHASE6C_UI=1"):
    assert flag in s3 and flag not in cyd, flag

# After an overlay expires or changes queue priority, draw() must restore the
# exact same screen and re-present it, rather than partial-paint stale pixels.
ended=main.index("if (overlayController.tick(millis()))")
# The event-completion path now restores the user-held game before repainting.
# Inspect the COMPLETE recovery block, not an arbitrary 330-character slice:
# a longer, correct hold-restoration path must not fail this release gate.
recovery_end=main.index(
    "if (!overlayController.active() && screen == Screen::LIVE",ended)
expired=main[ended:recovery_end]
assert "lastCarouselAdvance = millis();" in expired
assert "overlayLastFrame = 0;" in expired
assert "partialLiveUpdate = false;" in expired
assert "needsDraw = true;" in expired
assert "carouselHold.pin(overlayReturnHeldGameId);" in expired
assert "if(!overlayController.active() && overlayReturnHeldGameId)" in expired
assert "if (!overlayController.active() && screen == Screen::LIVE" in main

# Manual close and invalid GIF must also route through a full-scene recovery.
assert "gifOverlayFailedStart = overlayController.startedAt();" in main
assert "goalGif.stop();" in main
assert "drawS3CEventStatic();" in main and "drawS3EventStatic();" in main
assert "drawS3CGifBackdrop(e);" in main and "drawS3GifBackdrop(e);" in main
assert "drawS3GoalGifHud(e, now)" in main
assert "drawOverlayAnimation(frameNow);" in main
assert "if (needsDraw) {" in main and "draw();" in main

# Event animations never need a second TFT buffer, don't shift the static
# content, and keep the goal GIF 1:1 (320x240) with its NHL logo.
assert "tft.fillRect(viewport.x,viewport.y,viewport.w,viewport.h,TFT_BLACK);" in pages
assert "drawTeamLogo(tft,String(e.team),12,129,56);" in pages
assert "S3NativeLayout::kGifViewport.x" in main
assert "goalGif.start(tft,S3NativeLayout::kGifViewport.x," in main
assert "tft.fillRect(0,0,6,320,c);" in main  # perimeter-only event pulse
assert "tft.present();" in main
assert "tft.presentRegion(top.x,top.y,top.w,top.h);" in main
assert "tft.presentRegion(bottom.x,bottom.y,bottom.w,bottom.h);" in main

# Full scene on game switch; hidden readouts and untouched pages don't waste
# SPI cycles. A changed game ID must not inherit its predecessor's team marks.
assert "needsFullScene(s3BroadcastLastFrame,next)" in main
assert "drawS3ArenaScoreboard(g);" in main
assert "++s3PerfSkippedFrames;" in main
assert "if(s3ScoreOnlyFrame && (s3PartialMask&S3BroadcastFrame::TOP))" in main
assert "if(s3ScoreOnlyFrame && (s3PartialMask&S3BroadcastFrame::BOTTOM))" in main
assert "constexpr uint32_t kFullBytes=480UL*320UL*2UL;" in policy

# Phase 6B.4 frame-rate / PSRAM diagnostics remain available for real soaks.
for text in ("[S3 6B.4]", "maxTransfer=", "freePSRAM=",
             "ESP.getFreePsram()"):
    assert text in main,text
assert "s3Arena.begin()" in main

# The redesigned secondary pages still use real cached sports data and
# native 480x320 geometry; rendering must stay presentation-only.
for data in ("detail.loaded", "detail.gameId==g->id",
             "StandingsCarousel::collectAndSort(", "games[index]",
             "drawTeamLogo(", "gameStatus(g)", "rearFollows(g.id)"):
    assert data in pages,data
for forbidden in ("heap_caps_malloc(", "HTTPClient", "pollNhl(", "startWled(",
                  "setGameFollow(", "goalGif.start(", "tft.present("):
    assert forbidden not in pages,forbidden
for touch in ("kTouchCheckTopLeft", "kTouchCheckBottomRight",
              "kOverlayClose", "broadcastHitTest", "statsHitTest",
              "standingsHitTest", "kGifViewport"):
    assert touch in layout,touch

print("PASS S3 6D: event recovery, live frame budget, visual-only UI and rollback")
