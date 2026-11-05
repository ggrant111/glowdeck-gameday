#include <assert.h>
#include <stdio.h>
#include <string>
#include "LiveCarousel.h"
struct Game {uint32_t id; std::string state,away,home; Game(uint32_t x,const char* st):id(x),state(st),away("DAL"),home("ANA"){} };
using namespace LiveCarousel;
int main(){
  Game g[] = {
    {101,"FUT"},{102,"LIVE"},{103,"OFF"},{104,"FUT"},
    {105,"CRIT"},{106,"FINAL"},{107,"PRE"},{108,"LIVE"},
    {109,"OFF"},{110,"FUT"},{111,"FINAL"},{112,"PRE"}
  };
  // A favorite team should lead the LIVE carousel without removing games.
  g[4].home="UTA";
  const uint32_t fav=(1UL<<DisplayEventPolicy::teamIndex("UTA"));
  const Plan favoritePlan=build(g,12,true,fav);
  assert(favoritePlan.at(0).kind==Kind::GAME && g[favoritePlan.at(0).index].id==105);
  assert(favoritePlan.liveCount==3 && favoritePlan.otherCount==9);
  assert(locateGame(favoritePlan,g,102)==2);
  assert(restore(favoritePlan,g,Kind::GAME,102,0,0)==2);
  const Plan p=build(g,12,true);
  assert(p.liveMode && p.liveCount==3 && p.otherCount==9);
  assert(p.summaryPages==3 && p.count==6); // 3 live + ceil(9/4)
  assert(p.at(0).kind==Kind::GAME && g[p.at(0).index].id==102);
  assert(p.at(1).kind==Kind::GAME && g[p.at(1).index].id==105);
  assert(p.at(2).kind==Kind::GAME && g[p.at(2).index].id==108);
  assert(p.at(3).kind==Kind::SUMMARY && p.at(3).count==4);
  assert(p.at(4).count==4 && p.at(5).count==1);
  assert(g[p.at(3).rows[0]].id==101);
  assert(g[p.at(3).rows[1]].id==103);
  assert(g[p.at(4).rows[0]].id==107);
  assert(g[p.at(5).rows[0]].id==112);
  assert(advance(5,p,1)==0);
  assert(advance(0,p,-1)==5);
  assert(locateGame(p,g,102)==0);
  assert(locateGame(p,g,110)==4);
  assert(restore(p,g,Kind::GAME,105,0,0)==1);
  assert(restore(p,g,Kind::SUMMARY,110,0,0)==4);
  assert(restore(p,g,Kind::STANDINGS,0,4,0)==0); // no standings live
  // Reorder games in NHL feed; preserve game identity not old index.
  Game reordered[]={
    {108,"LIVE"},{101,"FUT"},{105,"CRIT"},{102,"LIVE"},
    {103,"OFF"},{104,"FUT"},{106,"FINAL"},{107,"PRE"},
    {109,"OFF"},{110,"FUT"},{111,"FINAL"},{112,"PRE"}
  };
  const Plan reorderedPlan=build(reordered,12,true);
  assert(restore(reorderedPlan,reordered,Kind::GAME,102,0,0)==2);
  // A finished live game moves to a summary, preserving its matchup.
  g[1].state="FINAL";
  const Plan afterFinal=build(g,12,true);
  assert(afterFinal.liveMode && afterFinal.liveCount==2);
  assert(afterFinal.summaryPages==3 && afterFinal.count==5);
  assert(afterFinal.at(locateGame(afterFinal,g,102)).kind==Kind::SUMMARY);
  // At a night with no active games, restore original all-game + 8 tables.
  for(auto &game:g)game.state="FINAL";
  const Plan quiet=build(g,12,true);
  assert(!quiet.liveMode && quiet.count==20 && !quiet.summaryPages);
  assert(quiet.at(0).kind==Kind::GAME && quiet.at(12).kind==Kind::STANDINGS);
  assert(restore(quiet,g,Kind::STANDINGS,0,7,0)==19);
  assert(restore(quiet,g,Kind::GAME,102,0,0)==1);
  assert(build(g,12,false).count==12);
  const Plan empty=build(g,0,true);
  assert(!empty.liveMode && empty.count==8 && empty.at(0).kind==Kind::STANDINGS);
  const Plan onlyLive=build(reordered,4,true);
  assert(onlyLive.liveMode && onlyLive.liveCount==3 && onlyLive.count==4);
  for(auto &game:g)game.state="LIVE";
  const Plan everyLive=build(g,12,true);
  assert(everyLive.count==12 && everyLive.summaryPages==0);
  puts("PASS live-first NHL carousel, four-game summaries, fallback standings, restore, wrap");
}
