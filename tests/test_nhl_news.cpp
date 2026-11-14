#include <assert.h>
#include <stdio.h>
#include <cstring>
#include "NhlNews.h"

static NhlNews::Story story(const char *slug,const char *date,
                            const char *team,const char *headline) {
  NhlNews::Story s;
  NhlNews::copy(s.slug,sizeof(s.slug),slug);
  NhlNews::copy(s.headline,sizeof(s.headline),headline);
  NhlNews::copy(s.published,sizeof(s.published),date);
  NhlNews::copy(s.team,sizeof(s.team),team);
  s.kind=NhlNews::classify(s.headline);
  return s;
}
int main() {
  using namespace NhlNews;
  assert(strcmp(teamFromTag("teamid-59"),"UTA")==0);
  assert(strcmp(teamFromTag("teamid-6"),"BOS")==0);
  assert(!teamFromTag("teamid-other")[0]);
  assert(!teamFromTag("teamid-999")[0]);
  assert(idFromTeam("UTA")==59);
  assert(idFromTeam("BOS")==6);
  assert(classify("Tonight's game preview")==Kind::PREGAME);
  assert(classify("Projected Lineups")==Kind::PREGAME);
  assert(classify("Game recap: Mammoth 3 - Bruins 2")==Kind::POSTGAME);
  assert(classify("GameDay roundtable")==Kind::GENERAL);
  const uint32_t utah=(1UL<<DisplayEventPolicy::teamIndex("UTA"));
  Catalog cache;
  cache.merge(story("league-new","2026-11-13T10:00:00Z","","League news"),utah);
  cache.merge(story("uta-older","2026-11-12T10:00:00Z","UTA","Utah preview"),utah);
  cache.merge(story("league-old","2026-11-10T10:00:00Z","","League news"),utah);
  assert(cache.count==3);
  assert(strcmp(cache.ranked(0,utah)->slug,"uta-older")==0);
  assert(strcmp(cache.ranked(1,utah)->slug,"league-new")==0);
  assert(cache.matching(utah,Filter::FOLLOWED)==1);
  assert(cache.matching(utah,Filter::PREGAME)==1);
  assert(cache.matching(utah,Filter::POSTGAME)==0);
  assert(cache.ranked(1,utah,Filter::FOLLOWED)==nullptr);
  cache.merge(story("uta-older","2026-11-13T11:00:00Z","UTA","Game recap"),utah);
  assert(cache.count==3); // duplicate slug updates; never repeats.
  assert(cache.ranked(0,utah)->kind==Kind::POSTGAME);
  for(int i=0;i<22;++i){
    char slug[32],date[32];
    snprintf(slug,sizeof(slug),"league-%d",i);
    snprintf(date,sizeof(date),"2026-11-%02dT10:00:00Z",i+1);
    cache.merge(story(slug,date,"","League story"),utah);
  }
  assert(cache.count==kMaxStories);
  assert(strcmp(cache.ranked(0,utah)->slug,"uta-older")==0);
  assert(cache.matching(utah,Filter::FOLLOWED)==1);
  printf("PASS Phase 9 Forge news: team tags, classification, dedupe, ranking, bounded cache\n");
}
