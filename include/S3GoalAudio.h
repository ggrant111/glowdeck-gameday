#pragma once
// Phase 7A: Waveshare ESP32-S3-Touch-LCD-3.5 ST7796 only.
//
// Speaker output: ES8311 DAC at I2C 0x18, shared SDA=8/SCL=7, I2S
// MCLK=12, BCLK=13, LRCK=15, DOUT=16, TCA9554 (0x20) P7 PA enable.
// IMPORTANT: GPIO26 belongs only to the original CYD. This is NOT the
// Waveshare QSPI 3.5B. Pins and codec sequence from the Waveshare official
// Arduino 01_audio_out/04_es8311 examples and Espressif's ES8311 driver
// (Apache-2.0); minimalist 48 kHz, 16-bit mono-to-stereo playback only.
//
// Task owns I2S audio DMA independently of slow NHL HTTPS/WLED traffic;
// loopTask exclusively owns I2C + amp switching, so FT6336 touch is safe.
// No audio task, codec init or expander changes in a CYD build.
#ifdef GLOWDECK_WAVESHARE_S3
#include <Arduino.h>
#include <Wire.h>
#include <driver/i2s.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <atomic>
#include "GameDayS3Hardware.h"
#include "S3HornPolicy.h"
#include "S3WavPolicy.h"
#include "S3SoundStorage.h"

class S3GoalAudio {
 public:
  enum class Clip:uint8_t { HORN=0, TEST_TONE=1, CUSTOM=2 };
  struct Probe {
    bool codecRead=false, expanderRead=false;
    uint8_t codecReset=0, codecMute=0, codecVolume=0;
    uint8_t codecId1=0, codecId2=0;
    uint8_t ampOutput=0, ampDirection=0;
  };
  bool begin() {
    // beginHardware() already initialized Wire on GPIO8/GPIO7.
    // The PA is P7 (active high); never toggle P1 LCD reset.
    uint8_t dir=0xFF, out=0xFF;
    if(!GameDayS3::readExpander(0x03,dir) ||
       !GameDayS3::readExpander(0x01,out)) {
      error_=Error::EXPANDER; return logFailure();
    }
    if(!GameDayS3::writeExpander(0x01,out & ~0x80U) ||
       !GameDayS3::writeExpander(0x03,dir & ~0x80U)) {
      error_=Error::EXPANDER; return logFailure();
    }
    ampOn_=false;
    Wire.beginTransmission(kCodecAddress);
    if(Wire.endTransmission()!=0){
      error_=Error::CODEC_MISSING;return logFailure();
    }

    i2s_config_t config={};
    config.mode=static_cast<i2s_mode_t>(I2S_MODE_MASTER|I2S_MODE_TX);
    config.sample_rate=S3HornPolicy::kSampleRate;
    config.bits_per_sample=I2S_BITS_PER_SAMPLE_16BIT;
    config.channel_format=I2S_CHANNEL_FMT_RIGHT_LEFT;
    config.communication_format=I2S_COMM_FORMAT_STAND_I2S;
    config.intr_alloc_flags=0;
    config.dma_buf_count=6;
    config.dma_buf_len=S3HornPolicy::kFrameChunk;
    config.use_apll=false;
    config.tx_desc_auto_clear=true;
    config.fixed_mclk=0;
    config.mclk_multiple=I2S_MCLK_MULTIPLE_256;
    if(i2s_driver_install(I2S_NUM_0,&config,0,nullptr)!=ESP_OK){
      error_=Error::I2S;return logFailure();
    }
    i2s_pin_config_t pins={};
    pins.mck_io_num=12;
    pins.bck_io_num=13;
    pins.ws_io_num=15;
    pins.data_out_num=16;
    pins.data_in_num=I2S_PIN_NO_CHANGE;
    if(i2s_set_pin(I2S_NUM_0,&pins)!=ESP_OK ||
       i2s_set_clk(I2S_NUM_0,S3HornPolicy::kSampleRate,
                   I2S_BITS_PER_SAMPLE_16BIT,I2S_CHANNEL_STEREO)!=ESP_OK) {
      error_=Error::I2S;
      i2s_driver_uninstall(I2S_NUM_0);
      return logFailure();
    }
    i2s_zero_dma_buffer(I2S_NUM_0);
    if(!configureCodec()){
      error_=Error::CODEC_INIT;
      i2s_driver_uninstall(I2S_NUM_0);
      return logFailure();
    }
    // No speaker pop at boot; start at 70% reference gain, PA remains OFF.
    if(xTaskCreatePinnedToCore(taskEntry,"s3_goal_audio",4096,this,2,
                              &task_,1)!=pdPASS){
      error_=Error::TASK;
      i2s_driver_uninstall(I2S_NUM_0);
      return logFailure();
    }
    ready_=true;
    error_=Error::NONE;
    Serial.printf("[S3 Audio] ES8311 ready, 48kHz/16-bit I2S, codec volume %u%%, PA idle\n",
                  unsigned(volumePercent_));
    return true;
  }

  // Real NHL celebrations use the persisted selected sound. If a custom
  // file is missing/corrupted, fall back to the already-tested built-in
  // horn. Previews can explicitly audition any slot, without changing it.
  bool play(){
    const uint8_t chosen=selectedSource_;
    if(chosen && storageMounted_){
      File file=LittleFS.open(S3SoundStorage::path(chosen),"r");
      S3WavPolicy::Info meta;
      const bool valid=file && S3WavPolicy::parse(file,meta);
      file.close();
      if(valid)return playClip(Clip::CUSTOM,chosen);
      Serial.println("[S3 WAV] selected file missing/invalid; using built-in goal horn");
    }
    return playClip(Clip::HORN);
  }
  bool playSource(uint8_t source){
    if(source==0)return playClip(Clip::HORN);
    if(!storageMounted_ || !S3WavPolicy::validSlot(source) ||
       storageBusy_.load())return false;
    File f=LittleFS.open(S3SoundStorage::path(source),"r");
    S3WavPolicy::Info meta;
    const bool valid=f && S3WavPolicy::parse(f,meta);
    f.close();
    return valid && playClip(Clip::CUSTOM,source);
  }
  bool testTone(){return playClip(Clip::TEST_TONE);}
  void setStorageMounted(bool mounted){storageMounted_=mounted;}
  bool storageMounted()const{return storageMounted_;}
  void setStorageBusy(bool busy){storageBusy_.store(busy);}
  bool storageBusy()const{return storageBusy_.load();}
  void setSelectedSource(uint8_t source){selectedSource_=source;}
  uint8_t selectedSource()const{return selectedSource_;}
  Probe probe(){
    Probe p;
    p.codecRead=readCodec(0x00,p.codecReset) &&
                readCodec(0x31,p.codecMute) &&
                readCodec(0x32,p.codecVolume) &&
                readCodec(0xFD,p.codecId1) &&
                readCodec(0xFE,p.codecId2);
    p.expanderRead=GameDayS3::readExpander(0x03,p.ampDirection) &&
                   GameDayS3::readExpander(0x01,p.ampOutput);
    return p;
  }
  uint32_t writtenFrames() const {return writtenFrames_.load();}
  uint32_t lastDurationMs() const {return lastDurationMs_.load();}
  bool lastStreamOk()const{return lastStreamOk_.load();}
  bool hasPlayed()const{return everPlayed_;}
  bool ampEnabled()const{return ampOn_;}
  const char *clipName()const{
    return activeClip_.load()==uint8_t(Clip::TEST_TONE)?"test-tone":
           activeClip_.load()==uint8_t(Clip::CUSTOM)?"custom-wav":"goal-horn";
  }

  // Call from main loop only: never access Wire from the audio DMA task.
  void tick(){
    if(ampOn_ && !playing_.load()){
      if(!setAmp(false))error_=Error::EXPANDER;
      if(streamError_.exchange(false))error_=Error::STREAM;
      Serial.printf("[S3 Audio] horn finished | PA=%s | %s\n",
                    ampOn_?"ON":"OFF",error());
    }
  }
  bool ready()const{return ready_;}
  bool playing()const{return playing_.load();}
  uint8_t volume()const{return volumePercent_;}
  // Only loopTask touches Wire; never update ES8311 gain while audio DMA
  // is active. Validate I2C register readback and restore last gain on
  // failure to avoid persisting misleading values. The PA stays off.
  bool setVolume(uint8_t percent){
    if(!ready_ || playing_.load() || !S3HornPolicy::validVolume(percent))
      return false;
    if(percent==volumePercent_)return true;
    const uint8_t next=S3HornPolicy::codecVolumeRegister(percent);
    const uint8_t old=S3HornPolicy::codecVolumeRegister(volumePercent_);
    uint8_t readback=0;
    if(!writeCodec(0x32,next) || !readCodec(0x32,readback) ||
       readback!=next){
      writeCodec(0x32,old);
      error_=Error::CODEC_VOLUME;
      Serial.printf("[S3 Audio] Volume set failed: requested %u%%, readback 0x%02X\n",
                    unsigned(percent),unsigned(readback));
      return false;
    }
    volumePercent_=percent;
    error_=Error::NONE;
    Serial.printf("[S3 Audio] Volume updated to %u%%, reg32=0x%02X\n",
                  unsigned(volumePercent_),unsigned(next));
    return true;
  }
  const char* error()const{
    switch(error_){
      case Error::EXPANDER:return "PA expander unavailable";
      case Error::CODEC_MISSING:return "ES8311 not found at 0x18";
      case Error::CODEC_INIT:return "ES8311 register initialization failed";
      case Error::I2S:return "I2S clock/pins unavailable";
      case Error::TASK:return "Audio task allocation failed";
      case Error::STREAM:return "I2S write timeout";
      case Error::CODEC_VOLUME:return "ES8311 volume readback failed";
      default:return "";
    }
  }

 private:
  static constexpr uint8_t kCodecAddress=0x18;
  enum class Error:uint8_t {
    NONE,EXPANDER,CODEC_MISSING,CODEC_INIT,I2S,TASK,STREAM,CODEC_VOLUME
  };
  bool ready_=false,ampOn_=false,everPlayed_=false;
  uint8_t volumePercent_=S3HornPolicy::kVolumePercent;
  bool storageMounted_=false;
  std::atomic<bool> storageBusy_{false};
  uint8_t selectedSource_=0;
  uint32_t lastStart_=0;
  std::atomic<bool> playing_{false};
  std::atomic<bool> streamError_{false};
  std::atomic<bool> lastStreamOk_{false};
  std::atomic<uint32_t> writtenFrames_{0};
  std::atomic<uint32_t> lastDurationMs_{0};
  std::atomic<uint8_t> activeClip_{uint8_t(Clip::HORN)};
  std::atomic<uint8_t> activeSource_{0};
  Error error_=Error::NONE;
  TaskHandle_t task_=nullptr;

  bool logFailure(){
    Serial.printf("[S3 Audio] unavailable: %s\n",error());
    return false;
  }
  bool readCodec(uint8_t reg,uint8_t &val){
    Wire.beginTransmission(kCodecAddress);
    Wire.write(reg);
    if(Wire.endTransmission(false)!=0 ||
       Wire.requestFrom(kCodecAddress,uint8_t(1))!=1)return false;
    val=Wire.read();
    return true;
  }
  bool playClip(Clip clip,uint8_t source=0) {
    tick();
    const uint32_t now=millis();
    if(storageBusy_.load() ||
       !S3HornPolicy::canPlay(ready_,playing_.load(),now,lastStart_,
                             everPlayed_))return false;
    if(clip==Clip::CUSTOM &&
       (!storageMounted_ || !S3WavPolicy::validSlot(source)))return false;
    if(!setAmp(true)){
      error_=Error::EXPANDER;
      return logFailure();
    }
    error_=Error::NONE;
    lastStart_=now;
    everPlayed_=true;
    streamError_.store(false);
    lastStreamOk_.store(false);
    writtenFrames_.store(0);
    lastDurationMs_.store(0);
    activeSource_.store(clip==Clip::CUSTOM?source:0);
    activeClip_.store(uint8_t(clip));
    playing_.store(true);
    xTaskNotifyGive(task_);
    Serial.printf("[S3 Audio] %s started (source=%u codec %u%%)\n",
                  clip==Clip::TEST_TONE?"1kHz test tone":
                  clip==Clip::CUSTOM?"custom WAV":"goal horn",
                  unsigned(source),unsigned(volumePercent_));
    return true;
  }
  bool writeCodec(uint8_t reg,uint8_t val){
    Wire.beginTransmission(kCodecAddress);
    Wire.write(reg);Wire.write(val);
    if(Wire.endTransmission()!=0){
      Serial.printf("[S3 Audio] ES8311 register %02X write failed\n",reg);
      return false;
    }
    return true;
  }
  bool setAmp(bool enabled){
    uint8_t out=0;
    if(!GameDayS3::readExpander(0x01,out))return false;
    const uint8_t next=enabled?(out|0x80U):(out&~0x80U);
    if(!GameDayS3::writeExpander(0x01,next))return false;
    // Read the actual TCA9554 latch back; a successful I2C ACK by itself
    // does not prove the PA-enable output reached the intended level.
    uint8_t verify=0;
    if(!GameDayS3::readExpander(0x01,verify) ||
       bool(verify&0x80U)!=enabled)return false;
    ampOn_=enabled;
    return true;
  }
  bool configureCodec(){
    // Subset of Espressif ES8311 init: 48kHz MCLK=12.288MHz
    // (256*Fs), 16-bit stereo I2S in slave mode, DAC analog powered.
    // The 48kHz table has predivider=1, multiplier=1, DAC/ADC OSR16,
    // BCLK divider=4, and LRCK divider=256.
    if(!writeCodec(0x00,0x1F))return false;
    delay(20);
    constexpr uint8_t sequence[][2]={
      {0x00,0x00},{0x00,0x80}, // reset release / power on
      {0x01,0x3F},{0x02,0x00},{0x03,0x10},{0x04,0x10},
      {0x05,0x00},{0x06,0x03},{0x07,0x00},{0x08,0xFF},
      {0x00,0x80},{0x09,0x0C},{0x0A,0x0C}, // slave + 16-bit I2S
      {0x0D,0x01},{0x0E,0x02},{0x12,0x00},{0x13,0x10},
      {0x1C,0x6A},{0x37,0x08}, // analog DAC and bypass EQ
      {0x17,0xC8},{0x14,0x1A}, // mirror official ES8311 analog-path init
      {0x31,0x00}, // DAC unmute
      {0x32,S3HornPolicy::codecVolumeRegister(S3HornPolicy::kVolumePercent)}
      // Reference volume 70%; persisted user preference applies after NVS load.
    };
    for(const auto &pair:sequence)if(!writeCodec(pair[0],pair[1]))return false;
    uint8_t volume=0,mute=0;
    if(!readCodec(0x32,volume)||!readCodec(0x31,mute))return false;
    if(volume!=S3HornPolicy::codecVolumeRegister(volumePercent_) ||
       (mute&0x60U)){
      Serial.printf("[S3 Audio] codec readback bad: vol=0x%02X mute=0x%02X\n",
                    volume,mute);
      return false;
    }
    return true;
  }
  static void taskEntry(void *arg){
    auto *self=static_cast<S3GoalAudio*>(arg);
    // This task runs at modest priority on core 1: NHL HTTPS, WebServer,
    // TFT updates, and WLED state recovery remain in their existing loop.
    for(;;){
      ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
      uint32_t cursor=0;
      bool ok=true;
      const uint32_t started=millis();
      const bool test=self->activeClip_.load()==uint8_t(Clip::TEST_TONE);
      const bool custom=self->activeClip_.load()==uint8_t(Clip::CUSTOM);
      File wav;
      S3WavPolicy::Info meta;
      S3WavPolicy::Stream<File> decoder;
      uint32_t frames=test?S3HornPolicy::kTestFrames:S3HornPolicy::kFrames;
      if(custom){
        const uint8_t slot=self->activeSource_.load();
        wav=LittleFS.open(S3SoundStorage::path(slot),"r");
        if(!wav || !S3WavPolicy::parse(wav,meta) ||
           !decoder.begin(wav,meta)){
          ok=false;
          Serial.printf("[S3 WAV] failed to stream WAV slot %u\n",
                        unsigned(slot));
        }else frames=meta.outputFrames;
      }
      int16_t stereo[S3HornPolicy::kFrameChunk*2];
      while(ok && cursor<frames){
        const uint32_t remaining=frames-cursor;
        const uint16_t count=remaining<S3HornPolicy::kFrameChunk?
                                uint16_t(remaining):S3HornPolicy::kFrameChunk;
        for(uint16_t i=0;i<count;++i){
          int16_t mono=0;
          if(custom){
            if(!decoder.next(mono)){ok=false;break;}
          }else mono=test?S3HornPolicy::diagnosticSample(cursor+i):
                           S3HornPolicy::sample(cursor+i);
          stereo[2*i]=mono;
          stereo[2*i+1]=mono;
        }
        if(!ok)break;
        size_t written=0;
        const esp_err_t result=i2s_write(I2S_NUM_0,stereo,
                              size_t(count)*4,&written,pdMS_TO_TICKS(120));
        if(result!=ESP_OK||written!=size_t(count)*4){
          ok=false;
          Serial.printf("[S3 Audio] I2S write failed: code=%d bytes=%u/%u frame=%lu\n",
                        int(result),unsigned(written),unsigned(size_t(count)*4),
                        (unsigned long)cursor);
          break;
        }
        cursor+=count;
        self->writtenFrames_.store(cursor);
      }
      wav.close();
      // Allow the last DMA block to finish before disabling the PA.
      vTaskDelay(pdMS_TO_TICKS(35));
      i2s_zero_dma_buffer(I2S_NUM_0);
      self->lastDurationMs_.store(millis()-started);
      self->lastStreamOk_.store(ok && cursor==frames);
      if(!ok)self->streamError_.store(true);
      Serial.printf("[S3 Audio] %s stream %s, frames=%lu/%lu, elapsed=%lums\n",
                    custom?"custom-wav":test?"test-tone":"goal-horn",
                    ok?"complete":"FAILED",
                    (unsigned long)cursor,(unsigned long)frames,
                    (unsigned long)self->lastDurationMs_.load());
      self->playing_.store(false);
    }
  }
};
#endif
