#pragma once
#ifdef GLOWDECK_WAVESHARE_S3
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "S3CaseLedPolicy.h"

// Waveshare ESP32-S3-Touch-LCD-3.5 (ST7796) 32-pin expansion header:
// GPIO17 = header pin 14 (HOME / LEFT)
// GPIO18 = header pin 16 (AWAY / RIGHT)
// Verified on Waveshare ST7796 header mapping; verify V1/V2 PCB marking
// and the corresponding schematic BEFORE attaching wiring.
// Never use CYD rear RGB GPIO4/17/16 mapping on this separate S3 target.
// Dedicated external 5V supply + 74AHCT125 level shifter mandatory;
// these are 3.3V logic OUTPUTS, NOT LED strip power pins.
class S3CaseLedDriver {
 public:
  static constexpr int kHomePin=17;
  static constexpr int kAwayPin=18;
  static constexpr uint8_t kPhysicalPixels=S3CaseLedPolicy::kMaxPixels;
  S3CaseLedDriver()
      :home_(kPhysicalPixels,kHomePin,NEO_GRB+NEO_KHZ800),
       away_(kPhysicalPixels,kAwayPin,NEO_GRB+NEO_KHZ800){}
  bool enable(bool state) {
    if(!state){
      if(enabled_){
        home_.clear();away_.clear();home_.show();away_.show();
        pinMode(kHomePin,INPUT);pinMode(kAwayPin,INPUT);
      }
      enabled_=false;
      return true;
    }
    if(enabled_)return true;
    // No GPIO claimed on boot. Lazy initialization only after explicit
    // web/API opt-in, following completed external 5V level-shifter wiring.
    home_.begin();away_.begin();
    home_.clear();away_.clear();
    home_.show();away_.show();
    enabled_=true;
    lastFrame_=0;
    return true;
  }
  bool enabled()const{return enabled_;}
  uint32_t lastFrame()const{return lastFrame_;}
  void tick(uint32_t now,const S3CaseLedPolicy::Controller &fx,
            uint8_t pixelCount,uint8_t brightness){
    if(!enabled_ || !S3CaseLedPolicy::validPixels(pixelCount) ||
       !S3CaseLedPolicy::validBrightness(brightness))return;
    if(lastFrame_ && uint32_t(now-lastFrame_)<
                        S3CaseLedPolicy::kFramePeriodMs)return;
    lastFrame_=now;
    // Update both independent strips using one NHL-visible snapshot.
    // RGB -> GRB is performed by Adafruit_NeoPixel transport, not by
    // modifying NHL primary colors (avoids CYD B/G swap).
    for(uint8_t i=0;i<kPhysicalPixels;++i){
      home_.setPixelColor(i,fx.pixel(S3CaseLedPolicy::Side::HOME,
                                    i,pixelCount,now,brightness));
      away_.setPixelColor(i,fx.pixel(S3CaseLedPolicy::Side::AWAY,
                                    i,pixelCount,now,brightness));
    }
    home_.show();
    away_.show();
  }
 private:
  bool enabled_=false;
  uint32_t lastFrame_=0;
  Adafruit_NeoPixel home_,away_;
};
#endif
