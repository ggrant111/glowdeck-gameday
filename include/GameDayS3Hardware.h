#pragma once
// Waveshare ESP32-S3-Touch-LCD-3.5 (ST7796 SPI, FT6336 I2C).
// Keeps the existing 320x240 GameDay renderer and touch hit targets identical
// while pixel-scaling onto the 480x320 physical landscape panel.
// No rear-RGB or speaker pins are claimed: Waveshare uses different hardware.
#ifdef GLOWDECK_WAVESHARE_S3
#include <Arduino.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include <esp_heap_caps.h>

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

// TFT_eSprite remains the shared drawable API. New S3 game screens use its
// native 480x320 canvas directly. Legacy stats, standings, setup and event
// overlays still render at 320x240 in the upper-left of the same canvas and
// are scaled only until their own native-design phases are implemented.
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
    scaled_ = static_cast<uint16_t *>(heap_caps_malloc(
      GameDayS3::PHYSICAL_W * GameDayS3::PHYSICAL_H * sizeof(uint16_t),
      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!scaled_) {
      scaled_ = static_cast<uint16_t *>(malloc(
        GameDayS3::PHYSICAL_W * GameDayS3::PHYSICAL_H * sizeof(uint16_t)));
    }
    if (!scaled_) Serial.println("[S3] ERROR: no scaled frame buffer");
    return scaled_ != nullptr;
  }
  // The original TFT_eSPI draw commands don't set sprite rotation here.
  void setRotation(uint8_t) {}
  void invertDisplay(bool enabled) { panel_.invertDisplay(enabled); }
  void setNativeLayout(bool enabled) { nativeLayout_ = enabled; }
  bool nativeLayout() const { return nativeLayout_; }
  void present() {
    if (nativeLayout_) {
      // Pass the native sprite directly to the panel: no pixel resampling,
      // extra RGB565 copies or 320x240 text enlargement.
      pushSprite(0, 0);
      return;
    }
    // Transitional compatibility for old CYD-only screens and the 320x240 GIF.
    if (!scaled_) return;
    for (uint16_t y = 0; y < GameDayS3::PHYSICAL_H; ++y) {
      const uint16_t sy = (uint32_t(y) * GameDayS3::VIRTUAL_H) /
                          GameDayS3::PHYSICAL_H;
      uint16_t *row = scaled_ + size_t(y) * GameDayS3::PHYSICAL_W;
      for (uint16_t x = 0; x < GameDayS3::PHYSICAL_W; ++x) {
        const uint16_t sx = (uint32_t(x) * GameDayS3::VIRTUAL_W) /
                            GameDayS3::PHYSICAL_W;
        row[x] = readPixel(sx, sy);
      }
    }
    panel_.pushImage(0, 0, GameDayS3::PHYSICAL_W,
                     GameDayS3::PHYSICAL_H, scaled_);
  }
 private:
  TFT_eSPI panel_;
  uint16_t *scaled_ = nullptr;
  bool nativeLayout_ = false;
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
