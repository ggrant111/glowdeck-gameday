#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "GameEventLog.h"
using namespace GameEventLog;
int main() {
  Buffer b;
  assert(b.size() == 0 && b.newest(0) == nullptr);
  Entry &first=b.add(Type::GOAL, 1793750400UL, 12, 123, "UTA","COL","UTA",2,3,3);
  const uint32_t id=first.sequence;
  assert(id==1 && b.size()==1);
  assert(!strcmp(b.newest(0)->team,"UTA"));
  assert(b.newest(0)->awayScore==2 && b.newest(0)->homeScore==3);
  b.find(id)->rear=Rear::ACTIVE;
  b.find(id)->wled[1]=Wled::ACCEPTED;
  b.find(id)->audio=Audio::STARTED;
  b.find(id)->audioSource=2;
  b.find(id)->audioFallback=false;
  assert(!strcmp(audioLabel(b.find(id)->audio),"Playback started"));
  b.find(id)->audio=Audio::COMPLETED;
  assert(!strcmp(audioLabel(b.find(id)->audio),
                 "Stream completed (speaker not verified)"));
  assert(!strcmp(audioLabel(Audio::DISABLED),
                 "Disabled in Sound settings"));
  assert(!strcmp(audioLabel(Audio::NOT_ARMED),
                 "Not armed for this game"));
  assert(!strcmp(audioLabel(Audio::BUSY),
                 "Playback busy or in cooldown"));
  assert(!strcmp(audioLabel(Audio::STREAM_FAILED),"Stream failed"));
  assert(b.find(id)->audioSource==2);
  assert(b.find(id)->rear==Rear::ACTIVE);
  assert(b.find(id)->wled[1]==Wled::ACCEPTED);
  assert(!strcmp(wledLabel(b.find(id)->wled[1]),"Command accepted"));
  for (int i=0;i<kCapacity+4;i++)
    b.add(Type::POWER_PLAY_START,0,i,200+i,"BUF","BOS","BUF",1,1,2);
  assert(b.size()==kCapacity);
  assert(b.find(id)==nullptr);
  assert(b.newest(0)->gameId == 200 + kCapacity + 3);
  assert(b.newest(kCapacity-1)->gameId == 204);
  const uint32_t before=b.lastSequence();
  b.clear();
  assert(!b.size() && b.find(before)==nullptr);
  assert(b.add(Type::PREVIEW_GOAL,0,1,0,nullptr,nullptr,nullptr,0,0,0,true).sequence==before+1);
  assert(b.newest(0)->preview);
  assert(!strcmp(b.newest(0)->away,""));
  assert(matchesCategory(Type::GOAL, "goals"));
  assert(matchesCategory(Type::POWER_PLAY_START, "powerplays"));
  assert(matchesCategory(Type::POWER_PLAY_END, "powerplays"));
  assert(matchesCategory(Type::FINAL_WIN, "other"));
  assert(matchesCategory(Type::PREVIEW_GOAL, "previews"));
  assert(!matchesCategory(Type::GOAL, "previews"));
  assert(!matchesCategory(Type::FINAL_LOSS, "goals"));
  assert(matchesCategory(Type::GOAL, "all"));
  b.newest(0)->wledAcknowledgedMask |= (1 << 2);
  b.newest(0)->wled[2] = Wled::RESTORED;
  assert((b.newest(0)->wledAcknowledgedMask & (1 << 2)) != 0);
  assert(b.newest(0)->wled[2] == Wled::RESTORED);
  assert(!strcmp(rearLabel(Rear::NOT_SELECTED),"Not followed"));
  puts("PASS: fixed-memory event ring, wrap, update, reset, status labels");
}
