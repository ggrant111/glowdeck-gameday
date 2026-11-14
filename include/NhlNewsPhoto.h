#pragma once
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <TJpg_Decoder.h>
#ifdef GLOWDECK_WAVESHARE_S3
#include <LittleFS.h>
#else
#include <SPIFFS.h>
#endif

// Single-task photo cache for editorial NHL thumbnails. No image requests or
// JPEG decoder work from draw()/overlays/web handlers. Image writes are capped,
// only attempted during idle mode, and never touch WLED or sound state.
// The JPEG is streamed to an existing filesystem partition, NOT buffered in RAM.
namespace NhlNewsPhoto {
#ifdef GLOWDECK_WAVESHARE_S3
constexpr uint8_t kSlots=10;
constexpr uint32_t kMaxBytes=110000UL;
#else
constexpr uint8_t kSlots=10;
constexpr uint32_t kMaxBytes=65000UL;
#endif
constexpr uint32_t kRetryMs=30UL*60UL*1000UL;
constexpr uint32_t kTimeoutMs=3500UL;

inline uint32_t urlKey(const char *url) {
  uint32_t hash=2166136261UL;
  if(!url)return hash;
  while(*url){hash^=uint8_t(*url++);hash*=16777619UL;}
  return hash;
}
inline bool allowedUrl(const String &url) {
  if(!url.startsWith("https://") || url.length()>220)return false;
  const int path=url.indexOf('/',8);
  if(path<=8)return false;
  const String host=url.substring(8,path);
  // The NHL and ESPN sites supply the article images, not callers.
  return host=="media.d3.nhle.com" || host=="assets.nhle.com" ||
      host=="cms.nhl.bamgrid.com" || host=="a.espncdn.com" ||
      host=="media.nhl.com" || host=="www.nhl.com";
}
inline String slotPath(uint8_t slot) {
  return "/news"+String(slot)+".jpg";
}
struct Slot {
  uint32_t key=0;
  uint32_t touchedAt=0;
  uint32_t attemptedAt=0;
  bool attempted=false, ready=false;
};
class Cache {
public:
  bool begin(bool existingS3Mount=false) {
#ifdef GLOWDECK_WAVESHARE_S3
    // GoalGifPlayer already mounts LittleFS for GIF/WAV user assets. Never
    // format or erase that partition just to support optional news photos.
    mounted_=existingS3Mount;
    storage_=&LittleFS;
#else
    // Only the dedicated CYD SPIFFS partition is affected; Wi-Fi and NVS
    // remain untouched. Once formatted, thumbnails occupy a fixed 10 slots.
    mounted_=SPIFFS.begin(false);
    if(!mounted_)mounted_=SPIFFS.begin(true);
    storage_=&SPIFFS;
#endif
    return mounted_;
  }
  bool ready() const {return mounted_;}
  bool has(const char *url,uint32_t now=0) {
    const int index=find(url);
    if(index<0 || !slots_[index].ready || !mounted_)return false;
    if(!storage_->exists(slotPath(index)))return false;
    if(now)slots_[index].touchedAt=now;
    return true;
  }
  // Store at most one new JPEG per idle card. Callers handle idle eligibility.
  bool fetch(const char *rawUrl,uint32_t now) {
    if(!mounted_ || !rawUrl || !*rawUrl)return false;
    const String url(rawUrl);
    if(!allowedUrl(url))return false;
    const int existing=find(rawUrl);
    if(existing>=0){
      Slot &slot=slots_[existing];
      slot.touchedAt=now;
      if(slot.ready)return false;
      if(slot.attempted && uint32_t(now-slot.attemptedAt)<kRetryMs)
        return false;
    }
    int target=existing;
    if(target<0)target=eviction();
    const String path=slotPath(uint8_t(target));
    Slot &slot=slots_[target];
    storage_->remove(path);
    slot.key=urlKey(rawUrl);
    slot.attemptedAt=now;
    slot.touchedAt=now;
    slot.attempted=true;
    slot.ready=false;
    WiFiClientSecure client;
    client.setInsecure(); // Same transport as the existing NHL services.
    client.setTimeout(2200);
    HTTPClient http;
    http.setConnectTimeout(1800);
    http.setTimeout(2300);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.useHTTP10(true);
    if(!http.begin(client,url))return false;
    http.addHeader("Accept","image/jpeg");
    http.addHeader("Accept-Encoding","identity");
    const char *headers[]={"Content-Type"};
    http.collectHeaders(headers,1);
    const int status=http.GET();
    const String type=http.header("Content-Type");
    const int expected=http.getSize();
    if(status!=HTTP_CODE_OK ||
       (!type.startsWith("image/jpeg") && !type.startsWith("image/jpg")) ||
       expected>(int)kMaxBytes){
      Serial.printf("[NEWS PHOTO] rejected HTTP=%d type=%s size=%d\n",
                    status,type.c_str(),expected);
      http.end();return false;
    }
    File file=storage_->open(path,FILE_WRITE);
    if(!file){http.end();return false;}
    uint8_t buffer[512]={};
    uint32_t total=0;
    int remaining=expected;
    bool valid=true;
    const uint32_t started=millis();
    WiFiClient *stream=http.getStreamPtr();
    while(http.connected() && (remaining!=0) &&
          uint32_t(millis()-started)<kTimeoutMs){
      int avail=stream->available();
      if(avail<=0){delay(1);continue;}
      const int take=min(avail,int(sizeof(buffer)));
      const int got=stream->readBytes(buffer,take);
      if(got<=0)break;
      if(total==0 && (got<2 || buffer[0]!=0xFF || buffer[1]!=0xD8))
        {valid=false;break;}
      if(total+uint32_t(got)>kMaxBytes ||
         file.write(buffer,size_t(got))!=size_t(got))
        {valid=false;break;}
      total+=uint32_t(got);
      if(remaining>0)remaining-=got;
      delay(0); // watchdog-friendly while still lower priority than NHL
    }
    file.close();
    http.end();
    if(!valid || total<512 || remaining>0){
      storage_->remove(path);
      Serial.printf("[NEWS PHOTO] incomplete/unsupported JPEG %luB\n",
                    (unsigned long)total);
      return false;
    }
    uint16_t width=0,height=0;
    if(TJpgDec.getFsJpgSize(&width,&height,path,*storage_)!=JDR_OK ||
       width<32 || height<32 || width>4096 || height>4096){
      storage_->remove(path);
      Serial.println("[NEWS PHOTO] invalid JPEG dimensions or format");
      return false;
    }
    slot.ready=true;
    Serial.printf("[NEWS PHOTO] cached %ux%u %luB slot=%d\n",
                  width,height,(unsigned long)total,target);
    return true;
  }
  // Callback must draw to the CYD panel or the S3 sprite, not a second sprite.
  bool draw(const char *url,int16_t x,int16_t y,int16_t maxW,int16_t maxH,
            SketchCallback output,uint32_t now) {
    if(!mounted_ || !output || maxW<20 || maxH<20)return false;
    const int target=find(url);
    if(target<0 || !slots_[target].ready)return false;
    const String path=slotPath(uint8_t(target));
    if(!storage_->exists(path)){slots_[target].ready=false;return false;}
    uint16_t width=0,height=0;
    if(TJpgDec.getFsJpgSize(&width,&height,path,*storage_)!=JDR_OK)
      return false;
    uint8_t scale=1;
    while(scale<8 && ((width+scale-1)/scale>maxW ||
                      (height+scale-1)/scale>maxH))scale*=2;
    const int16_t scaledW=(width+scale-1)/scale;
    const int16_t scaledH=(height+scale-1)/scale;
    const int16_t drawX=x+(maxW-scaledW)/2;
    const int16_t drawY=y+(maxH-scaledH)/2;
    TJpgDec.setCallback(output);
    TJpgDec.setSwapBytes(true);
    TJpgDec.setJpgScale(scale);
    slots_[target].touchedAt=now;
    return TJpgDec.drawFsJpg(drawX,drawY,path,*storage_)==JDR_OK;
  }
private:
  int find(const char *url) const {
    if(!url || !*url)return -1;
    const uint32_t hash=urlKey(url);
    for(uint8_t i=0;i<kSlots;++i)
      if(slots_[i].attempted && slots_[i].key==hash)return i;
    return -1;
  }
  int eviction() const {
    for(uint8_t i=0;i<kSlots;++i)
      if(!slots_[i].attempted)return i;
    uint8_t oldest=0;
    for(uint8_t i=1;i<kSlots;++i)
      if(int32_t(slots_[i].touchedAt-slots_[oldest].touchedAt)<0)
        oldest=i;
    return oldest;
  }
  Slot slots_[kSlots]={};
  fs::FS *storage_=nullptr;
  bool mounted_=false;
};
} // namespace NhlNewsPhoto
