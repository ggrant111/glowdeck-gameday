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
  if(filteredTeamId)
    url+="?tags.slug=teamid-"+String(filteredTeamId);
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
    const char *team=NhlNews::teamFromId(filteredTeamId);
    for(JsonObject tag:item["tags"].as<JsonArray>()){
      const char *found=NhlNews::teamFromTag(tag["slug"] | "");
      if(!found[0])found=NhlNews::teamFromTag(tag["neutralSlug"] | "");
      if(found[0]){team=found;break;}
      // Some NHL stories store team abbreviations in tag extras.
      const char *abbr=tag["extraData"]["abbreviation"] | "";
      if(DisplayEventPolicy::teamIndex(abbr)>=0){team=abbr;break;}
    }
    NhlNews::copy(story.team,sizeof(story.team),team);
    story.kind=NhlNews::classify(story.headline);
    if(!story.valid())continue;
    catalog.merge(story,teamMask);
    ++result.accepted;
  }
  result.ok=true;
  result.error="";
  return result;
}
} // namespace NhlNewsService
