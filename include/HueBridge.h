#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "HueColorPolicy.h"

// LOCAL Hue Bridge CLIP v2. HTTPS is required. Bridges advertise certificates
// whose names do not match their LAN IP, so TLS is deliberately unverified
// here; never use outside a trusted private LAN. No cloud or Entertainment API.
// Only one bounded HTTPS operation per tick; no busy loop or background task.
namespace HueBridge {
enum class Stage : uint8_t { IDLE, SNAPSHOT, PAINT, ACTIVE, PULSE, RESTORE };
enum class Result : uint8_t { NONE, PENDING, ACCEPTED, SNAPSHOT_FAILED,
                              COMMAND_FAILED, RESTORED, RESTORE_FAILED };
inline const char *label(Result r){
  switch(r){
    case Result::PENDING:return "Waiting";
    case Result::ACCEPTED:return "Command accepted";
    case Result::SNAPSHOT_FAILED:return "Snapshot failed";
    case Result::COMMAND_FAILED:return "Command failed";
    case Result::RESTORED:return "Restored";
    case Result::RESTORE_FAILED:return "Restore failed";
    default:return "Not triggered";
  }
}
struct Config {
  String ip, key, lights[HuePolicy::kMaxLights];
  bool enabled=false;
  uint8_t events=CelebrationEngine::ALL_EVENTS, brightness=75;
};
class Engine {
public:
  Config config;
  Stage stage=Stage::IDLE;
  CelebrationEngine::Event event=CelebrationEngine::Event::NONE;
  uint32_t color=0, gameId=0, sequence=0, expiresAt=0;
  String lastError;
  uint32_t revision=0, auditSequence=0;
  Result result=Result::NONE;
  bool accepted=false;
  uint8_t succeeded=0;
  bool idle() const {return stage==Stage::IDLE;}
  uint8_t count() const {
    uint8_t n=0;
    for(const auto &id:config.lights)if(id.length())++n;
    return n;
  }
  bool ready() const {
    return config.enabled && config.ip.length() && config.key.length() &&
      count()>0 && WiFi.status()==WL_CONNECTED;
  }
  void report(Result state,bool ack=false){
    result=state; accepted=accepted||ack; auditSequence=sequence; ++revision;
    Serial.printf("[HUE] event=%s game=%lu seq=%lu state=%s accepted=%u"
                  " lights=%u/%u %s\n",CelebrationEngine::label(event),
                  (unsigned long)gameId,(unsigned long)sequence,
                  label(result),unsigned(accepted),unsigned(succeeded),
                  unsigned(count()),lastError.c_str());
  }
  // Refuse public, multicast, broadcast and different-subnet destinations.
  // Bridge discovery and pair only work from the normal STA network.
  static bool validIp(const String &ip){
    IPAddress target,host=WiFi.localIP(),mask=WiFi.subnetMask();
    if(WiFi.status()!=WL_CONNECTED||!target.fromString(ip)||
       target[0]==0||target[0]==127||target[0]>=224||
       target==host||target==IPAddress(255,255,255,255))return false;
    return (uint32_t(target)&uint32_t(mask))==
           (uint32_t(host)&uint32_t(mask));
  }
  int request(const String &ip,const String &key,const char *verb,
              const String &path,const String &body,String *output=nullptr){
    if(!validIp(ip)){lastError="Hue bridge must be on the same LAN";return -1;}
    WiFiClientSecure client;
    client.setInsecure(); // Explicit local-only certificate policy.
    client.setTimeout(1000);
    HTTPClient http;
    if(!http.begin(client,"https://"+ip+path)){
      lastError="TLS connection could not start";return -1;
    }
    http.setConnectTimeout(750);
    http.setTimeout(1100);
    if(key.length())http.addHeader("hue-application-key",key);
    if(body.length())http.addHeader("Content-Type","application/json");
    const int code=!strcmp(verb,"GET")?http.GET():
      !strcmp(verb,"PUT")?http.PUT(body):http.POST(body);
    if(output && code>=200 && code<300) {
      const int len=http.getSize();
      if(len>18000){lastError="Hue response too large";http.end();return -1;}
      *output=http.getString();
      if(output->length()>18000){output->clear();http.end();return -1;}
    }
    http.end();
    if(code<200||code>=300)lastError="Hue HTTP "+String(code);
    return code;
  }
  bool pair(const String &ip,String &newKey){
    String payload="{\"devicetype\":\"glowdeck-gameday#esp32\"}";
    String response;
    int code=request(ip,"","POST","/api",payload,&response);
    if(code<200||code>=300)return false;
    DynamicJsonDocument doc(1536);
    if(deserializeJson(doc,response)){lastError="Invalid Hue pairing response";return false;}
    const char *key=doc[0]["success"]["username"]|""; // v1 key minting.
    if(!key[0]){
      lastError=doc[0]["error"]["description"]| "Press the Hue bridge button";
      return false;
    }
    newKey=key;
    lastError="";
    return true;
  }
  bool list(String &raw){
    if(!config.key.length()){lastError="Pair bridge first";return false;}
    int code=request(config.ip,config.key,"GET",
                     "/clip/v2/resource/light","",&raw);
    return code>=200&&code<300;
  }
  bool selected(uint8_t i) const {
    return i<HuePolicy::kMaxLights && config.lights[i].length();
  }
  void begin(CelebrationEngine::Event next,uint32_t rgb,uint32_t seq,
             uint32_t owner=0) {
    if(!ready()||!HuePolicy::participates(config.enabled,config.events,next))
      return;
    if(stage!=Stage::IDLE && event==next && color==rgb && gameId==owner)
      return;
    // Current original snapshots survive preemption (goal over PP).
    const bool first=stage==Stage::IDLE;
    event=next;color=rgb;gameId=owner;sequence=seq;accepted=false;
    succeeded=0;lastError="";
    if(first){
      for(uint8_t i=0;i<HuePolicy::kMaxLights;++i) {
        saved[i]="";available[i]=false;colorCapable[i]=false;
      }
      stage=Stage::SNAPSHOT;
    }else if(stage!=Stage::SNAPSHOT)stage=Stage::PAINT;
    index=0;
    expiresAt=0;
    report(Result::PENDING);
  }
  void stop(){
    if(stage==Stage::IDLE)return;
    if(stage==Stage::SNAPSHOT){reset();return;} // No bulb modified yet.
    stage=Stage::RESTORE;index=0;attempts=0;
  }
  void reset(){
    stage=Stage::IDLE;event=CelebrationEngine::Event::NONE;
    gameId=sequence=color=expiresAt=0;index=0;attempts=0;
    for(auto &s:saved)s="";
  }
  // Return original on/brightness/color or color-temperature. Off bulbs
  // are restored OFF, rather than incorrectly left illuminated.
  bool snapshot(uint8_t i){
    String response;
    const String path="/clip/v2/resource/light/"+config.lights[i];
    if(request(config.ip,config.key,"GET",path,"",&response)!=200)
      return false;
    DynamicJsonDocument doc(2400);
    if(deserializeJson(doc,response))return false;
    JsonObject light=doc["data"][0].as<JsonObject>();
    if(light.isNull())return false;
    colorCapable[i]=light.containsKey("color");
    StaticJsonDocument<350> previous;
    previous["on"]["on"]=light["on"]["on"]|false;
    if(light["dimming"]["brightness"].is<float>())
      previous["dimming"]["brightness"]=light["dimming"]["brightness"].as<float>();
    if(colorCapable[i] && light["color"]["xy"]["x"].is<float>() &&
       light["color"]["xy"]["y"].is<float>()) {
      previous["color"]["xy"]["x"]=light["color"]["xy"]["x"].as<float>();
      previous["color"]["xy"]["y"]=light["color"]["xy"]["y"].as<float>();
    }else if(light["color_temperature"]["mirek"].is<int>())
      previous["color_temperature"]["mirek"]=
          light["color_temperature"]["mirek"].as<int>();
    serializeJson(previous,saved[i]);
    return !previous.overflowed() && saved[i].length()>0;
  }
  bool paint(uint8_t i,bool pulseLow=false){
    StaticJsonDocument<220> json;
    json["on"]["on"]=true;
    json["dimming"]["brightness"]=
      HuePolicy::brightnessFor(event,config.brightness,pulseLow);
    if(colorCapable[i]){
      const HuePolicy::Xy xy=HuePolicy::rgbToXy(color);
      json["color"]["xy"]["x"]=xy.x;
      json["color"]["xy"]["y"]=xy.y;
    }
    String body;serializeJson(json,body);
    String response;
    const int code=request(config.ip,config.key,"PUT",
      "/clip/v2/resource/light/"+config.lights[i],body,&response);
    if(code<200||code>=300)return false;
    StaticJsonDocument<300> answer;
    if(deserializeJson(answer,response))return false;
    return answer["errors"].as<JsonArray>().size()==0;
  }
  void tick(){
    if(stage==Stage::IDLE || WiFi.status()!=WL_CONNECTED)return;
    const uint32_t now=millis();
    if(lastCommandAt && uint32_t(now-lastCommandAt)<150UL)return;
    if(stage==Stage::ACTIVE){
      if(CelebrationEngine::temporary(event) &&
         int32_t(now-expiresAt)>=0){stop();return;}
      // Gentle two-level PP brightness pulse, no high-frequency entertainment
      // streaming. One request/tick, capped to four chosen bulbs.
      if(event==CelebrationEngine::Event::POWER_PLAY &&
         uint32_t(now-lastPulseAt)>=3200UL) {
        lowPulse=!lowPulse;lastPulseAt=now;
        stage=Stage::PULSE;index=0;
      }
      return;
    }
    if(index>=HuePolicy::kMaxLights){
      if(stage==Stage::SNAPSHOT){stage=Stage::PAINT;index=0;return;}
      if(stage==Stage::PAINT){
        stage=Stage::ACTIVE;
        expiresAt=now+CelebrationEngine::duration(event,8);
        lastPulseAt=now;lowPulse=false;
        report(succeeded?Result::ACCEPTED:Result::COMMAND_FAILED,
               succeeded>0);
        if(!succeeded)stop();
        return;
      }
      if(stage==Stage::PULSE){stage=Stage::ACTIVE;return;}
      if(stage==Stage::RESTORE){
        if(restoreFailures && attempts<2){
          ++attempts;index=0;restoreFailures=0;return;
        }
        report(restoreFailures?Result::RESTORE_FAILED:Result::RESTORED);
        reset();return;
      }
    }
    const uint8_t slot=index++;
    if(!selected(slot))return;
    if(stage==Stage::SNAPSHOT){
      available[slot]=snapshot(slot);
      if(!available[slot])report(Result::SNAPSHOT_FAILED);
    }else if(stage==Stage::PAINT || stage==Stage::PULSE){
      if(!available[slot])return;
      if(paint(slot,stage==Stage::PULSE&&lowPulse)){
        if(stage==Stage::PAINT)++succeeded;
      }else{
        report(Result::COMMAND_FAILED);
      }
    }else if(stage==Stage::RESTORE){
      if(!saved[slot].length())return;
      String response;
      int code=request(config.ip,config.key,"PUT",
          "/clip/v2/resource/light/"+config.lights[slot],
          saved[slot],&response);
      if(code>=200&&code<300){
        StaticJsonDocument<260> answer;
        if(deserializeJson(answer,response) ||
           answer["errors"].as<JsonArray>().size()) {
          ++restoreFailures;
        }else saved[slot]=""; // Never restore the same bulb twice.
      }else ++restoreFailures;
    }
    lastCommandAt=millis();
  }
private:
  String saved[HuePolicy::kMaxLights];
  bool available[HuePolicy::kMaxLights]={};
  bool colorCapable[HuePolicy::kMaxLights]={};
  uint8_t index=0,attempts=0,restoreFailures=0;
  bool lowPulse=false;
  uint32_t lastCommandAt=0,lastPulseAt=0;
};
} // namespace HueBridge
