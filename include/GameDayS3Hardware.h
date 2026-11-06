#pragma once
// Waveshare ESP32-S3-Touch-LCD-3.5 (ST7796 SPI, FT6336 I2C).
// Full native 480x320 landscape UI, with factory-mapped FT6336 touch.
// Uploaded 320x240 GOAL GIFs are centered 1:1, not stretched.
// No rear-RGB or speaker pins are claimed: Waveshare uses different hardware.
#ifdef GLOWDECK_WAVESHARE_S3
#include <Arduino.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include <esp_heap_caps.h>
#include "S3RasterGeometry.h"

namespace GameDayS3 {
constexpr uint8_t SDA_PIN = 8;
constexpr uint8_t SCL_PIN = 7;
constexpr uint8_t TOUCH_ADDRESS = 0x38;
constexpr uint8_t EXPANDER_ADDRESS = 0x20;
constexpr uint16_t PHYSICAL_W = 480, PHYSICAL_H = 320;
constexpr uint16_t VIRTUAL_W = 320, VIRTUAL_H = 240;

inline bool readExpander(uint8_t reg, uint8_t &value) {
  Wire.beginTransmission(EXPANDER_ADDRESS);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(EXPANDER_ADDRESS, (uint8_t)1) != 1) return false;
  value = Wire.read();
  return true;
}
inline bool writeExpander(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(EXPANDER_ADDRESS);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}
// Reference: Waveshare official Arduino example 08_gfx_helloworld.
// TCA9554 P1 is LCD reset. Preserve the other expander bits.
inline bool beginHardware() {
  Wire.begin(SDA_PIN, SCL_PIN, 400000);
  uint8_t direction = 0xFF, output = 0xFF;
  if (!readExpander(0x03, direction) || !readExpander(0x01, output)) {
    Serial.println("[S3] TCA9554 missing; LCD reset not available");
    return false;
  }
  if (!writeExpander(0x03, direction & ~(1u << 1))) return false;
  writeExpander(0x01, output | (1u << 1));
  delay(10);
  writeExpander(0x01, output & ~(1u << 1));
  delay(10);
  writeExpander(0x01, output | (1u << 1));
  delay(200);
  return true;
}
} // namespace GameDayS3

// The shared GameDay renderer uses one native 480x320 RGB565 TFT_eSprite.
// Every S3 screen presents the same canvas directly to the ST7796.
class GameDayS3Display : public TFT_eSprite {
 public:
  GameDayS3Display() : TFT_eSprite(&panel_) {}
  bool init() {
    panel_.init();
    panel_.setRotation(1);  // 480x320 landscape
    panel_.fillScreen(TFT_BLACK);
    setColorDepth(16);
    if (!createSprite(GameDayS3::PHYSICAL_W, GameDayS3::PHYSICAL_H)) {
      Serial.println("[S3] ERROR: couldn't allocate native 480x320 canvas");
      return false;
    }
    Serial.printf("[S3] Native 480x320 RGB565 canvas ready, free PSRAM: %u bytes\\n",
                  (unsigned)ESP.getFreePsram());
    // Phase 4 removes the additional 480x320 scaling buffer (~300 KiB).
    // One native RGB565 sprite is sufficient for all S3 screens.
    return true;
  }
  // NOTE: TFT_eSprite inherits several non-virtual TFT_eSPI methods that
  // write straight to the LCD. fillScreen(), fillRoundRect(), drawRoundRect(),
  // fillCircle() and fillTriangle() are NOT sprite primitives. When inherited
  // methods run, the physical panel changes but the off-screen framebuffer
  // remains stale; the next pushSprite() resurrects pixels from the last page.
  // Explicitly shadow these methods on our S3 display type, and draw every
  // pixel into the same native sprite. CYD does not use this subclass.
  void fillScreen(uint32_t color) { fillSprite(color); }
  void fillRoundRect(int32_t x,int32_t y,int32_t w,int32_t h,
                     int32_t r,uint32_t color) {
    if(w<=0 || h<=0)return;
    r=S3Raster::roundedRadius(w,h,r);
    if(!r){TFT_eSprite::fillRect(x,y,w,h,color);return;}
    for(int32_t iy=0;iy<r;++iy){
      const int32_t inset=S3Raster::cornerInset(r,iy);
      TFT_eSprite::drawFastHLine(x+inset,y+iy,w-2*inset,color);
      TFT_eSprite::drawFastHLine(x+inset,y+h-1-iy,w-2*inset,color);
    }
    if(h>2*r)TFT_eSprite::fillRect(x,y+r,w,h-2*r,color);
  }
  void drawRoundRect(int32_t x,int32_t y,int32_t w,int32_t h,
                     int32_t r,uint32_t color) {
    if(w<=0 || h<=0)return;
    r=S3Raster::roundedRadius(w,h,r);
    if(!r) {
      TFT_eSprite::drawFastHLine(x,y,w,color);
      TFT_eSprite::drawFastHLine(x,y+h-1,w,color);
      TFT_eSprite::drawFastVLine(x,y,h,color);
      TFT_eSprite::drawFastVLine(x+w-1,y,h,color);
      return;
    }
    for(int32_t iy=0;iy<r;++iy) {
      const int32_t inset=S3Raster::cornerInset(r,iy);
      if(iy==0){
        TFT_eSprite::drawFastHLine(x+inset,y,w-2*inset,color);
        TFT_eSprite::drawFastHLine(x+inset,y+h-1,w-2*inset,color);
      }else{
        TFT_eSprite::drawPixel(x+inset,y+iy,color);
        TFT_eSprite::drawPixel(x+w-1-inset,y+iy,color);
        TFT_eSprite::drawPixel(x+inset,y+h-1-iy,color);
        TFT_eSprite::drawPixel(x+w-1-inset,y+h-1-iy,color);
      }
    }
    if(h>2*r) {
      TFT_eSprite::drawFastVLine(x,y+r,h-2*r,color);
      TFT_eSprite::drawFastVLine(x+w-1,y+r,h-2*r,color);
    }
  }
  void fillCircle(int32_t x,int32_t y,int32_t r,uint32_t color) {
    if(r<0)return;
    for(int32_t dy=-r;dy<=r;++dy){
      int32_t dx=r;
      while(dx>0 && dx*dx+dy*dy>r*r)--dx;
      TFT_eSprite::drawFastHLine(x-dx,y+dy,2*dx+1,color);
    }
  }
  void fillTriangle(int32_t x0,int32_t y0,int32_t x1,int32_t y1,
                    int32_t x2,int32_t y2,uint32_t color) {
    // Filled follow-star wedges are small (< 25px per side). Integer
    // barycentric scan conversion avoids LCD-only inherited fillTriangle().
    const int32_t minX=max(0,min(x0,min(x1,x2)));
    const int32_t maxX=min(int32_t(GameDayS3::PHYSICAL_W-1),
                           max(x0,max(x1,x2)));
    const int32_t minY=max(0,min(y0,min(y1,y2)));
    const int32_t maxY=min(int32_t(GameDayS3::PHYSICAL_H-1),
                           max(y0,max(y1,y2)));
    const int32_t orientation=(x1-x0)*(y2-y0)-(y1-y0)*(x2-x0);
    if(!orientation)return;
    for(int32_t y=minY;y<=maxY;++y)for(int32_t x=minX;x<=maxX;++x) {
      const int32_t a=(x1-x0)*(y-y0)-(y1-y0)*(x-x0);
      const int32_t b=(x2-x1)*(y-y1)-(y2-y1)*(x-x1);
      const int32_t c=(x0-x2)*(y-y2)-(y0-y2)*(x-x2);
      if((orientation>0 && a>=0 && b>=0 && c>=0) ||
         (orientation<0 && a<=0 && b<=0 && c<=0))
        TFT_eSprite::drawPixel(x,y,color);
    }
  }
  // The original TFT_eSPI draw commands don't set sprite rotation here.
  void setRotation(uint8_t) {}
  void invertDisplay(bool enabled) { panel_.invertDisplay(enabled); }
  void setNativeLayout(bool enabled) { nativeLayout_ = enabled; }
  bool nativeLayout() const { return nativeLayout_; }
  void present() { pushSprite(0,0); }
  bool presentRegion(int32_t x,int32_t y,int32_t w,int32_t h) {
    S3Raster::Rect region=S3Raster::cropToPanel({x,y,w,h});
    if(!S3Raster::area(region))return false;
    // TFT_eSprite crops its own source. The destination must be the same
    // origin to preserve exact 480x320 layout and existing touch targets.
    return pushSprite(region.x,region.y,region.x,region.y,region.w,region.h);
  }
 private:
  TFT_eSPI panel_;
  bool nativeLayout_ = true;
};

// Data shape compatible with the legacy XPT2046_Touchscreen usage.
struct TS_Point {
  int16_t x = 0, y = 0, z = 0;
  TS_Point() = default;
  TS_Point(int16_t x_, int16_t y_, int16_t z_ = 1) : x(x_), y(y_), z(z_) {}
};

class GameDayS3Touch {
 public:
  bool begin() {
    Wire.beginTransmission(GameDayS3::TOUCH_ADDRESS);
    bool ok = Wire.endTransmission() == 0;
    Serial.printf("[S3] FT6X36 touch %s\n", ok ? "detected" : "not detected");
    return ok;
  }
  void setRotation(uint8_t) {}
  bool touched() {
    const uint32_t now = millis();
    if (now - readAt_ < 22) return isDown_;
    readAt_ = now;
    Wire.beginTransmission(GameDayS3::TOUCH_ADDRESS);
    Wire.write(0x02); // TD_STATUS + first contact X/Y registers
    if (Wire.endTransmission(false) != 0 ||
        Wire.requestFrom(GameDayS3::TOUCH_ADDRESS, (uint8_t)5) != 5) {
      isDown_ = false;
      return false;
    }
    uint8_t count = Wire.read() & 0x0F;
    uint8_t xh = Wire.read(), xl = Wire.read();
    uint8_t yh = Wire.read(), yl = Wire.read();
    isDown_ = (count >= 1 && count <= 2);
    if (isDown_) {
      int16_t nativeX = ((xh & 0x0F) << 8) | xl;
      int16_t nativeY = ((yh & 0x0F) << 8) | yl;
      nativeX = constrain(nativeX, 0, 319);
      nativeY = constrain(nativeY, 0, 479);
      // Real Waveshare 3.5-inch hardware reports FT6336 coordinates
      // 180 degrees opposite the ST7796's landscape rotation 1.
      // Keep the correctly oriented display unchanged and invert BOTH
      // touch axes here; one-axis inversion would mirror the UI instead.
      // Scale into the legacy CYD calibration range (300..3800), which
      // keeps game stars, carousel swipes, and menu hit targets aligned.
      point_.x = 300 + (nativeY * 3500L / 479L);
      point_.y = 300 + ((319L - nativeX) * 3500L / 319L);
      point_.z = 1;
    }
    return isDown_;
  }
  TS_Point getPoint() { return point_; }
  // Hardware-oriented pixel coordinates for the native 480x320 S3 UI.
  // Independent from the legacy CYD calibration stored in NVS.
  TS_Point getScreenPoint() const {
    return TS_Point((point_.x - 300L) * 479L / 3500L,
                    (point_.y - 300L) * 319L / 3500L, point_.z);
  }
 private:
  uint32_t readAt_ = 0;
  bool isDown_ = false;
  TS_Point point_;
};
#endif
