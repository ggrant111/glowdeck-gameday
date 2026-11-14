#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include "DisplayEventPolicy.h"

// Small, allocation-free Forge news model shared by CYD and ESP32-S3.
// This is editorial content, never an NHL game-event or lighting trigger.
namespace NhlNews {
constexpr uint8_t kMaxStories = 10;
constexpr uint32_t kRefreshMs = 8UL * 60UL * 1000UL;
constexpr uint32_t kRetryMs = 2UL * 60UL * 1000UL;
constexpr uint32_t kMinimumBootMs = 20000UL;
constexpr uint32_t kCardMs = 10000UL;

enum class Kind : uint8_t { GENERAL, PREGAME, POSTGAME };
enum class Filter : uint8_t { ALL, FOLLOWED, PREGAME, POSTGAME };

inline void copy(char *dst, size_t capacity, const char *src) {
  if (!capacity) return;
  if (!src) src="";
  strncpy(dst,src,capacity-1);
  dst[capacity-1]=0;
}
inline bool includes(const char *text,const char *part) {
  if(!text || !part || !*part)return false;
  for(const char *s=text;*s;++s) {
    const char *a=s,*b=part;
    while(*a && *b && tolower((unsigned char)*a)==tolower((unsigned char)*b))
      ++a,++b;
    if(!*b)return true;
  }
  return false;
}
inline Kind classify(const char *title) {
  // Conservative editorial hints; not authoritative lineup/injury or game data.
  if(includes(title,"preview") || includes(title,"projected lineup") ||
     includes(title,"projected lineups") || includes(title,"pregame"))
    return Kind::PREGAME;
  if(includes(title,"recap") || includes(title,"postgame") ||
     includes(title,"takeaways") || includes(title,"game summary"))
    return Kind::POSTGAME;
  return Kind::GENERAL;
}
inline const char *kindLabel(Kind kind) {
  return kind==Kind::PREGAME?"PREGAME":kind==Kind::POSTGAME?"POSTGAME":"NHL NEWS";
}
struct IdTeam { uint8_t id; const char *abbr; };
static const IdTeam kIdTeams[] = {
 {24,"ANA"},{6,"BOS"},{7,"BUF"},{20,"CGY"},{12,"CAR"},{16,"CHI"},
 {21,"COL"},{29,"CBJ"},{25,"DAL"},{17,"DET"},{22,"EDM"},{13,"FLA"},
 {26,"LAK"},{30,"MIN"},{8,"MTL"},{1,"NJD"},{18,"NSH"},{2,"NYI"},
 {3,"NYR"},{9,"OTT"},{4,"PHI"},{5,"PIT"},{55,"SEA"},{28,"SJS"},
 {19,"STL"},{14,"TBL"},{10,"TOR"},{59,"UTA"},{23,"VAN"},{54,"VGK"},
 {15,"WSH"},{52,"WPG"}
};
inline const char *teamFromId(unsigned id) {
  for(const auto &t:kIdTeams)if(t.id==id)return t.abbr;
  return "";
}
inline uint8_t idFromTeam(const char *abbr) {
  for(const auto &t:kIdTeams)if(!strcmp(t.abbr,abbr))return t.id;
  return 0;
}
inline const char *teamFromTag(const char *tag) {
  if(!tag || strncmp(tag,"teamid-",7)!=0)return "";
  unsigned id=0;
  for(const char *p=tag+7;*p;++p){
    if(*p<'0'||*p>'9')return "";
    id=id*10U+unsigned(*p-'0');
    if(id>999)return "";
  }
  return teamFromId(id);
}

struct Story {
  char slug[128]={};
  char headline[116]={};
  char summary[180]={};
  char published[26]={};
  char team[5]={};
  uint32_t taggedTeams=0; // One story may reference multiple NHL teams.
  char image[192]={};
  char url[192]={}; // Per-story canonical link; ESPN fallback is not on NHL.com.
  char source[8]="NHL";
  Kind kind=Kind::GENERAL;
  bool valid() const {return slug[0] && headline[0];}
};
inline bool followed(const Story &s,uint32_t favorites) {
  if(s.taggedTeams)return (s.taggedTeams & favorites)!=0;
  return DisplayEventPolicy::isFavorite(favorites,s.team);
}
inline bool matches(const Story &s,Filter filter,uint32_t favorites) {
  return filter==Filter::ALL ||
    (filter==Filter::FOLLOWED && followed(s,favorites)) ||
    (filter==Filter::PREGAME && s.kind==Kind::PREGAME) ||
    (filter==Filter::POSTGAME && s.kind==Kind::POSTGAME);
}
inline bool ahead(const Story &a,const Story &b,uint32_t favorites) {
  const bool fa=followed(a,favorites);
  const bool fb=followed(b,favorites);
  if(fa!=fb)return fa;
  // The API places featured stories ahead of newer stories. Always order
  // chronologically within each followed/league tier.
  return strcmp(a.published,b.published)>0;
}
struct Catalog {
  Story stories[kMaxStories]={};
  uint8_t count=0;
  uint32_t lastSuccessAt=0;
  int16_t lastHttpCode=0;
  uint8_t failures=0;
  char source[12]="Pending";

  void merge(const Story &story,uint32_t favorites) {
    if(!story.valid())return;
    for(uint8_t i=0;i<count;++i)
      if(!strcmp(stories[i].slug,story.slug)){
        Story update=story;
        if(!update.team[0])copy(update.team,sizeof(update.team),stories[i].team);
        if(!update.taggedTeams)update.taggedTeams=stories[i].taggedTeams;
        stories[i]=update;
        return;
      }
    if(count<kMaxStories){stories[count++]=story;return;}
    // The fixed cache favors followed-team stories, otherwise newer articles.
    // Never evict a followed story for an older general-league headline.
    uint8_t weakest=0;
    for(uint8_t i=1;i<count;++i)
      if(ahead(stories[weakest],stories[i],favorites))weakest=i;
    if(ahead(story,stories[weakest],favorites))
      stories[weakest]=story;
  }
  const Story *ranked(uint8_t rank,uint32_t favorites,
                      Filter filter=Filter::ALL) const {
    bool used[kMaxStories]={};
    for(uint8_t slot=0;slot<=rank;++slot) {
      int best=-1;
      for(uint8_t i=0;i<count;++i){
        if(used[i] || !matches(stories[i],filter,favorites))continue;
        if(best<0 || ahead(stories[i],stories[best],favorites))best=i;
      }
      if(best<0)return nullptr;
      used[best]=true;
      if(slot==rank)return &stories[best];
    }
    return nullptr;
  }
  uint8_t matching(uint32_t favorites,Filter filter) const {
    uint8_t n=0;
    for(uint8_t i=0;i<count;++i)
      if(matches(stories[i],filter,favorites))++n;
    return n;
  }
};
} // namespace NhlNews
