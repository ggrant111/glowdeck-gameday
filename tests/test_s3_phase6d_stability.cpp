#include <cassert>
#include <cstdint>
#include <iostream>
#include "S3BroadcastFramePolicy.h"
#include "S3NativeLayout.h"
#include "S3BroadcastPagesPolicy.h"
#include "EventOverlay.h"

// Phase 6D host-side endurance tests. These prove data/control policies;
// they are NOT substitutes for physical ST7796 frame timing or touch tests.
static EventOverlay::Item event(EventOverlay::Kind kind,
                                uint32_t game,uint32_t seq,
                                uint8_t tier=1){
  EventOverlay::Item x;
  x.kind=kind;
  x.gameId=game;
  x.eventSequence=seq;
  x.tier=tier;
  x.duration=EventOverlay::durationMs(kind,8,7);
  x.color=0xCF0A2C;
  return x;
}
int main(){
  using namespace S3BroadcastFrame;
  using namespace EventOverlay;
  using namespace S3NativeLayout;
  assert(S3BroadcastPages::validPageGeometry());

  // Zero extra frames on a static scoreboard; selective SPI when only
  // one of the independent live data regions changed.
  Snapshot g1;
  g1.gameId=1001;g1.top=0x01u;g1.bottom=0x0Au;g1.valid=true;
  Snapshot clock=g1;clock.top=0x02u;
  Snapshot special=clock;special.bottom=0x0Bu;
  Snapshot g2=special;g2.gameId=1002;
  assert(changed(g1,g1)==NONE);
  assert(!needsFullScene(g1,g1));
  assert(changed(g1,clock)==TOP);
  assert(transferBytes(changed(g1,clock))==kTopBytes);
  assert(changed(clock,special)==BOTTOM);
  assert(transferBytes(changed(clock,special))==kBottomBytes);
  assert(changed(g1,special)==BOTH);
  assert(transferBytes(BOTH)<kFullBytes);
  assert(needsFullScene(special,g2));
  assert(needsFullScene(Snapshot{},g1));

  // One goal is allowed to interrupt PP, then the final-score event
  // takes priority over the resumed PP. Expiry leaves the original page
  // eligible for a full redraw instead of a stale celebration image.
  Controller c;
  const auto pp=event(Kind::POWER_PLAY_START,1001,1);
  const auto goal=event(Kind::GOAL,1001,2);
  const auto final=event(Kind::FINAL_WIN,1001,3);
  assert(c.push(pp,10000));
  assert(c.active() && c.current().kind==Kind::POWER_PLAY_START);
  assert(c.push(goal,11000));
  assert(c.current().kind==Kind::GOAL);
  assert(c.push(final,11010));
  assert(c.current().kind==Kind::GOAL);
  assert(!c.tick(18999));
  assert(c.tick(19000));
  assert(c.current().kind==Kind::FINAL_WIN);
  assert(c.tick(26000));
  assert(c.current().kind==Kind::POWER_PLAY_START);
  assert(c.tick(29500));
  assert(!c.active());
  // Following expiry, same game must be fully restored by runtime's
  // needsDraw path; full RGB565 scene is always larger than BOTH plates.
  assert(kFullBytes==307200u);

  // Explicit dismiss promotes the next queued event, and a final dismiss
  // leaves no lingering queue or active animation.
  c.clear();
  assert(c.push(goal,0));
  assert(c.push(final,100));
  c.dismiss(1000);
  assert(c.active() && c.current().kind==Kind::FINAL_WIN);
  c.dismiss(1200);
  assert(!c.active() && c.pending()==0);

  // Wraparound duration handling is required for long-running S3 devices.
  c.clear();
  const uint32_t nearWrap=UINT32_MAX-1000u;
  assert(c.push(goal,nearWrap));
  assert(c.remaining(nearWrap+4000u)==4000u);
  assert(!c.tick(nearWrap+7999u));
  assert(c.tick(nearWrap+8000u));
  assert(!c.active());

  // Simulate repeated busy-game days; queue length, masks, and nav geometry
  // must stay bounded and stable without a server or allocating framebuffer.
  c.clear();
  uint32_t now=nearWrap;
  Snapshot last=g1;
  for(uint32_t i=0;i<45000;++i){
    now+=1000u;
    if((i%13)==0){
      auto e=event((i%2)?Kind::POWER_PLAY_START:Kind::GOAL,
                   2000+(i%9),i+10,(i%3)==0?2:1);
      c.push(e,now);
    }
    if((i%17)==0){
      auto e=event(Kind::FINAL_WIN,3000+(i%7),i+100000,0);
      c.push(e,now);
    }
    c.tick(now);
    assert(c.pending()<=kQueueSize);
    Snapshot next=last;
    if((i%6)==0) ++next.top;
    if((i%10)==0) ++next.bottom;
    if((i%501)==0)++next.gameId;
    const uint8_t mask=changed(last,next);
    assert(mask<=BOTH);
    assert(transferBytes(mask)<=kTopBytes+kBottomBytes);
    assert(needsFullScene(last,next)==(last.gameId!=next.gameId));
    last=next;
  }
  c.clear();
  assert(!c.active()&&c.pending()==0);
  assert(kGifViewport.w==320 && kGifViewport.h==240);
  assert(overlayCloseHit(452,20));
  assert(broadcastHitTest(240,148).action==Action::DETAILS);
  assert(statsHitTest(398,24).action==Action::FOLLOW);
  assert(standingsHitTest(406,296).action==Action::NEXT);
  std::cout<<"PASS S3 6D: 45k-cycle screen policy, queued event recovery, "
              "wraparound, SPI bounds and unchanged navigation\n";
}
