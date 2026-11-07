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
  enum class Failure:uint8_t {
    NONE=0, WAV_OPEN, WAV_HEADER, WAV_READ, WAV_DECODE, I2S_WRITE
  };
  static const char *failureName(Failure failure){
    switch(failure){
      case Failure::WAV_OPEN:return "WAV file open failed";
      case Failure::WAV_HEADER:return "WAV header invalid";
      case Failure::WAV_READ:return "WAV flash read failed";
      case Failure::WAV_DECODE:return "WAV decoder stopped";
      case Failure::I2S_WRITE:return "I2S DMA write failed";
      default:return "None";
    }
  }
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
    // 12 x 256 frames = 64 ms of DMA elasticity during HTTPS/WLED work.
    config.dma_buf_count=12;
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
    if(xTaskCreatePinnedToCore(taskEntry,"s3_goal_audio",6144,this,2,
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
      if(valid)return playClip(Clip::CUSTOM,chosen,true);
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
  uint32_t expectedFrames()const{return expectedFrames_.load();}
  uint32_t peakSample()const{return peakSample_.load();}
  uint32_t writeRetries()const{return writeRetries_.load();}
  uint32_t readFailureOffset()const{return readFailureOffset_.load();}
  int32_t lastI2sCode()const{return lastI2sCode_.load();}
  Failure lastFailure()const{return Failure(lastFailure_.load());}
  bool usedFallback()const{return usedFallback_.load();}
  bool ampVerifiedAtStart()const{return ampVerifiedAtStart_.load();}
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
  // Distinguish expected overlap/cooldown refusals from a codec/PA failure.
  // Only the main loop reads this; lastStart_ is not written by the DMA task.
  bool busyOrCoolingDown()const{
    return playing_.load() || storageBusy_.load() ||
       (everPlayed_ && uint32_t(millis()-lastStart_)<
        S3HornPolicy::kRetriggerMs);
  }
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
      case Error::STREAM:return failureName(lastFailure());
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
  std::atomic<uint32_t> expectedFrames_{0};
  std::atomic<uint32_t> peakSample_{0};
  std::atomic<uint32_t> writeRetries_{0};
  std::atomic<uint32_t> readFailureOffset_{0};
  std::atomic<int32_t> lastI2sCode_{0};
  std::atomic<uint8_t> lastFailure_{uint8_t(Failure::NONE)};
  std::atomic<bool> usedFallback_{false};
  std::atomic<bool> ampVerifiedAtStart_{false};
  std::atomic<bool> allowFallback_{false};
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
  bool playClip(Clip clip,uint8_t source=0,bool allowFallback=false) {
    tick();
    const uint32_t now=millis();
    if(storageBusy_.load() ||
       !S3HornPolicy::canPlay(ready_,playing_.load(),now,lastStart_,
                             everPlayed_))return false;
    if(clip==Clip::CUSTOM &&
       (!storageMounted_ || !S3WavPolicy::validSlot(source)))return false;
    if(!setAmp(true)){
      ampVerifiedAtStart_.store(false);
      error_=Error::EXPANDER;
      return logFailure();
    }
    error_=Error::NONE;
    ampVerifiedAtStart_.store(true); // TCA9554 P7 latch readback verified.
    allowFallback_.store(allowFallback);
    lastFailure_.store(uint8_t(Failure::NONE));
    usedFallback_.store(false);
    expectedFrames_.store(0);
    peakSample_.store(0);
    writeRetries_.store(0);
    readFailureOffset_.store(0);
    lastI2sCode_.store(0);
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
  // Writes can be partial under network/display load. Resume at the exact
  // remaining sample without duplicating PCM; cap stalls to prevent deadlocks.
  static bool writeAudioBlock(const int16_t *pcm,size_t total,
                              uint32_t frame,uint32_t &retryCount,
                              int32_t &i2sCode){
    const uint8_t *data=reinterpret_cast<const uint8_t*>(pcm);
    size_t offset=0;
    uint8_t stalled=0;
    while(offset<total){
      size_t written=0;
      const esp_err_t result=i2s_write(I2S_NUM_0,data+offset,
                       total-offset,&written,pdMS_TO_TICKS(220));
      if(written>total-offset || (written%4)!=0){
        i2sCode=int32_t(result);
        Serial.printf("[S3 Audio] I2S invalid byte count: %u/%u at frame=%lu\n",
                      unsigned(written),unsigned(total-offset),
                      (unsigned long)frame);
        return false;
      }
      offset+=written;
      if(result==ESP_OK && written>0){
        stalled=0;
        continue;
      }
      ++retryCount;
      i2sCode=int32_t(result);
      if(written)stalled=0;
      else if(++stalled>=3){
        Serial.printf("[S3 Audio] I2S write failed: code=%ld bytes=%u/%u frame=%lu retries=%lu\n",
                      (long)i2sCode,unsigned(offset),unsigned(total),
                      (unsigned long)frame,(unsigned long)retryCount);
        return false;
      }
      vTaskDelay(pdMS_TO_TICKS(2));
    }
    return true;
  }
  static void taskEntry(void *arg){
    auto *self=static_cast<S3GoalAudio*>(arg);
    for(;;){
      ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
      const uint32_t started=millis();
      const bool test=self->activeClip_.load()==uint8_t(Clip::TEST_TONE);
      const bool initiallyCustom=self->activeClip_.load()==uint8_t(Clip::CUSTOM);
      const uint8_t slot=self->activeSource_.load();
      bool fallback=false;
      bool ok=false;
      uint32_t cursor=0,frames=0,retries=0,peak=0,readOffset=0;
      int32_t i2sCode=0;
      Failure failure=Failure::NONE;
      // Let the verified TCA9554 amplifier latch/analog path settle before
      // the first samples. No Wire/I2C access from this task.
      vTaskDelay(pdMS_TO_TICKS(15));
      for(uint8_t pass=0;pass<2;++pass){
        const bool custom=initiallyCustom && !fallback;
        cursor=0;frames=test?S3HornPolicy::kTestFrames:S3HornPolicy::kFrames;
        ok=true;
        File wav;
        S3WavPolicy::Info meta;
        S3WavPolicy::Stream<File> decoder;
        Failure passFailure=Failure::NONE;
        if(custom){
          wav=LittleFS.open(S3SoundStorage::path(slot),"r");
          if(!wav)passFailure=Failure::WAV_OPEN;
          else if(!S3WavPolicy::parse(wav,meta))passFailure=Failure::WAV_HEADER;
          else if(!decoder.begin(wav,meta)){
            passFailure=decoder.readFailed()?Failure::WAV_READ:Failure::WAV_DECODE;
            readOffset=decoder.readFailureOffset();
          }else frames=meta.outputFrames;
          if(passFailure!=Failure::NONE)ok=false;
        }
        self->expectedFrames_.store(frames);
        int16_t stereo[S3HornPolicy::kFrameChunk*2];
        while(ok && cursor<frames){
          const uint32_t remaining=frames-cursor;
          const uint16_t count=remaining<S3HornPolicy::kFrameChunk?
                                  uint16_t(remaining):S3HornPolicy::kFrameChunk;
          for(uint16_t i=0;i<count;++i){
            int16_t mono=0;
            if(custom){
              if(!decoder.next(mono)){
                passFailure=decoder.readFailed()?Failure::WAV_READ:
                                                  Failure::WAV_DECODE;
                readOffset=decoder.readFailureOffset();
                ok=false;break;
              }
            }else mono=test?S3HornPolicy::diagnosticSample(cursor+i):
                             S3HornPolicy::sample(cursor+i);
            const int32_t magnitude=mono<0?-int32_t(mono):int32_t(mono);
            if(uint32_t(magnitude)>peak)peak=uint32_t(magnitude);
            stereo[2*i]=mono;
            stereo[2*i+1]=mono;
          }
          if(!ok)break;
          if(!writeAudioBlock(stereo,size_t(count)*4,cursor,retries,i2sCode)){
            passFailure=Failure::I2S_WRITE;
            ok=false;break;
          }
          cursor+=count;
          self->writtenFrames_.store(cursor);
        }
        wav.close();
        if(ok && cursor==frames)break;
        if(passFailure==Failure::NONE)passFailure=Failure::WAV_DECODE;
        failure=passFailure;
        if(custom && self->allowFallback_.load() &&
           failure!=Failure::I2S_WRITE && pass==0){
          // A live GOAL must still sound if LittleFS becomes temporarily
          // unreadable. PP/start/final clips never impersonate a goal horn.
          fallback=true;
          self->usedFallback_.store(true);
          Serial.printf("[S3 Audio] GOAL WAV slot %u failed: %s at offset=%lu; built-in fallback\n",
                        unsigned(slot),failureName(failure),
                        (unsigned long)readOffset);
          i2s_zero_dma_buffer(I2S_NUM_0);
          continue;
        }
        break;
      }
      // The DMA queue may contain the final 64 ms; don't shut the PA off
      // before those samples leave the ES8311 output.
      vTaskDelay(pdMS_TO_TICKS(85));
      i2s_zero_dma_buffer(I2S_NUM_0);
      const bool complete=ok && cursor==frames;
      self->writtenFrames_.store(cursor);
      self->expectedFrames_.store(frames);
      self->lastDurationMs_.store(millis()-started);
      self->lastStreamOk_.store(complete);
      self->lastFailure_.store(uint8_t(failure));
      self->peakSample_.store(peak);
      self->writeRetries_.store(retries);
      self->lastI2sCode_.store(i2sCode);
      self->readFailureOffset_.store(readOffset);
      if(!complete)self->streamError_.store(true);
      Serial.printf("[S3 Audio] %s stream %s source=%u frames=%lu/%lu elapsed=%lums"
                    " peak=%lu retries=%lu i2s=%ld failure=%s readOffset=%lu fallback=%u PAstart=%u\n",
                    initiallyCustom?"custom-wav":test?"test-tone":"goal-horn",
                    complete?"complete":"FAILED",unsigned(slot),
                    (unsigned long)cursor,(unsigned long)frames,
                    (unsigned long)self->lastDurationMs_.load(),
                    (unsigned long)peak,(unsigned long)retries,
                    (long)i2sCode,failureName(failure),
                    (unsigned long)readOffset,unsigned(fallback),
                    unsigned(self->ampVerifiedAtStart_.load()));
      self->playing_.store(false);
    }
  }
};
#endif
