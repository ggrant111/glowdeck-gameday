#pragma once
// GlowDeck: minimal Improv Serial v1 endpoint, shared by CYD and S3.
// Input packets are validated before field reads. Each valid frame resets the
// parser BEFORE processing, so back-to-back RPCs cannot swallow the next 'I'.
// https://www.improv-wifi.com/serial/
#include <Arduino.h>
#include <WiFi.h>
#include <Stream.h>
#include <stdint.h>
#include <string.h>

class GameDayImprovSerial {
 public:
  enum ChipFamily : uint8_t { ESP32 = 0, ESP32_C3 = 1, ESP32_S2 = 2,
                              ESP32_S3 = 3, ESP8266 = 4 };
  using ConnectCallback = bool (*)(const char *,const char *);
  using ConnectedCallback = void (*)(const char *,const char *);
  using ErrorCallback = void (*)(uint8_t);

  explicit GameDayImprovSerial(Stream &stream) : io_(stream) {}
  void setDeviceInfo(ChipFamily family, const char *firmware,
                     const char *version, const char *name,
                     const char *url = "http://{LOCAL_IPV4}/") {
    family_=family;
    firmware_=firmware;
    version_=version;
    name_=name;
    url_=url;
  }
  void setCustomConnectWiFi(ConnectCallback fn) { connect_=fn; }
  void onImprovConnected(ConnectedCallback fn) { connected_=fn; }
  void onImprovError(ErrorCallback fn) { error_=fn; }

  void handleSerial() {
    if (!io_.available()) return;
    const uint8_t byte = uint8_t(io_.read());
    static const uint8_t kMagic[6] = {'I','M','P','R','O','V'};
    if (position_ < 6) {
      if(byte == kMagic[position_]) buffer_[position_++] = byte;
      else {
        // Restart on a repeated 'I' instead of losing the next header.
        position_=byte == 'I' ? 1 : 0;
        if(position_) buffer_[0]='I';
      }
      return;
    }
    if(position_ == 6 && byte != 1) { position_=0; return; }
    if(position_ == 7 && byte != 3) { position_=0; return; }
    if(position_ == 8 && byte > kMaxRpcBytes) { position_=0; return; }
    if(position_ >= sizeof(buffer_)) { position_=0; return; }
    buffer_[position_++] = byte;
    if(position_ < 9) return;
    const size_t total = size_t(10) + buffer_[8];
    if(position_ != total) return;
    uint8_t checksum=0;
    for(size_t i=0;i<total-1;++i)checksum=uint8_t(checksum+buffer_[i]);
    // Reset before executing callbacks so a second RPC begins at position 0.
    position_=0;
    if(checksum != buffer_[total-1]) { sendError(1); return; }
    handleRpc(&buffer_[9],buffer_[8]);
  }

 private:
  static constexpr uint8_t kMaxRpcBytes=128;
  Stream &io_;
  uint8_t buffer_[10+kMaxRpcBytes]={};
  size_t position_=0;
  ChipFamily family_=ESP32;
  const char *firmware_="GlowDeck GameDay",*version_="unknown",
             *name_="GlowDeck GameDay",*url_="http://{LOCAL_IPV4}/";
  ConnectCallback connect_=nullptr;
  ConnectedCallback connected_=nullptr;
  ErrorCallback error_=nullptr;

  void packet(uint8_t type,const uint8_t *data,uint8_t length) {
    uint8_t header[9]={'I','M','P','R','O','V',1,type,length};
    uint8_t sum=0;
    for(const uint8_t c:header)sum=uint8_t(sum+c);
    io_.write(header,sizeof(header));
    if(length) {
      io_.write(data,length);
      for(uint8_t i=0;i<length;++i)sum=uint8_t(sum+data[i]);
    }
    io_.write(sum);
  }
  void state(uint8_t value) { packet(1,&value,1); }
  void sendError(uint8_t value) {
    packet(2,&value,1);
    if(value && error_)error_(value);
  }
  static bool appendString(uint8_t *dst,uint8_t &count,
                           const char *value) {
    if(!value)return false;
    const size_t length=strlen(value);
    if(length>255 || size_t(count)+length+1>255)return false;
    dst[count++]=uint8_t(length);
    memcpy(dst+count,value,length);
    count=uint8_t(count+length);
    return true;
  }
  void result(uint8_t command,const char *a=nullptr,const char *b=nullptr,
              const char *c=nullptr,const char *d=nullptr) {
    uint8_t data[255]={};
    uint8_t used=2;
    data[0]=command;
    for(const char *str:{a,b,c,d}) {
      if(str && !appendString(data,used,str)) { sendError(1); return; }
    }
    data[1]=uint8_t(used-2);
    packet(4,data,used);
  }
  void localUrl(uint8_t cmd) {
    String url=url_;
    const IPAddress ip=WiFi.localIP();
    url.replace("{LOCAL_IPV4}",ip.toString());
    result(cmd,url.c_str());
  }
  void handleRpc(const uint8_t *rpc,size_t length) {
    if(length<2 || size_t(rpc[1])+2!=length){sendError(1);return;}
    const uint8_t cmd=rpc[0];
    sendError(0);
    switch(cmd){
      case 2: // GET_CURRENT_STATE: state + local URL when already connected
        if(WiFi.status()==WL_CONNECTED) {state(4);localUrl(2);}
        else state(2);
        return;
      case 3: { // GET_DEVICE_INFO: firmware, version, chip, display name
        static const char *families[]={"ESP32","ESP32-C3","ESP32-S2",
                                       "ESP32-S3","ESP8266"};
        result(3,firmware_,version_,families[family_],name_);
        return;
      }
      case 4: { // GET_WIFI_NETWORKS: bounded scan (no logging passwords)
        int found=WiFi.scanNetworks(false,true);
        if(found<0)found=0;
        if(found>32)found=32;
        for(int i=0;i<found;++i){
          String ssid=WiFi.SSID(i);
          String rssi=String(WiFi.RSSI(i));
          const char *secured=WiFi.encryptionType(i)==WIFI_AUTH_OPEN?"NO":"YES";
          if(ssid.length() && ssid.length()<=32)
            result(4,ssid.c_str(),rssi.c_str(),secured);
        }
        WiFi.scanDelete();
        result(4); // empty response means scan finished
        return;
      }
      case 1: { // WIFI_SETTINGS: data=[ssid len,ssid,pass len,pass]
        const size_t d=rpc[1];
        const uint8_t *field=&rpc[2];
        if(d<2 || field[0]==0 || field[0]>32 ||
           size_t(field[0])+2>d){sendError(1);return;}
        const size_t pwIndex=size_t(field[0])+1;
        const size_t pwLen=field[pwIndex];
        if(pwLen>63 || pwIndex+1+pwLen!=d){sendError(1);return;}
        char ssid[33]={},pw[64]={};
        memcpy(ssid,field+1,field[0]);
        memcpy(pw,field+pwIndex+1,pwLen);
        state(3);
        bool ok=connect_ && connect_(ssid,pw);
        if(ok){
          sendError(0);
          state(4);
          localUrl(1);
          if(connected_)connected_(ssid,pw);
        } else {
          state(2);
          sendError(3);
        }
        // Wipe the stack credential buffers as soon as used.
        memset(pw,0,sizeof(pw));
        memset(ssid,0,sizeof(ssid));
        return;
      }
      default: sendError(2); return;
    }
  }
};
