"""Display pause must not stop NHL fetch or WLED cleanup."""
from pathlib import Path
main=Path("src/main.cpp").read_text()
layout=Path("include/S3NativeLayout.h").read_text()
icons=Path("include/GameDayIcons.h").read_text()
assert "CarouselHoldPolicy::State carouselHold;" in main
assert "cfg.cycleGames && !newsViewEnabled()" in main
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
# A PP-end overlay must not call the old global restore: another armed
# game may still own the WLED lights. The scheduler resolves PP ownership.
pp=main.split("void observePowerPlayEvents() {",1)[1].split("Game *gameById",1)[0]
assert "change.ended" in pp
assert "scheduleWledRestoreAll();" not in pp
tick=main.split("void tickWledCelebrations() {",1)[1].split(
    "// Compact score/state fingerprint:",1)[0]
assert "ppSide=currentWledPowerPlay(ppColor,ppGameId,ppSeq);" in tick
assert "if(wledActiveEvent==CelebrationEngine::Event::POWER_PLAY)" in tick
assert "if(!hasPP)" in tick and "scheduleWledRestoreAll();" in tick
assert "wledActiveGameId!=ppGameId" in tick
assert "const bool ddpReleased=releaseWledDdp(i);" in main
assert "suppressArmedPowerPlays();" in main
# Event alerts must behave as temporary overlays, NOT carousel navigation.
# Check both auto-expiration and the overlay-close touch path.
assert "uint32_t overlayReturnHeldGameId=0;" in main
assert "if(carouselHold.paused && !overlayReturnHeldGameId)" in main
assert "overlayReturnHeldGameId=carouselHold.gameId;" in main
assert "carouselHold.onFeedRefresh(!newDay);" in main
assert "if(!overlayController.active() && overlayReturnHeldGameId)" in main
assert "carouselHold.pin(overlayReturnHeldGameId);" in main
assert "overlayReturnHeldGameId=0; // Explicit game selection" in main
assert "overlayController.dismiss(millis());" in main
# The whole touch function is reliable to inspect independently of #ifdef order.
touch=main.split("void handleTouch() {",1)[1].split("  if (screen == Screen::LIVE) {",1)[0]
assert "swallowLiveGesture=true;" in touch
assert "overlayController.dismiss(millis());" in touch
assert "cfg.cycleGames && !newsViewEnabled()" in main
assert "wledPpManuallyStoppedGameId" in main
assert "void tickWledCelebrations()" in main
assert "tickWledDdpStreams();" in main
assert "tickRestores();" in main
assert 'server.on("/api/wled/ddp/stop"' in main
print("PASS: hold + S3/CYD controls + verified PP end restore contracts")
