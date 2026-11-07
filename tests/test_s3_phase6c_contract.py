#!/usr/bin/env python3
"""Phase 6C renderer contracts: S3-only, rollback, truthfulness and parity."""
from pathlib import Path
root=Path(__file__).resolve().parent.parent
main=(root/"src/main.cpp").read_text()
pages=(root/"include/S3BroadcastPages.h").read_text()
config=(root/"platformio.ini").read_text()
layout=(root/"include/S3NativeLayout.h").read_text()
assert "-D GLOWDECK_S3_PHASE6C_UI=1" in config
assert config.index("-D GLOWDECK_S3_PHASE6C_UI=1")>config.index("[env:s3_waveshare_35]")
assert "-D GLOWDECK_S3_PHASE6C_UI" not in config.split("[env:s3_waveshare_35]")[0]
assert '#include "S3BroadcastPages.h"' in main
assert "#if defined(GLOWDECK_S3_PHASE6C_UI) && GLOWDECK_S3_PHASE6C_UI" in main
# Old renderers are retained verbatim, not replaced, as the 6C build flag
# must instantly restore Phase 5 content without touching data or settings.
old=("drawS3Summary","drawS3GamePicker","drawS3Stats","drawS3Standings",
     "drawS3Setup","drawS3TouchCheck","drawS3Empty","drawS3EventStatic",
     "drawS3GifBackdrop")
for name in old:
    assert f"void {name}(" in main, name
new=("drawS3CGameList","drawS3CStats","drawS3CStandings",
     "drawS3CSetup","drawS3CTouchCheck","drawS3CEmpty",
     "drawS3CEventStatic","drawS3CGifBackdrop")
for name in new:
    assert f"void {name}(" in pages, name
    assert f"{name}(" in main, name
assert "drawS3CGameList(false);" in main and "drawS3CGameList(true);" in main
assert "drawS3CEventStatic();" in main and "drawS3EventStatic();" in main
assert "drawS3CGifBackdrop(e);" in main and "drawS3GifBackdrop(e);" in main
assert "drawS3ArenaScoreboard(g);" in main
assert "drawS3Readout(games[" in main
# Decorative renderer must not change NHL, WLED, game selection or events.
for forbidden in ("server.","HTTPClient","WiFi.","pollNhl(",
                  "setGameFollow(", "selectGame(", "celebrate(", "startWled"):
    assert forbidden not in pages, forbidden
assert "tft.fillScreen(S3C_BG);" in pages
assert "tft.present();" not in pages
assert "heap_caps_malloc(" not in pages
assert "new uint16_t" not in pages and "new Game" not in pages
assert "S3BroadcastPages::statsRow(i)" in pages
assert "S3BroadcastPages::standingsRow(row)" in pages
# Existing team and game data MUST remain real rather than placeholders.
for value in ("games[index]","formattedStart(g.startUTC)","gameStatus(g)",
              "detail.loaded", "detail.gameId==g->id", "detail.away[i]",
              "detail.home[i]", "detail.label[i]", "seasonStandings[sorted[index]]",
              "StandingsCarousel::rank(", "entry.points.length()",
              "drawTeamLogo(tft,","rearFollows(g.id)","selectedGameId"):
    assert value in pages, value
# Touch affordances remain within the same 480x320 physical hit map.
for control in ("kPrev","kGames","kNext","kMenu","summaryRow",
                "kSetupTest","kSetupTouchCheck","kSetupColorMode",
                "kTouchCheckTopLeft","kTouchCheckBottomRight",
                "kGifViewport"):
    assert control in pages, control
assert "kOverlayClose" in layout
assert "tft.setTextDatum(previousDatum);" in pages
assert "tft.setTextColor(color);" in pages
assert "cfg.overlayLogos" in pages
assert "drawTeamLogo(tft,String(e.team),12,129,56);" in pages
assert "S3BroadcastPages::kStandingsRowCount" in pages
# GIF stays native, never stretched, and original animations resume as before.
assert "tft.fillRect(viewport.x,viewport.y,viewport.w,viewport.h,TFT_BLACK);" in pages
assert "drawOverlayAnimation(millis());" in main
assert "goalGif.start(tft,S3NativeLayout::kGifViewport.x," in main
assert "overlayController.tick(millis())" in main
print("PASS Phase 6C: S3 pages, S3-only feature flag, old UI rollback, touch/event parity")
