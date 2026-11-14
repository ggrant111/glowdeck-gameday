#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "NhlNews.h"

// NHL's Forge/Deltatre API is undocumented by the NHL itself; treat fields,
// sorting and team-tag availability as optional. No external proxy required.
// Fetch only from idle/low-priority loop time, never in screen drawing.
namespace NhlNewsService {
constexpr const char *kBase="https://forge-dapi.d3.nhle.com/v2/content/en-us/stories";
struct Result {
  bool ok=false;
  int code=0;
  uint8_t accepted=0;
  const char *error="request failed";
};
inline Result fetch(NhlNews::Catalog &catalog,uint32_t teamMask,
                    uint8_t filteredTeamId=0) {
  Result result;
  if(WiFi.status()!=WL_CONNECTED){result.error="offline";return result;}
  WiFiClientSecure client;
  client.setInsecure(); // Consistent with existing NHL endpoint transport.
  client.setTimeout(6000);
  HTTPClient http;
  http.setConnectTimeout(3500);
  http.setTimeout(6500);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.useHTTP10(true);
  String url=kBase;
  // Ordinary Forge tag filter; optional fallback if NHL changes tagging.
  url+="?%24limit=12"; // Keep filtered JSON within CYD heap budgets.
  if(filteredTeamId)
    url+="&tags.slug=teamid-"+String(filteredTeamId);
  if(!http.begin(client,url)){result.error="TLS begin failed";return result;}
  http.addHeader("Accept","application/json");
  http.addHeader("Accept-Encoding","identity");
  result.code=http.GET();
  if(result.code!=HTTP_CODE_OK) {
    http.end();
    result.error="Forge HTTP error";
    return result;
  }
  // Parse a filtered stream: article bodies, media relations and the full
  // 25-item response are never kept in RAM. Responses may still be large on
  // the wire, so no call is allowed during a game/celebration.
  StaticJsonDocument<1024> filter;
  JsonObject sample=filter.createNestedArray("items").createNestedObject();
  sample["slug"]=true;
  sample["headline"]=true;
  sample["title"]=true;
  sample["summary"]=true;
  sample["contentDate"]=true;
  sample["thumbnail"]["thumbnailUrl"]=true;
  JsonObject tag=sample.createNestedArray("tags").createNestedObject();
  tag["slug"]=true;
  tag["neutralSlug"]=true;
  tag["extraData"]["abbreviation"]=true;
  DynamicJsonDocument doc(18432);
  if(doc.capacity()<18432){http.end();result.error="no memory";return result;}
  const auto parse=deserializeJson(doc,http.getStream(),
     DeserializationOption::Filter(filter),DeserializationOption::NestingLimit(12));
  http.end();
  if(parse){result.error="invalid/truncated JSON";return result;}
  JsonArray items=doc["items"].as<JsonArray>();
  if(items.isNull()){result.error="missing items";return result;}
  for(JsonObject item:items){
    NhlNews::Story story;
    NhlNews::copy(story.slug,sizeof(story.slug),item["slug"] | "");
    const char *headline=item["headline"] | "";
    if(!headline[0])headline=item["title"] | "";
    NhlNews::copy(story.headline,sizeof(story.headline),headline);
    NhlNews::copy(story.summary,sizeof(story.summary),item["summary"] | "");
    NhlNews::copy(story.published,sizeof(story.published),item["contentDate"] | "");
    NhlNews::copy(story.image,sizeof(story.image),
                  item["thumbnail"]["thumbnailUrl"] | "");
    const String link=String("https://www.nhl.com/news/")+story.slug;
    NhlNews::copy(story.url,sizeof(story.url),link.c_str());
    const char *team=NhlNews::teamFromId(filteredTeamId);
    if(team[0]){
      const int8_t index=DisplayEventPolicy::teamIndex(team);
      if(index>=0)story.taggedTeams|=1UL<<index;
    }
    for(JsonObject tag:item["tags"].as<JsonArray>()){
      const char *found=NhlNews::teamFromTag(tag["slug"] | "");
      if(!found[0])found=NhlNews::teamFromTag(tag["neutralSlug"] | "");
      // Some NHL stories store team abbreviations in tag extras.
      if(!found[0]){
        const char *abbr=tag["extraData"]["abbreviation"] | "";
        if(DisplayEventPolicy::teamIndex(abbr)>=0)found=abbr;
      }
      const int8_t index=DisplayEventPolicy::teamIndex(found);
      if(index<0)continue;
      story.taggedTeams|=1UL<<index;
      if(!team[0])team=found;
    }
    NhlNews::copy(story.team,sizeof(story.team),team);
    story.kind=NhlNews::classify(story.headline);
    if(!story.valid())continue;
    catalog.merge(story,teamMask);
    ++result.accepted;
  }
  result.ok=result.accepted>0;
  result.error=result.ok?"":"Forge has no usable stories";
  return result;
}

// ESPN's public Site API is an alternate editorial source, used only when
// Forge fails or its response contains no usable article cards. This is NOT
// a gameplay, score, goal or lighting source.
inline Result fetchEspn(NhlNews::Catalog &catalog,uint32_t teamMask) {
  Result result;
  if(WiFi.status()!=WL_CONNECTED){result.error="offline";return result;}
  WiFiClientSecure client;
  client.setInsecure(); // Same transport behavior as existing NHL requests.
  client.setTimeout(6000);
  HTTPClient http;
  http.setConnectTimeout(3500);
  http.setTimeout(6500);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.useHTTP10(true);
  const char *url=
      "https://site.api.espn.com/apis/site/v2/sports/hockey/nhl/news?limit=10";
  if(!http.begin(client,url)){result.error="ESPN TLS begin failed";return result;}
  http.addHeader("Accept","application/json");
  http.addHeader("Accept-Encoding","identity");
  result.code=http.GET();
  if(result.code!=HTTP_CODE_OK){
    http.end();
    result.error="ESPN HTTP error";
    return result;
  }
  // The raw ESPN response includes dozens of image variants and links per
  // article; explicitly discard them to keep this viable on original CYDs.
  StaticJsonDocument<768> filter;
  JsonObject sample=filter.createNestedArray("articles").createNestedObject();
  sample["id"]=true;
  sample["headline"]=true;
  sample["description"]=true;
  sample["published"]=true;
  sample["type"]=true;
  sample["images"].createNestedObject()["url"]=true;
  sample["links"]["web"]["href"]=true;
  JsonObject category=sample.createNestedArray("categories").createNestedObject();
  category["type"]=true;
  category["team"]["abbreviation"]=true;
  DynamicJsonDocument doc(16384);
  if(doc.capacity()<16384){http.end();result.error="ESPN no memory";return result;}
  const auto parsed=deserializeJson(doc,http.getStream(),
      DeserializationOption::Filter(filter),DeserializationOption::NestingLimit(12));
  http.end();
  if(parsed){result.error="ESPN invalid/truncated JSON";return result;}
  JsonArray items=doc["articles"].as<JsonArray>();
  if(items.isNull()){result.error="ESPN missing articles";return result;}
  for(JsonObject item:items){
    const uint32_t id=item["id"] | 0UL;
    if(!id)continue;
    NhlNews::Story story;
    const String slug=String("espn-")+String(id);
    NhlNews::copy(story.slug,sizeof(story.slug),slug.c_str());
    NhlNews::copy(story.headline,sizeof(story.headline),item["headline"] | "");
    NhlNews::copy(story.summary,sizeof(story.summary),item["description"] | "");
    NhlNews::copy(story.published,sizeof(story.published),item["published"] | "");
    for(JsonObject photo:item["images"].as<JsonArray>()){
      const char *url=photo["url"] | "";
      if(strncmp(url,"https://",8)==0){
        NhlNews::copy(story.image,sizeof(story.image),url);
        break;
      }
    }
    NhlNews::copy(story.source,sizeof(story.source),"ESPN");
    const char *type=item["type"] | "";
    story.kind=NhlNews::classify(story.headline);
    if(NhlNews::includes(type,"recap"))story.kind=NhlNews::Kind::POSTGAME;
    const char *link=item["links"]["web"]["href"] | "";
    // Only expose a verified ESPN page, never an arbitrary URL in the UI.
    String target=link;
    if(target.startsWith("http://"))target="https://"+target.substring(7);
    if((target.startsWith("https://www.espn.com/") ||
        target.startsWith("https://espn.com/")) &&
       target.length()<sizeof(story.url)){
      // ESPN news provides the actual publication URL (which can be a
      // story, feature, or media page). Retain it verbatim where possible.
      NhlNews::copy(story.url,sizeof(story.url),target.c_str());
    }else if(!strcmp(type,"Story") || !strcmp(type,"HeadlineNews")){
      // Fallback only for editorial articles: numeric ESPN ID, never an
      // nhl.com/news/espn-... pseudo-slug. Video/news clips need their
      // original links, so do not manufacture an article path for those.
      const String fallback=String("https://www.espn.com/nhl/story/_/id/")+String(id);
      NhlNews::copy(story.url,sizeof(story.url),fallback.c_str());
    }
    const char *firstTeam="";
    for(JsonObject cat:item["categories"].as<JsonArray>()){
      if(strcmp(cat["type"] | "","team")!=0)continue;
      const char *abbr=cat["team"]["abbreviation"] | "";
      const int8_t index=DisplayEventPolicy::teamIndex(abbr);
      if(index<0)continue;
      story.taggedTeams|=1UL<<index;
      if(!firstTeam[0])firstTeam=abbr;
    }
    NhlNews::copy(story.team,sizeof(story.team),firstTeam);
    if(!story.valid())continue;
    catalog.merge(story,teamMask);
    ++result.accepted;
  }
  result.ok=result.accepted>0;
  result.error=result.ok?"":"ESPN has no usable stories";
  return result;
}
} // namespace NhlNewsService
