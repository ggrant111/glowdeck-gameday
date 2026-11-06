#pragma once
#ifdef GLOWDECK_WAVESHARE_S3
#include <Arduino.h>
#include <esp_heap_caps.h>
// Arduino-ESP32 2.0.x / ESP-IDF 4.4 Waveshare ESP32-S3 ROM inflater.
#include <esp32s3/rom/miniz.h>
#include "S3ArenaEmbedded.h"
#include "S3RasterGeometry.h"

// Phase 6B.1: lossless 480x320 RGB565BE broadcast artwork.
// The asset is compressed in flash and decoded ONCE directly into a
// PSRAM background cache. No 192x128 intermediate, upscaling or extra
// 300 KiB temporary buffer. Dynamic NHL overlays remain in the sprite.
class S3ArenaBackground {
 public:
  bool begin() {
    if (attempted_) return ready_;
    attempted_ = true;
    static_assert(S3ArenaEmbedded::SourceW == kWidth &&
                  S3ArenaEmbedded::SourceH == kHeight,
                  "Arena art must be native 480x320");
    static_assert(S3ArenaEmbedded::RawBytes == kWidth*kHeight*2,
                  "Arena asset must be 16-bit RGB565");

    const uint32_t start = millis();
    cache_ = static_cast<uint16_t *>(heap_caps_malloc(
      S3ArenaEmbedded::RawBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!cache_) return fail("insufficient PSRAM for native arena image");

    // ESP32-S3 is little-endian. The source is RGB565 big-endian, which
    // is EXACTLY the memory representation required by TFT_eSprite's
    // swapped 16-bit pixels. Decompress bytes directly into the cache.
    const size_t decoded = tinfl_decompress_mem_to_mem(
      cache_, S3ArenaEmbedded::RawBytes,
      S3ArenaEmbedded::Compressed, sizeof(S3ArenaEmbedded::Compressed),
      TINFL_FLAG_PARSE_ZLIB_HEADER |
      TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
    if (decoded != S3ArenaEmbedded::RawBytes)
      return fail("arena inflate length/checksum failed");

    // Independent end-to-end asset check, including RGB565 byte ordering.
    uint32_t crc = 0xFFFFFFFFUL;
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(cache_);
    for (size_t i=0; i<S3ArenaEmbedded::RawBytes; ++i) {
      crc ^= bytes[i];
      for (uint8_t bit=0; bit<8; ++bit)
        crc = (crc>>1) ^ ((crc&1) ? 0xEDB88320UL : 0UL);
    }
    crc ^= 0xFFFFFFFFUL;
    if (crc != S3ArenaEmbedded::RawCrc32)
      return fail("native RGB565 CRC mismatch");

    ready_=true;
    const uint32_t loadMs = millis()-start;
    const int center = (kHeight/2*kWidth+kWidth/2)*2;
    const uint16_t centerPixel = uint16_t(bytes[center]<<8)|bytes[center+1];
    Serial.printf("[S3 Arena] native %dx%d | compressed %u B | "
                  "cache %u B PSRAM | loaded %lu ms | center RGB565 #%04X | "
                  "free PSRAM %u B\n",
                  kWidth,kHeight,
                  unsigned(sizeof(S3ArenaEmbedded::Compressed)),
                  unsigned(S3ArenaEmbedded::RawBytes),
                  (unsigned long)loadMs,centerPixel,
                  unsigned(ESP.getFreePsram()));
    return true;
  }
  bool ready() const {return ready_;}

  template<typename Sprite> bool restore(Sprite &sprite) const {
    return restoreRegion(sprite,0,0,kWidth,kHeight);
  }
  template<typename Sprite>
  bool restoreRegion(Sprite &sprite,int x,int y,int w,int h) const {
    if (!ready_ || !cache_) return false;
    const S3Raster::Rect r = S3Raster::cropToPanel({x,y,w,h});
    if (r.w<=0 || r.h<=0) return false;
    auto *framebuffer = static_cast<uint16_t *>(sprite.frameBuffer(1));
    if (!framebuffer) return false;
    for(int py=r.y; py<r.y+r.h; ++py) {
      const size_t pos = size_t(py)*kWidth+r.x;
      memcpy(framebuffer+pos,cache_+pos,size_t(r.w)*sizeof(uint16_t));
    }
    return true;
  }
  ~S3ArenaBackground() {free(cache_);}

 private:
  static constexpr int kWidth=480, kHeight=320;
  bool attempted_=false, ready_=false;
  uint16_t *cache_=nullptr;
  bool fail(const char *message) {
    Serial.printf("[S3 Arena] fallback to Phase 5 scoreboard: %s\n",message);
    free(cache_);
    cache_=nullptr;
    return false;
  }
};
#endif
