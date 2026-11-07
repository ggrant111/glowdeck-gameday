"""Display pause must not stop NHL fetch or WLED cleanup."""
from pathlib import Path
main=Path("src/main.cpp").read_text()
layout=Path("include/S3NativeLayout.h").read_text()
icons=Path("include/GameDayIcons.h").read_text()
assert "CarouselHoldPolicy::State carouselHold;" in main
assert "cfg.cycleGames && !carouselHold.paused" in main
assert "currentCarouselPage()" in main and "carouselHold.index(games,gameCount)" in main
assert "carouselHold.reset();" in main
assert "void toggleCarouselHold()" in main
assert "case S3NativeLayout::Action::PAUSE:" in main
assert "toggleCarouselHold();" in main
assert "Action::PAUSE" in layout and "kBroadcastPause" in layout
assert "kBroadcastPrev" in layout and "kBroadcastNext" in layout
assert "GameDayIcons::Kind::GAMES" in main
assert "GameDayIcons::Kind::SETUP" in main
assert "GameDayIcons::Kind::PLAY" in main
assert "GameDayIcons::Kind::PAUSE" in main
assert "GameDayIcons::Kind::PREVIOUS" in main
assert "GameDayIcons::Kind::NEXT" in main
# Method names may appear in explanatory comments. Reject only actual unsafe
# draw calls; the S3 framebuffer uses sprite-safe pixel and fill primitives.
assert "tft.drawCircle(" not in icons and "tft.drawLine(" not in icons
pp=main.split("void observePowerPlayEvents()",1)[1].split("Game *selectedGame()",1)[0]
assert "change.ended" in pp and "scheduleWledRestoreAll();" in pp
assert "wledPpManuallyStoppedGameId" in main
assert "void tickWledCelebrations()" in main
assert "tickWledDdpStreams();" in main
assert "tickRestores();" in main
assert 'server.on("/api/wled/ddp/stop"' in main
print("PASS: hold + S3/CYD controls + verified PP end restore contracts")
