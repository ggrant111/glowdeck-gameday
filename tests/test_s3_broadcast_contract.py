#!/usr/bin/env python3
"""S3 Phase 6B.2B source contract: native readout truthfulness and isolation.

Pure static checks complement C++ lifecycle tests and PlatformIO compilation.
They detect accidental full-frame redraws, invented future-game shots or
power plays, and leakage of S3 rendering into the original CYD build.
"""
from pathlib import Path

root = Path(__file__).resolve().parent.parent
main = (root / "src" / "main.cpp").read_text(encoding="utf-8")
header = (root / "include" / "S3BroadcastGameState.h").read_text(encoding="utf-8")
layout = (root / "include" / "S3NativeLayout.h").read_text(encoding="utf-8")

start = main.index("void drawS3ArenaReadout(const Game &g,")
end = main.index("// A freestanding follow star", start)
readout = main[start:end]
assert main.index("#if defined(GLOWDECK_S3_PHASE6_LIVE)") < start
assert main.index("#endif", end) > start

for phase in ("UPCOMING", "LIVE", "INTERMISSION", "FINAL"):
    assert f"S3Broadcast::Phase::{phase}" in readout

assert "S3Broadcast::resolve(" in readout
assert "S3Broadcast::finalStatus(" in readout
assert "S3Broadcast::periodName(" in readout
assert 'badge="INTERMISSION"' in readout
assert 'badge="UPCOMING"' in readout
assert "view.winnerSide" in readout
assert "view.showSeasonRecord" in readout
assert "view.showSpecialTeams" in readout
assert "view.showShots" in readout

# Never show SOG/PP placeholder data in a future or unknown-status game.
assert readout.index("if(view.showSeasonRecord)") < readout.index("else if(view.showShots)")
assert readout.index("if(view.showSpecialTeams)") > readout.index("else if(view.showShots)")
assert "cached->recordAway" in readout and "cached->recordHome" in readout
assert "N/A" in readout and "RECORDS PENDING" in readout
assert "S3Broadcast::specialTeams(" in readout
assert "g.ppDataKnown,g.ppSide,g.ppUpdatedAt" in readout
assert "g.emptyNetKnown,g.emptyNetSide,g.emptyNetUpdatedAt" in readout
assert "status.ppSide && status.emptyNetSides" in readout
assert "status.emptyNetSides==3" in readout
assert 'drawS3ArenaStatusLine(g,"PP "+ppTeam' in readout
assert 'drawS3ArenaStatusLine(g,"EN "+enTeam' in readout
assert "drawS3ArenaStatusLine(const Game &g" in main
assert "S3Broadcast::hasAway(sides)" in main
assert "S3Broadcast::hasHome(sides)" in main
assert "g.awayShots>=0?WHITE:S3_MUTED" in readout
assert "g.homeShots>=0?WHITE:S3_MUTED" in readout
assert 'tft.drawString("TAP FOR STATS",240,195,1);' in readout
assert "s3BroadcastRenderedStatus=visibleStatus.signature();" in readout

# A TTL expiry can update the two status regions even when Wi-Fi and NHL
# polls stop. This path must be readout-only (no extra polling or light actions).
tick=main.split("// Expire a visibly stale PP/goalie-pulled badge on time",1)[1]
tick=tick.split("bool displayChanged = false;",1)[0]
assert "S3Broadcast::needsStatusRefresh(" in tick
assert "partialLiveUpdate=true;" in tick
assert "!overlayController.active()" in tick
assert "fetchDetailStats(" not in tick
assert "pollNhl(" not in tick
assert "startWledEvent(" not in tick

# Dynamic fragments have two bounded, cache-restored regions and no
# full-screen redraw, network fetch or icon reallocation.
assert "s3Arena.restoreRegion(tft,top.x,top.y,top.w,top.h);" in readout
assert "s3Arena.restoreRegion(tft,bottom.x,bottom.y,bottom.w,bottom.h);" in readout
assert "tft.fillScreen(" not in readout
assert "tft.present()" not in readout
assert "HTTPClient" not in readout
assert "fetchDetailStats(" not in readout
assert "drawTeamLogo(" not in readout
assert "tft.setTextDatum(TL_DATUM);" in readout

# Preserve separate event overlays, original CYD and old S3 fallback.
assert "drawS3ArenaReadout(" in main
assert "drawS3Readout(" in main
assert "drawLiveReadout(" in main
assert "s3ScoreOnlyFrame=true;" in main
assert "tft.presentRegion(top.x,top.y,top.w,top.h);" in main
assert "tft.presentRegion(bottom.x,bottom.y,bottom.w,bottom.h);" in main
assert "overlayController.active()" in main
assert "constexpr Rect kBroadcastReadoutTop={144,43,192,160};" in layout
assert "constexpr Rect kBroadcastReadoutBottom={48,215,384,53};" in layout
assert "constexpr Rect kBroadcastDetails={24,88,432,180};" in layout
assert "broadcastTap(dx,dy)" in main
assert "inline bool broadcastTap(int dx,int dy)" in layout
assert "enum class Phase" in header
assert "winnerSide" in header
assert "GLOWDECK_S3_PHASE6_LIVE=1" in (root / "platformio.ini").read_text()
print("PASS: S3 6B.3 team-status, TTL clearing, broad Stats touch, and isolated dirty refresh")
