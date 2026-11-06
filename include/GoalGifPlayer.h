#pragma once
// S3-only, flash-backed hockey-goal GIF player. No large GIF arrays in firmware.
// Upload a 320x240 GIF through the authenticated GameDay dashboard.
#ifdef GLOWDECK_WAVESHARE_S3
#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <TFT_eSPI.h>
#include <AnimatedGIF.h>

class GoalGifPlayer {
 public:
  static constexpr const char* kFilename = "/goal.gif";
  static constexpr size_t kMaxBytes = 1024 * 1024;
  bool beginStorage() {
    mounted_ = LittleFS.begin(true);
    if (!mounted_) Serial.println("[GOAL GIF] LittleFS mount failed");
    return mounted_;
  }
  bool mounted() const { return mounted_; }
  bool available() {
    if (!mounted_ || !LittleFS.exists(kFilename)) return false;
    File f = LittleFS.open(kFilename, "r");
    if (!f || f.size() < 16 || f.size() > kMaxBytes) return false;
    uint8_t h[10] = {};
    bool ok = f.read(h, 10) == 10 && validHeader(h);
    f.close();
    return ok;
  }
  size_t size() {
    if (!available()) return 0;
    File f = LittleFS.open(kFilename, "r");
    size_t n = f ? f.size() : 0;
    f.close();
    return n;
  }
  static bool validHeader(const uint8_t *h) {
    return h[0] == 'G' && h[1] == 'I' && h[2] == 'F' &&
      h[3] == '8' && (h[4] == '7' || h[4] == '9') && h[5] == 'a' &&
      ((uint16_t)h[6] | ((uint16_t)h[7] << 8)) == 320 &&
      ((uint16_t)h[8] | ((uint16_t)h[9] << 8)) == 240;
  }
  bool start(TFT_eSprite &canvas) {
    stop();
    if (!available()) return false;
    canvas_ = &canvas;
    current() = this;
    decoder_.begin(LITTLE_ENDIAN_PIXELS);
    if (!decoder_.open(kFilename, openFile, closeFile, readFile, seekFile, drawLine)) {
      Serial.printf("[GOAL GIF] Decode open failed: %d\n", decoder_.getLastError());
      stop();
      return false;
    }
    playing_ = true;
    nextFrameAt_ = 0;
    canvas_->fillScreen(TFT_BLACK);
    return true;
  }
  void stop() {
    if (playing_) decoder_.close();
    file_.close();
    playing_ = false;
    if (current() == this) current() = nullptr;
    canvas_ = nullptr;
  }
  bool active() const { return playing_; }
  bool due(uint32_t now) const {
    return playing_ && (int32_t)(now - nextFrameAt_) >= 0;
  }
  bool tick(uint32_t now) {
    if (!due(now)) return false;
    int delayMs = 140;
    if (!decoder_.playFrame(false, &delayMs)) {
      // A GIF is short (~1.26 seconds), and repeats for the entire goal overlay.
      decoder_.close();
      canvas_->fillScreen(TFT_BLACK);
      if (!decoder_.open(kFilename, openFile, closeFile, readFile, seekFile, drawLine)) {
        stop();
        return false;
      }
      if (!decoder_.playFrame(false, &delayMs)) { stop(); return false; }
    }
    nextFrameAt_ = millis() + constrain(delayMs, 50, 500);
    return true;
  }
 private:
  static GoalGifPlayer*& current() {
    static GoalGifPlayer* ptr = nullptr;
    return ptr;
  }
  static void* openFile(const char *name, int32_t *size) {
    GoalGifPlayer* player = current();
    if (!player) return nullptr;
    player->file_ = LittleFS.open(name, "r");
    if (!player->file_) return nullptr;
    *size = player->file_.size();
    return &player->file_;
  }
  static void closeFile(void *handle) {
    if (handle) static_cast<File*>(handle)->close();
  }
  static int32_t readFile(GIFFILE *gifFile, uint8_t *data, int32_t length) {
    if (!gifFile || !gifFile->fHandle || length <= 0) return 0;
    File* file = static_cast<File*>(gifFile->fHandle);
    const int32_t remain = gifFile->iSize - gifFile->iPos;
    if (remain <= 0) return 0;
    if (length > remain) length = remain;
    const int32_t read = file->read(data, length);
    gifFile->iPos = file->position();
    return read;
  }
  static int32_t seekFile(GIFFILE *gifFile, int32_t position) {
    if (!gifFile || !gifFile->fHandle || position < 0 ||
        position > gifFile->iSize) return -1;
    File* file = static_cast<File*>(gifFile->fHandle);
    if (!file->seek(position)) return -1;
    gifFile->iPos = file->position();
    return gifFile->iPos;
  }
  static void drawLine(GIFDRAW *line) {
    GoalGifPlayer* player = current();
    if (!player || !player->canvas_ || !line) return;
    int16_t y = line->iY + line->y;
    if (y < 0 || y >= 240) return;
    for (int16_t index = 0; index < line->iWidth; ++index) {
      const int16_t x = line->iX + index;
      if (x < 0 || x >= 320) continue;
      const uint8_t pixel = line->pPixels[index];
      if (line->ucHasTransparency && pixel == line->ucTransparent) {
        if (line->ucDisposalMethod == 2)
          player->canvas_->drawPixel(x, y, TFT_BLACK);
        continue;
      }
      player->canvas_->drawPixel(x, y, line->pPalette[pixel]);
    }
  }
  bool mounted_ = false, playing_ = false;
  TFT_eSprite *canvas_ = nullptr;
  File file_;
  AnimatedGIF decoder_;
  uint32_t nextFrameAt_ = 0;
};
#endif
