#pragma once
#ifdef GLOWDECK_WAVESHARE_S3
#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include "S3WavPolicy.h"

// S3-only LittleFS sound library. Three fixed, non-user-controlled paths.
// Existing /goal.gif is never touched. Two-stage install keeps the old
// sound intact if upload fails, and restores it after a power interruption.
class S3SoundStorage {
 public:
  static constexpr const char *kTemp="/sound.tmp";
  static constexpr const char *kBackup="/sound.bak";
  void begin(bool mounted){
    mounted_=mounted;
    if(!mounted_)return;
    // Restore a clip if power died after moving the old slot to backup.
    // The last slot recorded in /sound.slot identifies that old path.
    if(LittleFS.exists(kBackup)){
      File f=LittleFS.open("/sound.slot","r");
      int slot=f?f.read():-1;
      f.close();
      if(slot>=1 && slot<=S3WavPolicy::kSlots){
        const char *target=path(uint8_t(slot));
        if(!LittleFS.exists(target))
          LittleFS.rename(kBackup,target);
        else LittleFS.remove(kBackup);
      }else{
        // Unknown backup identity: leave it intact for manual recovery.
        Serial.println("[S3 WAV] unknown backup identity; preserved");
      }
    }
    // Never retain partial upload bytes across reboot.
    LittleFS.remove(kTemp);
    if(!LittleFS.exists(kBackup))LittleFS.remove("/sound.slot");
  }
  bool mounted()const{return mounted_;}
  static const char* path(uint8_t slot){
    switch(slot){
      case 1:return "/sound1.wav";
      case 2:return "/sound2.wav";
      case 3:return "/sound3.wav";
      default:return "";
    }
  }
  bool inspect(uint8_t slot,S3WavPolicy::Info &info){
    info=S3WavPolicy::Info{};
    if(!mounted_ || !S3WavPolicy::validSlot(slot))return false;
    File f=LittleFS.open(path(slot),"r");
    if(!f || f.isDirectory())return false;
    bool ok=S3WavPolicy::parse(f,info);
    f.close();
    return ok;
  }
  bool available(uint8_t slot){
    S3WavPolicy::Info m;
    return inspect(slot,m);
  }
  size_t bytes(uint8_t slot){
    if(!mounted_||!S3WavPolicy::validSlot(slot))return 0;
    File f=LittleFS.open(path(slot),"r");
    const size_t n=f?f.size():0;
    f.close();
    return n;
  }
  // Caller validates /sound.tmp in full and ensures audio is idle.
  bool install(uint8_t slot){
    if(!mounted_ || !S3WavPolicy::validSlot(slot))return false;
    File f=LittleFS.open(kTemp,"r");
    S3WavPolicy::Info m;
    bool ok=f && S3WavPolicy::parse(f,m);
    f.close();
    if(!ok)return false;
    // Don't overwrite a currently unrecovered backup from another slot.
    if(LittleFS.exists(kBackup) || LittleFS.exists("/sound.slot"))
      return false;
    const char *target=path(slot);
    const bool hadOld=LittleFS.exists(target);
    if(hadOld){
      File marker=LittleFS.open("/sound.slot","w");
      if(!marker || marker.write(slot)!=1){
        marker.close();LittleFS.remove("/sound.slot");return false;
      }
      marker.close();
      if(!LittleFS.rename(target,kBackup)){
        LittleFS.remove("/sound.slot");return false;
      }
    }
    if(!LittleFS.rename(kTemp,target)){
      if(hadOld)LittleFS.rename(kBackup,target);
      if(!LittleFS.exists(kBackup))LittleFS.remove("/sound.slot");
      return false;
    }
    if(hadOld)LittleFS.remove(kBackup);
    LittleFS.remove("/sound.slot");
    return true;
  }
  bool remove(uint8_t slot){
    if(!mounted_ || !S3WavPolicy::validSlot(slot))return false;
    if(!LittleFS.exists(path(slot)))return true;
    return LittleFS.remove(path(slot));
  }
  size_t totalBytes()const{return mounted_?LittleFS.totalBytes():0;}
  size_t usedBytes()const{return mounted_?LittleFS.usedBytes():0;}
 private:
  bool mounted_=false;
};
#endif
