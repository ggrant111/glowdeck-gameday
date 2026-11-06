#pragma once
#ifdef GLOWDECK_WAVESHARE_S3
#include <Arduino.h>
#include <esp_heap_caps.h>
// Arduino-ESP32 2.0.x (ESP-IDF 4.4) keeps the S3 ROM inflater here.
#include <esp32s3/rom/miniz.h>
#include "S3ArenaEmbedded.h"
#include "S3RasterGeometry.h"

// Phase 6B: arena background from a build-time optimized PNG master.
// 192x128 RGB565 + zlib occupies ~6KB in flash; reconstructed to the
// physical 480x320 RGB565 layout just once per boot and cached in PSRAM.
// Score/clock updates restore clean pixels from the cache, then draw live
// data on the native sprite. The event queue and TFT display stay non-blocking.
class S3ArenaBackground {
 public:
  bool begin() {
    if(attempted_)return ready_;
    attempted_=true;
    const size_t rawSize=S3ArenaEmbedded::RawBytes;
    const size_t chars=sizeof(S3ArenaEmbedded::CompressedBase64)-1;
    if(chars<8 || (chars&3))return fail("invalid embedded base64 size");
    const size_t compressedCapacity=chars/4*3;
    uint8_t *compressed=(uint8_t *)heap_caps_malloc(
        compressedCapacity,MALLOC_CAP_8BIT);
    uint8_t *raw=(uint8_t *)heap_caps_malloc(
        rawSize,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    cache_=(uint16_t *)heap_caps_malloc(
        kWidth*kHeight*sizeof(uint16_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!compressed || !raw || !cache_) {
      free(compressed); free(raw);
      return fail("PSRAM/heap asset allocation failed");
    }
    size_t cSize=0;
    uint32_t accumulator=0;
    uint8_t bits=0;
    for(size_t i=0;i<chars;++i) {
      const char c=pgm_read_byte(&S3ArenaEmbedded::CompressedBase64[i]);
      if(c=='=')break;
      const int n=base64Digit(c);
      if(n<0) { free(compressed);free(raw);return fail("invalid asset base64"); }
      accumulator=(accumulator<<6)|uint8_t(n);
      bits+=6;
      if(bits>=8) {
        bits-=8;
        if(cSize>=compressedCapacity){
          free(compressed);free(raw);return fail("invalid asset capacity");
        }
        compressed[cSize++]=uint8_t(accumulator>>bits);
      }
    }
    const size_t produced=tinfl_decompress_mem_to_mem(
      raw,rawSize,compressed,cSize,
      TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
    free(compressed);
    if(produced!=rawSize) {
      free(raw);return fail("embedded arena zlib invalid");
    }
    // Source color is big-endian RGB565. The TFT_eSprite 16-bit framebuffer
    // uses byte-swapped RGB565 words, as TFT_eSprite::drawPixel() does.
    // Coordinate transforms exactly cover x=[0,479], y=[0,319].
    for(int y=0;y<kHeight;++y) {
      const uint32_t fy=uint32_t(y)*(S3ArenaEmbedded::SourceH-1)*256U/(kHeight-1);
      const int sy=fy>>8, sy2=sy+1<S3ArenaEmbedded::SourceH?sy+1:sy;
      const uint8_t wy=fy&255U;
      for(int x=0;x<kWidth;++x) {
        const uint32_t fx=uint32_t(x)*(S3ArenaEmbedded::SourceW-1)*256U/(kWidth-1);
        const int sx=fx>>8, sx2=sx+1<S3ArenaEmbedded::SourceW?sx+1:sx;
        const uint8_t wx=fx&255U;
        const uint16_t a=sourceColor(raw,sx,sy);
        const uint16_t b=sourceColor(raw,sx2,sy);
        const uint16_t c=sourceColor(raw,sx,sy2);
        const uint16_t d=sourceColor(raw,sx2,sy2);
        const uint16_t color=mix565(mix565(a,b,wx),mix565(c,d,wx),wy);
        cache_[y*kWidth+x]=uint16_t((color<<8)|(color>>8));
      }
      if((y&15)==15)yield();
    }
    free(raw);
    ready_=true;
    Serial.printf("[S3 Arena] loaded %ux%u RGB565 broadcast background, PSRAM: %u bytes\\n",
                  kWidth,kHeight,(unsigned)ESP.getFreePsram());
    return true;
  }
  bool ready() const {return ready_;}
  template<typename Sprite> bool restore(Sprite &sprite) const {
    return restoreRegion(sprite,0,0,kWidth,kHeight);
  }
  template<typename Sprite> bool restoreRegion(Sprite &sprite,
      int x,int y,int w,int h) const {
    if(!ready_||!cache_)return false;
    const auto rect=S3Raster::cropToPanel({x,y,w,h});
    if(rect.w<=0 || rect.h<=0)return false;
    auto* framebuffer=static_cast<uint16_t*>(sprite.frameBuffer(1));
    if(!framebuffer)return false;
    for(int py=rect.y;py<rect.y+rect.h;++py) {
      const size_t offset=size_t(py)*kWidth+rect.x;
      memcpy(framebuffer+offset,cache_+offset,rect.w*sizeof(uint16_t));
    }
    return true;
  }
  ~S3ArenaBackground(){free(cache_);}
 private:
  static constexpr int kWidth=480,kHeight=320;
  bool attempted_=false,ready_=false;
  uint16_t *cache_=nullptr;
  bool fail(const char *message) {
    Serial.printf("[S3 Arena] fallback to Phase 5 scoreboard: %s\\n",message);
    free(cache_);cache_=nullptr;
    return false;
  }
  static int base64Digit(char c) {
    if(c>='A'&&c<='Z')return c-'A';
    if(c>='a'&&c<='z')return c-'a'+26;
    if(c>='0'&&c<='9')return c-'0'+52;
    if(c=='+')return 62;
    if(c=='/')return 63;
    return -1;
  }
  static uint16_t sourceColor(const uint8_t *raw,int x,int y) {
    const size_t offset=2U*(size_t(y)*S3ArenaEmbedded::SourceW+x);
    return uint16_t(uint16_t(raw[offset])<<8|raw[offset+1]);
  }
  static uint16_t mix565(uint16_t a,uint16_t b,uint8_t w) {
    const uint16_t inv=256-w;
    const uint16_t r=((((a>>11)&31U)*inv+((b>>11)&31U)*w)>>8);
    const uint16_t g=((((a>>5)&63U)*inv+((b>>5)&63U)*w)>>8);
    const uint16_t blue=(((a&31U)*inv+(b&31U)*w)>>8);
    return uint16_t((r<<11)|(g<<5)|blue);
  }
};
#endif
