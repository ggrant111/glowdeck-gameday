#!/usr/bin/env python3
"""Phase 6B.4 S3-only dirty-frame and event recovery source-level guards."""
from pathlib import Path

root=Path(__file__).resolve().parent.parent
main=(root/"src/main.cpp").read_text()
policy=(root/"include/S3BroadcastFramePolicy.h").read_text()
assert "#ifdef GLOWDECK_WAVESHARE_S3\n#include \"GameDayS3Hardware.h\"" in main
assert '#include "S3BroadcastFramePolicy.h"' in main
assert "S3BroadcastFrame::Snapshot s3BroadcastLastFrame;" in main
assert "S3BroadcastFrame::Snapshot captureS3ArenaFrame(" in main
assert "S3BroadcastFrame::mixText(top,g.clock.c_str());" in main
assert "S3BroadcastFrame::mixText(top,g.periodType.c_str());" in main
assert "S3BroadcastFrame::mixNumber(bottom,special.signature());" in main
assert "carouselStats[i].id==g.id" in main
assert "carouselStats[i].recordAway.c_str()" in main
assert "carouselStats[i].recordHome.c_str()" in main

start=main.index("void drawS3ArenaReadout(const Game &g,")
end=main.index("// A freestanding follow star",start)
render=main[start:end]
assert "if(mask&S3BroadcastFrame::TOP)" in render
assert "if(mask&S3BroadcastFrame::BOTTOM)" in render
assert "s3Arena.restoreRegion(tft,top.x,top.y,top.w,top.h);" in render
assert "s3Arena.restoreRegion(tft,bottom.x,bottom.y,bottom.w,bottom.h);" in render
assert "s3BroadcastLastFrame=captureS3ArenaFrame(g,millis());" in render
assert "tft.fillScreen(" not in render
assert "drawTeamLogo(" not in render
assert "HTTPClient" not in render

partial=main.split("} else if (partialLiveUpdate && screen == Screen::LIVE",1)[1]
partial=partial.split("// Update one optional pregame",1)[0]
assert "S3BroadcastFrame::needsFullScene(" in partial
assert "drawS3ArenaScoreboard(g);" in partial
assert "S3BroadcastFrame::changed(s3BroadcastLastFrame,next)" in partial
assert "if(s3PartialMask)" in partial
assert "drawS3ArenaReadout(g,s3PartialMask);" in partial
assert "++s3PerfSkippedFrames;" in partial
assert "if(s3ScoreOnlyFrame && (s3PartialMask&S3BroadcastFrame::TOP))" in partial
assert "if(s3ScoreOnlyFrame && (s3PartialMask&S3BroadcastFrame::BOTTOM))" in partial
assert "if(s3TickerFrame)tft.presentRegion(0,269,480,20);" in partial
assert "tft.present();" in partial
assert "[S3 6B.4]" in partial
assert "ESP.getFreePsram()" in partial

# Other S3 screens, overlays and fallback must retain a whole-canvas present.
assert "partialLiveUpdate = false;\n    needsDraw = true;" in main
assert "overlayController.tick(millis())" in main
assert "overlayLastFrame = 0;" in main
assert "drawS3GoalGifHud(e, now)" in main
assert "if(s3Arena.begin()) drawS3ArenaScoreboard(g);" in main
assert "else drawS3Scoreboard(g);" in main
assert "GLOWDECK_S3_PHASE6_LIVE" in main
assert "constexpr uint32_t kFullBytes=480UL*320UL*2UL;" in policy
print("PASS S3 Phase 6B.4: independent changes, event full restoration, no extra buffer")
