#include <assert.h>
#include <stdio.h>
#include "MultiGameArming.h"
using namespace MultiGameArming;
int main(){
  Armed a;
  assert(a.add(101));assert(a.add(102));assert(a.add(103));
  assert(a.add(104));assert(a.add(105));
  assert(!a.add(106));assert(a.count()==5);
  assert(a.promote(103));assert(a.primary()==103 && a.rank(101)==1);
  assert(a.move(104,-1));assert(a.rank(104)==2);
  assert(a.remove(103));assert(a.count()==4 && a.primary()==101);
  assert(a.add(106));assert(a.rank(106)==4);
  Queue q;
  Request start;start.gameId=101;start.event=CelebrationEngine::Event::GAME_START;
  Request goal;goal.gameId=105;goal.event=CelebrationEngine::Event::GOAL;
  Request pp;pp.gameId=104;pp.event=CelebrationEngine::Event::POWER_PLAY;
  Request final;final.gameId=102;final.event=CelebrationEngine::Event::FINAL_WIN;
  assert(q.add(start,a));assert(q.add(pp,a));assert(q.add(final,a));assert(q.add(goal,a));
  Request out;
  assert(q.pop(out,a) && out.gameId==105);
  assert(q.pop(out,a) && out.gameId==102);
  assert(q.pop(out,a) && out.gameId==101);
  assert(q.pop(out,a) && out.gameId==104);
  assert(!q.pop(out,a));
  // Equal-priority goals favor the game ranked higher, not the network order.
  Request g1=goal;g1.gameId=105;g1.sequence=10;
  Request g2=goal;g2.gameId=101;g2.sequence=11;
  assert(q.add(g1,a));assert(q.add(g2,a));
  assert(q.pop(out,a) && out.gameId==101);
  q.removeGame(105);assert(q.count==0);
  assert(q.add(goal,a)); // Goal 105 remains armed.
  q.clear();a.clear();assert(!a.primary() && a.count()==0);
  puts("PASS 5 armed game ranks; goal/final/start/PP priority; no unbounded queue");
}
