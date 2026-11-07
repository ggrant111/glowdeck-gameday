#include <cassert>
#include <iostream>
#include "S3BroadcastFramePolicy.h"
using namespace S3BroadcastFrame;

int main(){
  Snapshot empty, game;
  game.valid=true; game.gameId=2026020112U;
  uint32_t top=kFNVOffset,bottom=kFNVOffset;
  mixText(top,"LIVE"); mixText(top,"P2 10:34");
  mixNumber(top,2); mixNumber(top,3);
  mixText(bottom,"SOG"); mixNumber(bottom,15); mixNumber(bottom,21);
  game.top=top; game.bottom=bottom;
  assert(changed(empty,game)==BOTH);
  assert(needsFullScene(empty,game));
  assert(changed(game,game)==NONE);
  assert(!needsFullScene(game,game));

  Snapshot clock=game;
  mixText(clock.top,"P2 10:33");
  assert(changed(game,clock)==TOP);
  assert(transferBytes(changed(game,clock))==kTopBytes);

  Snapshot shots=game;
  mixNumber(shots.bottom,22);
  assert(changed(game,shots)==BOTTOM);
  assert(transferBytes(changed(game,shots))==kBottomBytes);

  Snapshot pp=game;
  mixNumber(pp.bottom,1); // verified PP/EN signature arrival or expiry
  assert(changed(game,pp)==BOTTOM);

  Snapshot both=clock;
  both.bottom=shots.bottom;
  assert(changed(game,both)==BOTH);
  assert(transferBytes(BOTH)==kTopBytes+kBottomBytes);
  assert(transferBytes(BOTH)<kFullBytes);
  assert(transferBytes(NONE)==0);
  assert(kFullBytes==307200UL);

  Snapshot nextGame=game;
  nextGame.gameId++;
  assert(changed(game,nextGame)==BOTH);
  assert(needsFullScene(game,nextGame)); // repaint official logos + nav

  // A new season-record cache can change the bottom while pregame header
  // stays the same; carousel or overlay restoration uses full scene.
  Snapshot record=game;
  mixText(record.bottom,"12-5-3");
  assert(changed(game,record)==BOTTOM);
  assert(!needsFullScene(game,record));

  uint32_t joined=kFNVOffset,separated=kFNVOffset;
  mixText(joined,"AB");mixText(joined,"C");
  mixText(separated,"A");mixText(separated,"BC");
  assert(joined!=separated); // field separators prevent concatenation alias

  std::cout<<"PASS S3 6B.4: separate clock/SOG regions, no-op updates, "
              "full scene on game switch, PSRAM/SPI budget\n";
}
