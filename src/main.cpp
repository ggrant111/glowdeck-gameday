#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <XPT2046_Touchscreen.h>
#include "WebUi.h"

// GlowDeck GameDay — ESP32-2432S028R (2.8" resistive CYD).
// Display: HSPI / TFT_eSPI; touch: dedicated VSPI; speaker amp input: GPIO26.
// Keep initialization order boring: display -> preferences -> AP -> web -> STA.
// No Wi-Fi or NHL network call is allowed to block boot.

namespace {
constexpr uint8_t PIN_BL = 21, PIN_AUDIO = 26, PIN_TOUCH_CS = 33, PIN_TOUCH_IRQ = 36;
constexpr uint8_t AUDIO_CH = 6;
constexpr uint8_t MAX_GAMES = 20, MAX_WLED = 4;
constexpr uint32_t LIVE_POLL_MS = 20000, IDLE_POLL_MS = 60000;
constexpr uint16_t BG = 0x0841, PANEL = 0x10A2, WHITE = 0xFFFF;
constexpr uint16_t MUTED = 0x9CF3, TEAL = 0x340F, ORANGE = 0xFD20;

struct Game {
  uint32_t id = 0;
  String home, away, state;
  int homeScore = 0, awayScore = 0;
};
struct Settings {
  String ssid, wifiPassword, adminPassword;
  String wled[MAX_WLED];
  bool wledEnabled[MAX_WLED] = {false, false, false, false};
  uint8_t mode = 3; // 0=off, 1=home, 2=away, 3=both
  uint8_t brightness = 190;
  uint8_t seconds = 8;
  bool horn = false;
  int touchX0 = 300, touchX1 = 3800, touchY0 = 300, touchY1 = 3800;
} cfg;

Preferences prefs;
TFT_eSPI tft;
SPIClass touchSPI(VSPI);
XPT2046_Touchscreen touch(PIN_TOUCH_CS, PIN_TOUCH_IRQ);
WebServer server(80);
Game games[MAX_GAMES];
uint8_t gameCount = 0, gamePage = 0;
uint32_t selectedGameId = 0;
int previousHome = 0, previousAway = 0;
bool haveBaseline = false, mdnsStarted = false, mdnsAttempted = false, needsDraw = true;
bool touchWasDown = false, wifiWasConnected = false;
String apSsid, apPassword, mdnsName, lastError;
uint32_t lastPoll = 0, reconnectAt = 0, rebootAt = 0;
uint32_t lastTouch = 0;
String savedState[MAX_WLED];
bool restoring[MAX_WLED] = {false, false, false, false};
uint32_t restoreAt[MAX_WLED] = {0, 0, 0, 0};
uint8_t restoreRetries[MAX_WLED] = {0, 0, 0, 0};
bool hornPlaying = false;
uint32_t hornStart = 0;
uint16_t hornFrequency = 0;

enum class Screen : uint8_t { LIVE, GAMES, SETUP, CALIBRATE };
Screen screen = Screen::LIVE;
uint8_t calStep = 0;
int calX, calY;

const char *modeName(uint8_t m) {
  static const char *names[] = {"OFF", "HOME", "AWAY", "BOTH"};
  return names[constrain(m, 0, 3)];
}

struct TeamColor { const char *abbr; uint32_t rgb; };
const TeamColor teamColors[] = {
  {"ANA", 0xF47A38}, {"BOS", 0xFFB81C}, {"BUF", 0x003087},
  {"CGY", 0xC8102E}, {"CAR", 0xCC0000}, {"CHI", 0xCF0A2C},
  {"COL", 0x6F263D}, {"CBJ", 0x002654}, {"DAL", 0x006847},
  {"DET", 0xCE1126}, {"EDM", 0xFF4C00}, {"FLA", 0xC8102E},
  {"LAK", 0xA2AAAD}, {"MIN", 0x154734}, {"MTL", 0xAF1E2D},
  {"NJD", 0xCE1126}, {"NSH", 0xFFB81C}, {"NYI", 0x00539B},
  {"NYR", 0x0038A8}, {"OTT", 0xC52032}, {"PHI", 0xF74902},
  {"PIT", 0xFCB514}, {"SEA", 0x99D9D9}, {"SJS", 0x006D75},
  {"STL", 0x002F87}, {"TBL", 0x002868}, {"TOR", 0x00205B},
  {"UTA", 0x73B7D3}, {"VAN", 0x00843D}, {"VGK", 0xB4975A},
  {"WSH", 0xC8102E}, {"WPG", 0x041E42}
};
uint32_t teamRgb(const String &code) {
  for (const auto &t : teamColors) if (code == t.abbr) return t.rgb;
  return 0x31837A;
}

Game *selectedGame() {
  for (uint8_t i = 0; i < gameCount; ++i)
    if (games[i].id == selectedGameId) return &games[i];
  return nullptr;
}

void loadSettings() {
  prefs.begin("gameday", false);
  cfg.ssid = prefs.getString("ssid", "");
  cfg.wifiPassword = prefs.getString("wpass", "");
  cfg.adminPassword = prefs.getString("admin", apPassword);
  for (uint8_t i = 0; i < MAX_WLED; ++i) {
    cfg.wled[i] = prefs.getString(("ip" + String(i)).c_str(), "");
    cfg.wledEnabled[i] = prefs.getBool(("enable" + String(i)).c_str(), false);
  }
  cfg.mode = constrain(prefs.getUChar("mode", 3), 0, 3);
  cfg.brightness = constrain(prefs.getUChar("bright", 190), 1, 255);
  cfg.seconds = constrain(prefs.getUChar("seconds", 8), 2, 30);
  cfg.horn = prefs.getBool("horn", false);
  cfg.touchX0 = prefs.getInt("tx0", 300);
  cfg.touchX1 = prefs.getInt("tx1", 3800);
  cfg.touchY0 = prefs.getInt("ty0", 300);
  cfg.touchY1 = prefs.getInt("ty1", 3800);
  selectedGameId = prefs.getUInt("game", 0);
}

void saveSettings() {
  prefs.putString("ssid", cfg.ssid);
  prefs.putString("wpass", cfg.wifiPassword);
  prefs.putString("admin", cfg.adminPassword);
  for (uint8_t i = 0; i < MAX_WLED; ++i) {
    prefs.putString(("ip" + String(i)).c_str(), cfg.wled[i]);
    prefs.putBool(("enable" + String(i)).c_str(), cfg.wledEnabled[i]);
  }
  prefs.putUChar("mode", cfg.mode);
  prefs.putUChar("bright", cfg.brightness);
  prefs.putUChar("seconds", cfg.seconds);
  prefs.putBool("horn", cfg.horn);
}

bool authorized() {
  if (server.authenticate("admin", cfg.adminPassword.c_str())) return true;
  server.requestAuthentication(BASIC_AUTH, "GlowDeck", "Sign in using admin and the password shown on the CYD.");
  return false;
}
void sendJson(const JsonDocument &doc, int status = 200) {
  String body;
  serializeJson(doc, body);
  server.sendHeader("Cache-Control", "no-store");
  server.send(status, "application/json", body);
}
void errorResponse(const char *message, int status = 400) {
  StaticJsonDocument<256> doc;
  doc["error"] = message;
  sendJson(doc, status);
}

String localAddress() {
  return WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String("Not connected");
}
void beginStation() {
  if (cfg.ssid.isEmpty()) return;
  WiFi.begin(cfg.ssid.c_str(), cfg.wifiPassword.c_str());
  reconnectAt = millis();
}

bool postWled(const String &ip, const String &body) {
  IPAddress addr;
  if (!addr.fromString(ip) || WiFi.status() != WL_CONNECTED) return false;
  WiFiClient client;
  HTTPClient http;
  if (!http.begin(client, "http://" + ip + "/json/state")) return false;
  http.setTimeout(1800);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(body);
  http.end();
  return code >= 200 && code < 300;
}
String getWledState(const String &ip) {
  IPAddress addr;
  if (!addr.fromString(ip) || WiFi.status() != WL_CONNECTED) return "";
  WiFiClient client;
  HTTPClient http;
  if (!http.begin(client, "http://" + ip + "/json/state")) return "";
  http.setTimeout(1800);
  int code = http.GET();
  String result;
  if (code == HTTP_CODE_OK) result = http.getString();
  http.end();
  // Avoid holding a giant or malformed snapshot in scarce ESP32 RAM.
  if (result.length() < 2 || result.length() > 14000 || result[0] != '{') return "";
  return result;
}

void startHorn() {
  if (!cfg.horn) return;
  hornPlaying = true;
  hornStart = millis();
  hornFrequency = 0;
}
void tickHorn() {
  if (!hornPlaying) return;
  const uint32_t elapsed = millis() - hornStart;
  const uint16_t nextFreq = elapsed < 170 ? 784 : elapsed < 260 ? 0 :
                             elapsed < 490 ? 880 : elapsed < 580 ? 0 :
                             elapsed < 900 ? 784 : 0;
  if (nextFreq != hornFrequency) {
    ledcWriteTone(AUDIO_CH, nextFreq);
    hornFrequency = nextFreq;
  }
  if (elapsed >= 900) hornPlaying = false;
}

void celebrate(uint32_t color) {
  StaticJsonDocument<256> doc;
  doc["on"] = true;
  doc["bri"] = cfg.brightness;
  doc["transition"] = 0;
  JsonObject seg = doc.createNestedArray("seg").createNestedObject();
  seg["id"] = 0;
  seg["fx"] = 0;
  JsonArray col = seg.createNestedArray("col").createNestedArray();
  col.add((color >> 16) & 0xFF);
  col.add((color >> 8) & 0xFF);
  col.add(color & 0xFF);
  String payload;
  serializeJson(doc, payload);

  for (uint8_t i = 0; i < MAX_WLED; ++i) {
    if (!cfg.wledEnabled[i] || cfg.wled[i].isEmpty()) continue;
    if (!restoring[i]) {
      savedState[i] = getWledState(cfg.wled[i]);
      if (savedState[i].isEmpty()) continue; // Never alter lights if we cannot restore.
    }
    if (postWled(cfg.wled[i], payload)) {
      restoring[i] = true;
      restoreRetries[i] = 0;
      restoreAt[i] = millis() + uint32_t(cfg.seconds) * 1000UL;
    } else if (!restoring[i]) savedState[i] = "";
  }
  startHorn();
}

void tickRestores() {
  for (uint8_t i = 0; i < MAX_WLED; ++i) {
    if (!restoring[i] || int32_t(millis() - restoreAt[i]) < 0) continue;
    if (postWled(cfg.wled[i], savedState[i]) || ++restoreRetries[i] >= 3) {
      restoring[i] = false;
      savedState[i] = "";
    } else restoreAt[i] = millis() + 1500;
  }
}

void pollNhl() {
  if (WiFi.status() != WL_CONNECTED) return;
  WiFiClientSecure client;
  // NHL's public endpoint; no credentials are transmitted. See SECURITY.md.
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(4500);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.useHTTP10(true); // JSON stream will not contain chunked transfer framing.
  if (!http.begin(client, "https://api-web.nhle.com/v1/score/now")) {
    lastError = "NHL connection setup failed";
    return;
  }
  int response = http.GET();
  if (response != HTTP_CODE_OK) {
    lastError = "NHL HTTP " + String(response);
    http.end();
    return;
  }
  StaticJsonDocument<640> filter;
  JsonObject gameFilter = filter["games"][0].to<JsonObject>();
  gameFilter["id"] = true;
  gameFilter["gameState"] = true;
  gameFilter["homeTeam"]["abbrev"] = true;
  gameFilter["homeTeam"]["score"] = true;
  gameFilter["awayTeam"]["abbrev"] = true;
  gameFilter["awayTeam"]["score"] = true;
  DynamicJsonDocument doc(12288);
  const DeserializationError err = deserializeJson(
    doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) {
    lastError = String("NHL JSON: ") + err.c_str();
    return;
  }
  JsonArray arr = doc["games"].as<JsonArray>();
  if (arr.isNull()) {
    lastError = "NHL games unavailable";
    return;
  }
  gameCount = 0;
  for (JsonObject item : arr) {
    if (gameCount >= MAX_GAMES) break;
    Game &g = games[gameCount++];
    g.id = item["id"] | 0U;
    g.home = item["homeTeam"]["abbrev"].as<String>();
    g.away = item["awayTeam"]["abbrev"].as<String>();
    g.state = item["gameState"].as<String>();
    g.homeScore = item["homeTeam"]["score"] | 0;
    g.awayScore = item["awayTeam"]["score"] | 0;
  }
  lastError = "";
  Game *g = selectedGame();
  if (g) {
    if (haveBaseline && (g->state == "LIVE" || g->state == "CRIT")) {
      bool homeGoal = g->homeScore > previousHome;
      bool awayGoal = g->awayScore > previousAway;
      if (homeGoal && (cfg.mode == 1 || cfg.mode == 3))
        celebrate(teamRgb(g->home));
      if (awayGoal && (cfg.mode == 2 || cfg.mode == 3))
        celebrate(teamRgb(g->away));
    }
    previousHome = g->homeScore;
    previousAway = g->awayScore;
    haveBaseline = true; // First fetch after choosing a game never replays old goals.
  } else {
    haveBaseline = false;
  }
  needsDraw = true;
}

uint16_t to565(uint32_t rgb) {
  return ((rgb >> 19) & 0x1F) << 11 | ((rgb >> 10) & 0x3F) << 5 | ((rgb >> 3) & 0x1F);
}
void textAt(const String &text, int x, int y, uint16_t color = WHITE, uint8_t font = 2) {
  tft.setTextColor(color, BG);
  tft.drawString(text, x, y, font);
}
void button(int x, int y, int w, int h, const String &label, bool active = false) {
  tft.fillRoundRect(x, y, w, h, 7, active ? TEAL : PANEL);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(WHITE, active ? TEAL : PANEL);
  tft.drawString(label, x + w / 2, y + h / 2, 2);
  tft.setTextDatum(TL_DATUM);
}
void drawCalibration() {
  tft.fillScreen(BG);
  textAt("Touch calibration", 14, 9, WHITE, 4);
  textAt(calStep == 0 ? "Tap the TOP LEFT cross" : "Tap the BOTTOM RIGHT cross", 14, 49, MUTED);
  int x = calStep == 0 ? 25 : 295, y = calStep == 0 ? 85 : 185;
  tft.drawFastHLine(x - 12, y, 25, ORANGE);
  tft.drawFastVLine(x, y - 12, 25, ORANGE);
  textAt("Use a stylus for accuracy", 12, 215, MUTED);
}

void draw() {
  needsDraw = false;
  if (screen == Screen::CALIBRATE) { drawCalibration(); return; }
  tft.fillScreen(BG);
  tft.fillRect(0, 0, 320, 34, PANEL);
  tft.setTextColor(WHITE, PANEL);
  tft.drawString("GLOWDECK", 10, 8, 2);
  tft.setTextDatum(TR_DATUM);
  tft.drawString(WiFi.status() == WL_CONNECTED ? "WiFi OK" : "SETUP AP", 309, 8, 2);
  tft.setTextDatum(TL_DATUM);

  if (screen == Screen::LIVE) {
    Game *g = selectedGame();
    if (g) {
      textAt(g->away + "  " + String(g->awayScore), 18, 45, to565(teamRgb(g->away)), 4);
      textAt("@", 154, 54, MUTED, 2);
      textAt(g->home + "  " + String(g->homeScore), 191, 45, to565(teamRgb(g->home)), 4);
      textAt(g->state == "LIVE" ? "LIVE GAME" :
             g->state == "OFF" ? "FINAL" : g->state, 113, 92, ORANGE);
      textAt("Goal celebrations", 12, 126, MUTED);
    } else {
      textAt("No game selected", 18, 54, WHITE, 4);
      textAt("Tap GAMES to pick a matchup", 18, 92, MUTED);
      textAt("Goal celebrations", 12, 126, MUTED);
    }
    for (int i = 0; i < 4; ++i)
      button(6 + i * 79, 150, 74, 34, modeName(i), cfg.mode == i);
  } else if (screen == Screen::GAMES) {
    textAt("Select an NHL game", 12, 42, WHITE);
    if (gameCount == 0) {
      textAt(lastError.isEmpty() ? "Loading games..." : lastError, 12, 81, ORANGE);
      textAt("Wi-Fi connection required", 12, 105, MUTED);
    }
    for (uint8_t n = 0; n < 3; ++n) {
      const uint8_t i = gamePage * 3 + n;
      if (i >= gameCount) break;
      Game &g = games[i];
      button(8, 66 + n * 40, 304, 36,
             g.away + " " + String(g.awayScore) + "  @  " +
             g.home + " " + String(g.homeScore) + "  " + g.state,
             g.id == selectedGameId);
    }
    if (gameCount > 3) {
      tft.setTextDatum(TR_DATUM);
      textAt("", 0, 0); // keep font/background state deterministic
      tft.setTextColor(MUTED, BG);
      tft.drawString("< prev       next >", 309, 40, 2);
      tft.setTextDatum(TL_DATUM);
    }
  } else if (screen == Screen::SETUP) {
    textAt("Web setup", 12, 43, WHITE, 4);
    textAt("Network: " + (cfg.ssid.isEmpty() ? String("Not configured") : cfg.ssid), 12, 76, MUTED);
    textAt("IP: " + localAddress(), 12, 97, MUTED);
    textAt("AP: " + apSsid, 12, 118, MUTED);
    textAt("AP password: " + apPassword, 12, 138, WHITE);
    button(10, 161, 145, 29, "TEST LIGHTS");
    button(166, 161, 145, 29, "CALIBRATE");
  }
  button(6, 200, 99, 34, "LIVE", screen == Screen::LIVE);
  button(111, 200, 99, 34, "GAMES", screen == Screen::GAMES);
  button(216, 200, 98, 34, "SETUP", screen == Screen::SETUP);
}

void selectGame(uint32_t id) {
  selectedGameId = id;
  haveBaseline = false;
  prefs.putUInt("game", id);
  screen = Screen::LIVE;
  needsDraw = true;
}

void handleTouch() {
  // Act on touch DOWN only; a held press must never advance two calibration steps.
  bool pressed = touch.touched();
  if (!pressed) { touchWasDown = false; return; }
  if (touchWasDown || millis() - lastTouch < 120) return;
  touchWasDown = true;
  TS_Point p = touch.getPoint();
  lastTouch = millis();
  if (screen == Screen::CALIBRATE) {
    if (calStep == 0) {
      calX = p.x;
      calY = p.y;
      calStep = 1;
    } else {
      if (abs(p.x - calX) > 400 && abs(p.y - calY) > 400) {
        cfg.touchX0 = calX - (p.x - calX) * 25 / 270;
        cfg.touchX1 = p.x + (p.x - calX) * 24 / 270;
        cfg.touchY0 = calY - (p.y - calY) * 85 / 100;
        cfg.touchY1 = p.y + (p.y - calY) * 54 / 100;
        prefs.putInt("tx0", cfg.touchX0);
        prefs.putInt("tx1", cfg.touchX1);
        prefs.putInt("ty0", cfg.touchY0);
        prefs.putInt("ty1", cfg.touchY1);
      }
      screen = Screen::SETUP;
      calStep = 0;
    }
    needsDraw = true;
    return;
  }
  const int x = constrain((int)map(p.x, cfg.touchX0, cfg.touchX1, 0, 319), 0, 319);
  const int y = constrain((int)map(p.y, cfg.touchY0, cfg.touchY1, 0, 239), 0, 239);
  if (y >= 198) {
    screen = x < 107 ? Screen::LIVE : x < 214 ? Screen::GAMES : Screen::SETUP;
  } else if (screen == Screen::LIVE && y >= 148 && y < 187) {
    cfg.mode = constrain(x / 79, 0, 3);
    prefs.putUChar("mode", cfg.mode);
  } else if (screen == Screen::GAMES) {
    if (y >= 39 && y < 64 && gameCount > 3) {
      if (x > 170 && (gamePage + 1) * 3 < gameCount) ++gamePage;
      else if (x <= 170 && gamePage) --gamePage;
    } else if (y >= 66 && y < 186) {
      uint8_t index = gamePage * 3 + (y - 66) / 40;
      if (index < gameCount) selectGame(games[index].id);
    }
  } else if (screen == Screen::SETUP && y >= 160 && y < 193) {
    if (x < 158) {
      Game *g = selectedGame();
      celebrate(teamRgb(g ? g->home : String("UTA")));
    } else {
      screen = Screen::CALIBRATE;
      calStep = 0;
    }
  }
  needsDraw = true;
}

void registerRoutes() {
  server.on("/", HTTP_GET, []() {
    if (!authorized()) return;
    server.send_P(200, "text/html; charset=utf-8", WEB_UI);
  });
  server.on("/api/status", HTTP_GET, []() {
    if (!authorized()) return;
    StaticJsonDocument<1024> doc;
    doc["version"] = "0.1.0";
    doc["wifiConnected"] = WiFi.status() == WL_CONNECTED;
    doc["ip"] = localAddress();
    doc["apSsid"] = apSsid;
    doc["apIp"] = WiFi.softAPIP().toString();
    doc["mdns"] = mdnsStarted ? mdnsName + ".local" : "";
    doc["selectedId"] = selectedGameId;
    doc["mode"] = cfg.mode;
    doc["error"] = lastError;
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["uptimeSeconds"] = millis() / 1000UL;
    sendJson(doc);
  });
  server.on("/api/games", HTTP_GET, []() {
    if (!authorized()) return;
    DynamicJsonDocument doc(8192);
    JsonArray arr = doc.createNestedArray("games");
    for (uint8_t i = 0; i < gameCount; ++i) {
      JsonObject g = arr.createNestedObject();
      g["id"] = games[i].id;
      g["home"] = games[i].home;
      g["away"] = games[i].away;
      g["homeScore"] = games[i].homeScore;
      g["awayScore"] = games[i].awayScore;
      g["state"] = games[i].state;
    }
    sendJson(doc);
  });
  server.on("/api/select", HTTP_POST, []() {
    if (!authorized()) return;
    StaticJsonDocument<128> doc;
    if (deserializeJson(doc, server.arg("plain"))) return errorResponse("Invalid JSON");
    uint32_t id = doc["id"] | 0U;
    for (uint8_t i = 0; i < gameCount; ++i) {
      if (games[i].id == id) {
        selectGame(id);
        StaticJsonDocument<64> ok; ok["ok"] = true; sendJson(ok); return;
      }
    }
    errorResponse("Game is not in the current scoreboard", 404);
  });
  server.on("/api/config", HTTP_GET, []() {
    if (!authorized()) return;
    StaticJsonDocument<1536> doc;
    doc["ssid"] = cfg.ssid;
    doc["mode"] = cfg.mode;
    doc["brightness"] = cfg.brightness;
    doc["seconds"] = cfg.seconds;
    doc["horn"] = cfg.horn;
    JsonArray wled = doc.createNestedArray("wled");
    for (uint8_t i = 0; i < MAX_WLED; ++i) {
      JsonObject d = wled.createNestedObject();
      d["ip"] = cfg.wled[i];
      d["enabled"] = cfg.wledEnabled[i];
    }
    sendJson(doc);
  });
  server.on("/api/config", HTTP_POST, []() {
    if (!authorized()) return;
    if (server.arg("plain").length() > 2500) return errorResponse("Configuration too large");
    StaticJsonDocument<2560> doc;
    if (deserializeJson(doc, server.arg("plain"))) return errorResponse("Invalid JSON");
    String ssid = doc["ssid"] | cfg.ssid;
    if (ssid.length() > 32) return errorResponse("SSID too long");
    String password = doc["password"] | String("");
    String admin = doc["adminPassword"] | String("");
    if (!admin.isEmpty() && admin.length() < 8)
      return errorResponse("Admin password requires at least 8 characters");
    if (password.length() > 64 || admin.length() > 64)
      return errorResponse("Password too long");
    bool wifiChanged = ssid != cfg.ssid || !password.isEmpty();
    cfg.ssid = ssid;
    if (!password.isEmpty()) cfg.wifiPassword = password;
    if (!admin.isEmpty()) cfg.adminPassword = admin;
    cfg.mode = constrain((int)(doc["mode"] | cfg.mode), 0, 3);
    cfg.brightness = constrain((int)(doc["brightness"] | cfg.brightness), 1, 255);
    cfg.seconds = constrain((int)(doc["seconds"] | cfg.seconds), 2, 30);
    cfg.horn = doc["horn"] | cfg.horn;
    JsonArray arr = doc["wled"].as<JsonArray>();
    for (uint8_t i = 0; i < MAX_WLED && !arr.isNull() && i < arr.size(); ++i) {
      String ip = arr[i]["ip"] | String("");
      ip.trim();
      IPAddress parsed;
      if (!ip.isEmpty() && !parsed.fromString(ip))
        return errorResponse("WLED address must be an IPv4 address");
      cfg.wled[i] = ip;
      cfg.wledEnabled[i] = arr[i]["enabled"] | false;
    }
    saveSettings();
    if (wifiChanged) {
      WiFi.disconnect();
      beginStation();
    }
    needsDraw = true;
    StaticJsonDocument<96> ok;
    ok["ok"] = true;
    sendJson(ok);
  });
  server.on("/api/wifi/scan", HTTP_GET, []() {
    if (!authorized()) return;
    int n = WiFi.scanNetworks(false, true);
    StaticJsonDocument<2200> doc;
    JsonArray arr = doc.createNestedArray("networks");
    for (int i = 0; i < n && i < 16; ++i) {
      JsonObject x = arr.createNestedObject();
      x["ssid"] = WiFi.SSID(i);
      x["rssi"] = WiFi.RSSI(i);
    }
    WiFi.scanDelete();
    sendJson(doc);
  });
  server.on("/api/test", HTTP_POST, []() {
    if (!authorized()) return;
    StaticJsonDocument<160> doc;
    if (deserializeJson(doc, server.arg("plain"))) return errorResponse("Invalid JSON");
    String side = doc["side"] | String("home");
    if (side != "home" && side != "away") return errorResponse("Invalid side");
    Game *g = selectedGame();
    celebrate(teamRgb(g ? (side == "home" ? g->home : g->away) : String("UTA")));
    StaticJsonDocument<96> ok; ok["ok"] = true; sendJson(ok);
  });
  server.on("/api/update", HTTP_POST, []() {
    if (!authorized()) return;
    bool ok = !Update.hasError();
    server.send(ok ? 200 : 500, "text/plain",
                ok ? "Firmware accepted. Rebooting..." : "Firmware update failed");
    if (ok) rebootAt = millis() + 1000;
  }, []() {
    if (!server.authenticate("admin", cfg.adminPassword.c_str())) return;
    HTTPUpload &upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("OTA upload: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
        Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_END) {
      if (!Update.end(true)) Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
      Update.abort();
    }
  });
  server.on("/api/reboot", HTTP_POST, []() {
    if (!authorized()) return;
    server.send(200, "text/plain", "Rebooting...");
    rebootAt = millis() + 900;
  });
  server.onNotFound([]() {
    if (!authorized()) return;
    errorResponse("Not found", 404);
  });
  server.begin();
}
} // namespace

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\nGlowDeck GameDay v0.1.0 — CYD only");

  pinMode(PIN_BL, OUTPUT);
  digitalWrite(PIN_BL, HIGH);
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(BG);
  textAt("GlowDeck GameDay", 15, 30, WHITE, 4);
  textAt("Starting local controls...", 15, 68, MUTED);

  touchSPI.begin(25, 39, 32, PIN_TOUCH_CS);
  touch.begin(touchSPI);
  touch.setRotation(1);

  ledcSetup(AUDIO_CH, 2000, 8);
  ledcAttachPin(PIN_AUDIO, AUDIO_CH);
  ledcWriteTone(AUDIO_CH, 0);

  char suffix[7];
  snprintf(suffix, sizeof(suffix), "%06X", (unsigned)(ESP.getEfuseMac() & 0xFFFFFF));
  apSsid = "GlowDeck-" + String(suffix);
  apPassword = "GD" + String(suffix) + "99";
  mdnsName = "glowdeck-" + String(suffix);
  mdnsName.toLowerCase();
  loadSettings();

  WiFi.persistent(false);
  WiFi.setSleep(false);
  WiFi.mode(WIFI_AP_STA);
  if (!WiFi.softAP(apSsid.c_str(), apPassword.c_str())) Serial.println("AP start failed");
  registerRoutes();
  beginStation();
  draw();
  Serial.printf("AP: %s | Password: %s | Browse: http://192.168.4.1\n",
                apSsid.c_str(), apPassword.c_str());
  Serial.printf("Web admin username: admin | password: %s (unless changed)\n",
                cfg.adminPassword.c_str());
}

void loop() {
  server.handleClient();
  handleTouch();
  tickHorn();
  tickRestores();

  const uint32_t now = millis();
  const bool connected = WiFi.status() == WL_CONNECTED;
  if (connected && !wifiWasConnected) {
    wifiWasConnected = true;
    mdnsAttempted = false;
    lastPoll = millis() - IDLE_POLL_MS; // Fetch soon after connection, not 60 seconds later.
    needsDraw = true;
  } else if (!connected && wifiWasConnected) {
    wifiWasConnected = false;
    mdnsStarted = false;
    mdnsAttempted = false;
    needsDraw = true;
  }
  if (connected) {
    if (!mdnsAttempted) {
      mdnsAttempted = true;
      mdnsStarted = MDNS.begin(mdnsName.c_str());
      if (mdnsStarted) MDNS.addService("http", "tcp", 80);
      needsDraw = true;
      Serial.printf("STA: http://%s/\n", WiFi.localIP().toString().c_str());
    }
    uint32_t interval = selectedGameId ? LIVE_POLL_MS : IDLE_POLL_MS;
    if (now - lastPoll >= interval) {
      lastPoll = now;
      pollNhl();
    }
  } else if (!cfg.ssid.isEmpty() && now - reconnectAt > 20000) {
    WiFi.reconnect();
    reconnectAt = now;
    needsDraw = true;
  }
  if (needsDraw) draw();
  if (rebootAt && int32_t(now - rebootAt) >= 0) ESP.restart();
  delay(5);
}
