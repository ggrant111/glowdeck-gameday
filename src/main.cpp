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
#include "TeamLogos.h"
#include "GameDayDate.h"
#include "GameDayCadence.h"
#include "ConferenceRecords.h"
#include "GameDetailMode.h"
#include "StandingsCarousel.h"
#include "NhlFetchPolicy.h"
#include "RearLedPattern.h"
#include <time.h>

// GlowDeck GameDay — ESP32-2432S028R (2.8" resistive CYD).
// Display: HSPI / TFT_eSPI; touch: dedicated VSPI; speaker amp input: GPIO26.
// Keep initialization order boring: display -> preferences -> AP -> web -> STA.
// No Wi-Fi or NHL network call is allowed to block boot.

namespace {
constexpr uint8_t PIN_BL = 21, PIN_AUDIO = 26, PIN_TOUCH_CS = 33, PIN_TOUCH_IRQ = 36;
constexpr uint8_t AUDIO_CH = 6;
constexpr uint8_t PIN_RGB_R = 4, PIN_RGB_G = 16, PIN_RGB_B = 17;
constexpr uint8_t RGB_CH_R = 2, RGB_CH_G = 3, RGB_CH_B = 4;
constexpr uint8_t MAX_FOLLOWED_GAMES = 4;
constexpr uint8_t MAX_GAMES = 20, MAX_WLED = 4;
constexpr uint16_t BG = 0x0841, PANEL = 0x10A2, WHITE = 0xFFFF;
constexpr uint16_t MUTED = 0x9CF3, TEAL = 0x340F, ORANGE = 0xFD20;

struct Game {
  uint32_t id = 0;
  String home, away, homeName, awayName, state, startUTC, clock, periodType;
  int homeScore = 0, awayScore = 0, homeShots = -1, awayShots = -1;
  uint8_t period = 0;
  bool intermission = false;
  uint8_t ppSide = 0; // 0=not known/no PP, 1=home PP, 2=away PP
  bool ppDataKnown = false;
  uint32_t ppUpdatedAt = 0;
};
struct Settings {
  String ssid, wifiPassword, adminPassword;
  String wled[MAX_WLED];
  bool wledEnabled[MAX_WLED] = {false, false, false, false};
  uint8_t mode = 3; // 0=off, 1=home, 2=away, 3=both
  uint8_t brightness = 190;
  uint8_t seconds = 8;
  bool horn = false;
  bool rearLedEnabled = true;
  bool rearGoal = true;
  bool rearPowerPlay = true;
  uint8_t rearBrightness = 128;
  uint8_t rearGoalSeconds = 6;
  uint32_t followIds[MAX_FOLLOWED_GAMES] = {0, 0, 0, 0};
  bool invertScreen = true; // This CYD panel needs INVON to render dark colors correctly.
  bool cycleGames = true;
  bool showStandings = true; // Add 4 conference + 4 division table slides.
  uint8_t cycleSeconds = 9;
  uint8_t timezone = 1; // 0=ET, 1=CT, 2=MT, 3=PT, 4=AZ
  int touchX0 = 300, touchX1 = 3800, touchY0 = 300, touchY1 = 3800;
} cfg;

Preferences prefs;
TFT_eSPI tft;
SPIClass touchSPI(VSPI);
XPT2046_Touchscreen touch(PIN_TOUCH_CS, PIN_TOUCH_IRQ);
WebServer server(80);
Game games[MAX_GAMES];
struct GoalWatch {
  uint32_t id = 0;
  int home = 0, away = 0;
  bool baseline = false;
};
GoalWatch goalWatch[MAX_GAMES];
struct PowerPlaySnapshot {
  uint32_t id = 0, updatedAt = 0;
  uint8_t side = 0;
  bool known = false;
};
PowerPlaySnapshot ppCache[MAX_GAMES];
enum class RearEvent : uint8_t { IDLE, GOAL, POWER_PLAY };
RearEvent rearEvent = RearEvent::IDLE;
uint32_t rearGoalAt = 0, rearGoalColor = 0, rearGoalDuration = 0;
uint32_t rearPulseStart = 0;
uint32_t rearLastTick = 0;
uint8_t rearLastDuty[3] = {255, 255, 255};
uint32_t lastPpFallback = 0;
uint32_t rearTestPpUntil = 0, rearTestPpColor = 0;
uint8_t gameCount = 0, gamePage = 0, featuredIndex = 0;
uint8_t carouselSlide = 0; // Independent of the armed game and game-detail index.
uint32_t lastCarouselAdvance = 0;
String scoreboardDate;
String targetScoreboardDate;
bool scoreboardLoaded = false;
uint32_t lastDateCheck = 0;
uint32_t selectedGameId = 0;
int previousHome = 0, previousAway = 0;
bool haveBaseline = false, mdnsStarted = false, mdnsAttempted = false, needsDraw = true;
bool touchWasDown = false, wifiWasConnected = false, partialLiveUpdate = false;
int16_t gestureStartX = 0, gestureStartY = 0, gestureEndX = 0, gestureEndY = 0;
bool swallowLiveGesture = false;
String apSsid, apPassword, mdnsName, lastError;
uint32_t lastPoll = 0, reconnectAt = 0, rebootAt = 0;
// NHL feed health diagnostics; no extra heap copies of the raw JSON payload.
uint8_t nhlConsecutiveFailures = 0;
int nhlLastHttpCode = 0;
int nhlLastContentLength = -1;
uint32_t nhlLastDurationMs = 0;
uint32_t nhlLastSuccessAt = 0;
uint32_t lastTouch = 0;
String savedState[MAX_WLED];
bool restoring[MAX_WLED] = {false, false, false, false};
uint32_t restoreAt[MAX_WLED] = {0, 0, 0, 0};
uint8_t restoreRetries[MAX_WLED] = {0, 0, 0, 0};
bool hornPlaying = false;
uint32_t hornStart = 0;
uint16_t hornFrequency = 0;

enum class Screen : uint8_t { LIVE, GAMES, SETUP, CALIBRATE, STATS };
struct DetailStats {
  uint32_t gameId = 0, updatedAt = 0;
  bool loaded = false;
  bool fetchedPregame = false; // Used to switch from season records to live game stats.
  String error;
  String label[5], away[5], home[5];
  uint8_t count = 0;
} detail;
bool needDetailLoad = false;
uint32_t detailGameId = 0;

// Keep the compact scoreboard stats per game, not just in the current detail view.
// Cached once when a carousel game is shown; refreshed for ongoing games.
struct CarouselStats {
  uint32_t id = 0, updatedAt = 0;
  bool loaded = false;
  String ppAway, ppHome, pimAway, pimHome, faceAway, faceHome;
  String recordAway, recordHome, pointsAway, pointsHome;
};
CarouselStats carouselStats[MAX_GAMES];
uint32_t lastCarouselStatsCheck = 0;

// Cache league standings once across all upcoming matchups, rather than
// repeatedly fetching the same 32-team feed on every carousel advance.
struct SeasonStanding {
  String team, seasonRecord, points, conferenceRank;
  StandingsCarousel::Group division = StandingsCarousel::Group::EAST;
  uint8_t conferenceSequence = 0, divisionSequence = 0;
  uint8_t gamesPlayed = 0, wins = 0, losses = 0, otLosses = 0;
};
// The carousel can show games followed by eight league-wide standings slides.
// Browsing a standings slide must NEVER change the WLED armed game, score
// baselines, live polls or the selected game detail.
uint8_t carouselPageCount() {
  return StandingsCarousel::totalSlides(gameCount, cfg.showStandings);
}
bool showingStandingsPage() {
  return cfg.showStandings && carouselSlide >= gameCount &&
         carouselSlide < carouselPageCount();
}
void advanceCarousel(int direction) {
  const uint8_t total = carouselPageCount();
  carouselSlide = StandingsCarousel::advance(carouselSlide, total, direction);
  if (carouselSlide < gameCount) featuredIndex = carouselSlide;
  lastCarouselAdvance = millis();
  needsDraw = true;
}

SeasonStanding seasonStandings[32];
bool standingsReady = false;
uint32_t standingsLastAttempt = 0;
String standingsForDate;

// The NHL standings feed has conference *rank*, not record vs conference.
// Derive the W-L-OT vs-conference split from each team's season schedule,
// in a low-priority/on-demand task only while its pregame detail is open.
struct ConferenceSnapshot {
  String team;
  uint32_t season = 0, checkedAt = 0;
  bool tried = false, valid = false;
  String record;
};
ConferenceSnapshot conferenceSnapshots[32];
uint32_t lastConferenceFetch = 0;
uint32_t webPreviewGameId = 0, webPreviewUntil = 0;

CarouselStats &statsFor(uint8_t index) {
  CarouselStats &entry = carouselStats[index];
  if (entry.id != games[index].id) {
    entry = CarouselStats();
    entry.id = games[index].id;
  }
  return entry;
}

bool statsAreFresh(const Game &g, const CarouselStats &stat) {
  if (!stat.loaded || stat.id != g.id) return false;
  const uint32_t ttl = (g.state == "LIVE" || g.state == "CRIT") ? 60000UL
       : (g.state == "FUT" || g.state == "PRE") ? 900000UL : 3600000UL;
  return uint32_t(millis() - stat.updatedAt) < ttl;
}

void updateCarouselStatsFromDetail(uint8_t index, const DetailStats &source) {
  CarouselStats &cached = statsFor(index);
  cached.loaded = true;
  cached.updatedAt = millis();
  for (uint8_t i = 0; i < source.count; ++i) {
    if (source.label[i] == "POWER PLAY") {
      cached.ppAway = source.away[i]; cached.ppHome = source.home[i];
    } else if (source.label[i] == "PEN. MIN") {
      cached.pimAway = source.away[i]; cached.pimHome = source.home[i];
    } else if (source.label[i] == "FACEOFFS") {
      cached.faceAway = source.away[i]; cached.faceHome = source.home[i];
    } else if (source.label[i] == "SEASON" || source.label[i] == "W-L-OT") {
      cached.recordAway = source.away[i]; cached.recordHome = source.home[i];
    } else if (source.label[i] == "POINTS") {
      cached.pointsAway = source.away[i]; cached.pointsHome = source.home[i];
    }
  }
}

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

bool rearFollows(uint32_t gameId) {
  if (!gameId) return false;
  if (gameId == selectedGameId) return true; // Armed WLED game also drives rear RGB.
  for (uint8_t i = 0; i < MAX_FOLLOWED_GAMES; ++i)
    if (cfg.followIds[i] == gameId) return true;
  return false;
}

// All games share the official date-scoped NHL scoreboard request. Prioritize
// live matchups that drive lights, without hammering the API while idle.
uint32_t currentScoreboardPollIntervalMs() {
  bool anyLive = false;
  bool priorityLive = false;
  bool armedPregame = false;
  for (uint8_t i = 0; i < gameCount; ++i) {
    const Game &g = games[i];
    const bool live = g.state == "LIVE" || g.state == "CRIT";
    if (live) {
      anyLive = true;
      if (rearFollows(g.id)) priorityLive = true;
    }
    if (g.id == selectedGameId &&
        (g.state == "FUT" || g.state == "PRE")) armedPregame = true;
  }
  return GameDayCadence::intervalMs(priorityLive, anyLive, armedPregame);
}

void setupRearLed() {
  // Active-LOW common-anode LED: PWM duty 255 is OFF.
  const uint8_t pins[3] = {PIN_RGB_R, PIN_RGB_G, PIN_RGB_B};
  const uint8_t channels[3] = {RGB_CH_R, RGB_CH_G, RGB_CH_B};
  for (uint8_t i = 0; i < 3; ++i) {
    pinMode(pins[i], OUTPUT);
    digitalWrite(pins[i], HIGH); // OFF before PWM is attached.
    ledcSetup(channels[i], 4000, 8);
    ledcAttachPin(pins[i], channels[i]);
    ledcWrite(channels[i], 255);
  }
}

void writeRearLed(uint32_t rgb, uint8_t intensity) {
  const uint8_t colors[3] = {
    (uint8_t)((rgb >> 16) & 255), (uint8_t)((rgb >> 8) & 255),
    (uint8_t)(rgb & 255)
  };
  const uint8_t channels[3] = {RGB_CH_R, RGB_CH_G, RGB_CH_B};
  for (uint8_t i = 0; i < 3; ++i) {
    const uint8_t duty = RearLedPattern::invertedDuty(
      colors[i], cfg.rearBrightness, intensity);
    if (duty != rearLastDuty[i]) {
      rearLastDuty[i] = duty;
      ledcWrite(channels[i], duty);
    }
  }
}

void triggerRearGoal(uint32_t rgb) {
  if (!cfg.rearLedEnabled || !cfg.rearGoal) return;
  rearGoalColor = rgb;
  rearGoalAt = millis();
  rearGoalDuration = uint32_t(cfg.rearGoalSeconds) * 1000UL;
  rearEvent = RearEvent::GOAL;
}

void stopRearLed() {
  rearEvent = RearEvent::IDLE;
  rearGoalDuration = 0;
  rearTestPpUntil = 0;
  writeRearLed(0, 0);
}

// The NHL 'situation' object marks which team has "PP" and/or more skaters.
// Treat missing data as UNKNOWN: no invented power-play event.
uint8_t detectPowerPlay(JsonVariantConst situation) {
  if (situation.isNull()) return 0;
  JsonVariantConst home = situation["homeTeam"];
  JsonVariantConst away = situation["awayTeam"];
  auto hasPP = [](JsonVariantConst team) -> bool {
    for (JsonVariantConst label : team["situationDescriptions"].as<JsonArrayConst>()) {
      if (label.as<String>().indexOf("PP") >= 0) return true;
    }
    return false;
  };
  const bool homePP = hasPP(home), awayPP = hasPP(away);
  if (homePP && !awayPP) return 1;
  if (awayPP && !homePP) return 2;
  if (!home["strength"].isNull() && !away["strength"].isNull()) {
    const int h = home["strength"] | 0, a = away["strength"] | 0;
    if (h > a) return 1;
    if (a > h) return 2;
  }
  return 0;
}

uint8_t activePowerPlaySide(uint32_t now, uint32_t &color) {
  for (uint8_t i = 0; i < gameCount; ++i) {
    const Game &g = games[i];
    if (!rearFollows(g.id) || (g.state != "LIVE" && g.state != "CRIT") ||
        g.intermission || !g.ppDataKnown || !g.ppSide ||
        uint32_t(now - g.ppUpdatedAt) > 80000UL) continue;
    color = teamRgb(g.ppSide == 1 ? g.home : g.away);
    return g.ppSide;
  }
  return 0;
}

void tickRearLed() {
  const uint32_t now = millis();
  if (uint32_t(now - rearLastTick) < 25UL) return;
  rearLastTick = now;
  if (!cfg.rearLedEnabled) { stopRearLed(); return; }
  const uint32_t goalElapsed = uint32_t(now - rearGoalAt);
  if (rearGoalDuration && goalElapsed < rearGoalDuration && cfg.rearGoal) {
    rearEvent = RearEvent::GOAL;
    writeRearLed(rearGoalColor,
                 RearLedPattern::flash(goalElapsed, rearGoalDuration));
    return;
  }
  rearGoalDuration = 0;
  uint32_t ppColor = 0;
  const bool testPulse = rearTestPpUntil &&
      int32_t(rearTestPpUntil - now) > 0;
  if (testPulse) ppColor = rearTestPpColor;
  if (testPulse || (cfg.rearPowerPlay && activePowerPlaySide(now, ppColor))) {
    if (rearEvent != RearEvent::POWER_PLAY) rearPulseStart = now;
    rearEvent = RearEvent::POWER_PLAY;
    writeRearLed(ppColor, RearLedPattern::pulse(uint32_t(now - rearPulseStart)));
    return;
  }
  rearEvent = RearEvent::IDLE;
  writeRearLed(0, 0);
}

// Every game gets an independent baseline, preventing false flashes on reboot,
// date rollover, first fetch, or when following a game already in progress.
void detectRearGoalChanges() {
  for (uint8_t i = 0; i < gameCount; ++i) {
    const Game &g = games[i];
    GoalWatch &watch = goalWatch[i];
    if (watch.id != g.id) {
      watch = GoalWatch();
      watch.id = g.id;
    }
    const bool live = g.state == "LIVE" || g.state == "CRIT";
    if (watch.baseline && live && rearFollows(g.id)) {
      if (g.homeScore > watch.home) triggerRearGoal(teamRgb(g.home));
      if (g.awayScore > watch.away) triggerRearGoal(teamRgb(g.away));
    }
    watch.home = g.homeScore;
    watch.away = g.awayScore;
    watch.baseline = true;
  }
}

Game *selectedGame() {
  for (uint8_t i = 0; i < gameCount; ++i)
    if (games[i].id == selectedGameId) return &games[i];
  return nullptr;
}

// Timezone follows the user's chosen local zone, including daylight saving
// transitions. NHL start times are sent in UTC and don't need a local NTP clock.
const char *timezoneRule() {
  switch (cfg.timezone) {
    case 0: return "EST5EDT,M3.2.0/2,M11.1.0/2";
    case 1: return "CST6CDT,M3.2.0/2,M11.1.0/2";
    case 2: return "MST7MDT,M3.2.0/2,M11.1.0/2";
    case 3: return "PST8PDT,M3.2.0/2,M11.1.0/2";
    case 4: return "MST7";
    default: return "CST6CDT,M3.2.0/2,M11.1.0/2";
  }
}
const char *timezoneLabel() {
  static const char *names[] = {"ET", "CT", "MT", "PT", "AZ"};
  return names[constrain(cfg.timezone, 0, 4)];
}
void applyTimezone() {
  setenv("TZ", timezoneRule(), 1);
  tzset();
}

// Local date is authoritative for the calendar displayed by the CYD.
// The NHL's /score/now redirect can remain on yesterday well into the morning.
String deviceLocalDate() {
  char date[11] = {};
  return formatLocalGameDate(time(nullptr), date) ? String(date) : String("");
}

void saveSettings(); // Declaration: Preferences implementation follows later.

void prepareNewScoreboardDate(const String &today) {
  if (today.isEmpty() || today == targetScoreboardDate) return;
  targetScoreboardDate = today;
  // If the device was displaying yesterday, immediately retire that slate.
  // This also avoids accidental goal alerts for previous-day games.
  if (scoreboardLoaded && scoreboardDate != today) {
    Serial.printf("GameDay rollover: %s -> %s\n",
                  scoreboardDate.c_str(), today.c_str());
    scoreboardLoaded = false;
    scoreboardDate = "";
    gameCount = 0;
    gamePage = 0;
    featuredIndex = 0;
    lastCarouselAdvance = millis();
    needDetailLoad = false;
    detail.loaded = false;
    for (auto &entry : carouselStats) entry = CarouselStats();
    standingsReady = false;
    standingsLastAttempt = 0;
    standingsForDate = "";
    for (auto &entry : seasonStandings) entry = SeasonStanding();
    for (auto &entry : conferenceSnapshots) entry = ConferenceSnapshot();
    lastConferenceFetch = 0;
    webPreviewGameId = 0;
    for (auto &entry : goalWatch) entry = GoalWatch();
    for (auto &entry : ppCache) entry = PowerPlaySnapshot();
    for (uint8_t i = 0; i < MAX_FOLLOWED_GAMES; ++i) cfg.followIds[i] = 0;
    saveSettings();
    stopRearLed();
    if (screen == Screen::STATS) screen = Screen::LIVE;
    if (selectedGameId) {
      selectedGameId = 0;
      prefs.putUInt("game", 0);
    }
    haveBaseline = false;
    previousHome = previousAway = 0;
    needsDraw = true;
  }
  // Poll the explicitly dated endpoint on the next loop, even when the
  // previous game was being polled on a slower idle interval.
  lastPoll = millis() - GameDayCadence::kIdleMs;
}
String formattedStart(const String &utc) {
  int yr = 0, mo = 0, dy = 0, hh = 0, mm = 0, sec = 0;
  if (sscanf(utc.c_str(), "%d-%d-%dT%d:%d:%d",
             &yr, &mo, &dy, &hh, &mm, &sec) != 6)
    return "Time TBA";
  struct tm utcTime = {};
  utcTime.tm_year = yr - 1900;
  utcTime.tm_mon = mo - 1;
  utcTime.tm_mday = dy;
  utcTime.tm_hour = hh;
  utcTime.tm_min = mm;
  utcTime.tm_sec = sec;
  // Arduino-ESP32's newlib does not expose timegm(). Convert UTC civil date
  // directly to Unix days, without temporarily changing global TZ state.
  const int y = yr - (mo <= 2 ? 1 : 0);
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const int adjustedMonth = mo + (mo > 2 ? -3 : 9);
  const unsigned doy = (153 * adjustedMonth + 2) / 5 + dy - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  const long days = era * 146097L + (long)doe - 719468L;
  const time_t epoch = (time_t)(days * 86400L + hh * 3600L + mm * 60L + sec);
  struct tm localTime = {};
  localtime_r(&epoch, &localTime);
  char timeBuffer[24] = {};
  strftime(timeBuffer, sizeof(timeBuffer), "%I:%M %p", &localTime);
  String result = timeBuffer;
  if (result.startsWith("0")) result.remove(0, 1);
  return result + " " + timezoneLabel();
}
String gameStatus(const Game &g) {
  if (g.state == "FUT" || g.state == "PRE")
    return "START " + formattedStart(g.startUTC);
  if (g.state == "LIVE" || g.state == "CRIT") {
    const String period = g.periodType == "SO" ? "SO" :
                          g.periodType == "OT" ? "OT" : "P" + String(g.period);
    if (g.intermission) return "INTERMISSION " + period;
    return period + (g.clock.isEmpty() ? String("  LIVE") : String("  ") + g.clock);
  }
  if (g.state == "OFF" || g.state == "FINAL")
    return g.periodType == "SO" ? "FINAL / SO" :
           g.periodType == "OT" ? "FINAL / OT" : "FINAL";
  return g.state.isEmpty() ? "SCHEDULED" : g.state;
}
void showDetail(uint32_t id) {
  if (!id) return;
  detailGameId = id;
  if (detail.gameId != id) detail.loaded = false;
  needDetailLoad = true;
  screen = Screen::STATS;
  needsDraw = true;
}
Game *detailGame() {
  for (uint8_t i = 0; i < gameCount; ++i)
    if (games[i].id == detailGameId) return &games[i];
  return nullptr;
}
void addDetailRow(const String &name, const String &away, const String &home) {
  if (detail.count >= 5) return;
  detail.label[detail.count] = name;
  detail.away[detail.count] = away;
  detail.home[detail.count] = home;
  ++detail.count;
}
uint32_t seasonStartYear(uint32_t gameId) {
  // NHL game IDs are YYYY + game type + game number (e.g., 2026020118).
  return gameId / 1000000UL;
}

ConferenceSnapshot *conferenceSnapshot(const String &team, uint32_t season) {
  for (auto &c : conferenceSnapshots)
    if (c.team == team && c.season == season) return &c;
  for (auto &c : conferenceSnapshots) if (c.team.isEmpty()) {
    c = ConferenceSnapshot();
    c.team = team;
    c.season = season;
    return &c;
  }
  // Never accumulate records from different NHL seasons in the same slot.
  ConferenceSnapshot &oldest = conferenceSnapshots[0];
  oldest = ConferenceSnapshot();
  oldest.team = team;
  oldest.season = season;
  return &oldest;
}

String knownConferenceRecord(const String &team, uint32_t season) {
  ConferenceSnapshot *c = conferenceSnapshot(team, season);
  return c && c->valid ? c->record : String("--");
}

// A single standings request supplies both clubs and every other pregame
// carousel matchup. Date-specific URLs avoid the league's /now day lag.
bool loadSeasonStandings() {
  const String today = scoreboardDate.length() ? scoreboardDate : deviceLocalDate();
  const uint32_t now = millis();
  // League standings need not refresh on every score poll. During live NHL
  // games refresh at most every 3 minutes, otherwise every 15 minutes.
  bool anyLive = false;
  for (uint8_t i = 0; i < gameCount; ++i)
    if (games[i].state == "LIVE" || games[i].state == "CRIT") anyLive = true;
  const uint32_t ttl = anyLive ? 180000UL : 900000UL;
  if (standingsReady && standingsForDate == today &&
      uint32_t(now - standingsLastAttempt) < ttl) return true;
  if (!standingsReady && standingsLastAttempt &&
      uint32_t(now - standingsLastAttempt) < 60000UL) return false;
  if (WiFi.status() != WL_CONNECTED) return standingsReady;

  standingsLastAttempt = now;
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setConnectTimeout(4500);
  http.setTimeout(10000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.useHTTP10(true);
  const String url = "https://api-web.nhle.com/v1/standings/" +
                     (today.length() == 10 ? today : String("now"));
  if (!http.begin(client, url)) return standingsReady;
  http.addHeader("Accept-Encoding", "identity");
  if (http.GET() != HTTP_CODE_OK) {
    http.end();
    return standingsReady;
  }
  // These NHL fields are shared with the pregame preview. Allocate the
  // expanded filter on heap to protect the loopTask stack on the CYD.
  DynamicJsonDocument filter(1024);
  if (filter.capacity() < 1024) { http.end(); return standingsReady; }
  JsonObject first = filter["standings"][0].to<JsonObject>();
  first["teamAbbrev"]["default"] = true;
  first["wins"] = true;
  first["losses"] = true;
  first["otLosses"] = true;
  first["points"] = true;
  first["conferenceSequence"] = true;
  first["conferenceAbbrev"] = true;
  first["divisionAbbrev"] = true;
  first["divisionSequence"] = true;
  first["gamesPlayed"] = true;
  if (filter.overflowed()) {
    Serial.println("[NHL] Standings filter capacity exceeded");
    http.end();
    return standingsReady;
  }
  DynamicJsonDocument parsed(20480);
  if (parsed.capacity() < 20480) { http.end(); return standingsReady; }
  Stream &stream = http.getStream();
  stream.setTimeout(10000);
  const DeserializationError err = deserializeJson(
      parsed, stream, DeserializationOption::Filter(filter));
  http.end();
  if (err) {
    Serial.printf("Pregame standings JSON: %s\n", err.c_str());
    return standingsReady;
  }
  JsonArray teams = parsed["standings"].as<JsonArray>();
  if (teams.isNull()) return standingsReady;
  uint8_t loaded = 0;
  // 32 Arduino String-based records need not reside on the loopTask stack.
  // Standings requests are serialized on this single Arduino task.
  static SeasonStanding fresh[32];
  for (auto &entry : fresh) entry = SeasonStanding();
  for (JsonObject row : teams) {
    if (loaded >= 32) break;
    const String team = row["teamAbbrev"]["default"].as<String>();
    if (team.length() != 3 || row["wins"].isNull() ||
        row["losses"].isNull() || row["otLosses"].isNull()) continue;
    SeasonStanding &record = fresh[loaded++];
    record.team = team;
    record.wins = (uint8_t)constrain(row["wins"].as<int>(), 0, 255);
    record.losses = (uint8_t)constrain(row["losses"].as<int>(), 0, 255);
    record.otLosses = (uint8_t)constrain(row["otLosses"].as<int>(), 0, 255);
    record.gamesPlayed = (uint8_t)constrain((int)(row["gamesPlayed"] | 0), 0, 255);
    record.seasonRecord = String(record.wins) + "-" + String(record.losses) +
                          "-" + String(record.otLosses);
    if (!row["points"].isNull())
      record.points = String(row["points"].as<int>());
    const int rank = row["conferenceSequence"] | 0;
    record.conferenceSequence = (uint8_t)constrain(rank, 0, 255);
    record.divisionSequence =
        (uint8_t)constrain((int)(row["divisionSequence"] | 0), 0, 255);
    const String div = row["divisionAbbrev"].as<String>();
    const String conf = row["conferenceAbbrev"].as<String>();
    record.division = StandingsCarousel::groupFor(conf.c_str(), div.c_str());
    if (rank > 0) record.conferenceRank = "#" + String(rank);
  }
  if (loaded < 24) {
    // A truncated-but-parseable partial standings object must never overwrite
    // a good 32-team cache; it would make the table silently drop teams.
    Serial.printf("[NHL] Only %u valid standings teams; keeping cache\n", loaded);
    return standingsReady;
  }
  for (uint8_t i = 0; i < 32; ++i)
    seasonStandings[i] = i < loaded ? fresh[i] : SeasonStanding();
  standingsForDate = today;
  standingsReady = true;
  Serial.printf("Pregame standings loaded: %u teams\n", loaded);
  return true;
}

const SeasonStanding *standingFor(const String &code) {
  if (!standingsReady) return nullptr;
  for (const auto &standing : seasonStandings)
    if (standing.team == code) return &standing;
  return nullptr;
}

void refreshVisibleConferenceRecord(const Game &g) {
  if (detail.gameId != g.id || !detail.loaded || !detail.fetchedPregame) return;
  const uint32_t season = seasonStartYear(g.id);
  for (uint8_t i = 0; i < detail.count; ++i) {
    if (detail.label[i] != "VS CONF") continue;
    const String away = knownConferenceRecord(g.away, season);
    const String home = knownConferenceRecord(g.home, season);
    if (away != detail.away[i] || home != detail.home[i]) {
      detail.away[i] = away;
      detail.home[i] = home;
      if (screen == Screen::STATS && detailGameId == g.id) needsDraw = true;
    }
    break;
  }
}

// Fetch one team's regular-season results, filtered during HTTPS streaming so
// the ESP32 never loads the ~200kB raw JSON payload into RAM.
bool loadConferenceResults(ConferenceSnapshot &cache) {
  if (WiFi.status() != WL_CONNECTED) return false;
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(4400);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.useHTTP10(true);
  const String season = String(cache.season) + String(cache.season + 1);
  const String url = "https://api-web.nhle.com/v1/club-schedule-season/" +
                     cache.team + "/" + season;
  if (!http.begin(client, url)) return false;
  if (http.GET() != HTTP_CODE_OK) { http.end(); return false; }
  StaticJsonDocument<768> filter;
  JsonObject first = filter["games"][0].to<JsonObject>();
  first["gameType"] = true;
  first["gameState"] = true;
  first["homeTeam"]["abbrev"] = true;
  first["homeTeam"]["score"] = true;
  first["awayTeam"]["abbrev"] = true;
  first["awayTeam"]["score"] = true;
  first["gameOutcome"]["lastPeriodType"] = true;
  DynamicJsonDocument parsed(24576);
  const DeserializationError err = deserializeJson(
      parsed, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) {
    Serial.printf("Conference results %s: %s\n",
                  cache.team.c_str(), err.c_str());
    return false;
  }
  JsonArray schedule = parsed["games"].as<JsonArray>();
  if (schedule.isNull()) return false;
  ConferenceRecords::Record result;
  for (JsonObject match : schedule) {
    const String state = match["gameState"].as<String>();
    const bool final = state == "OFF" || state == "FINAL";
    if (!final || (match["gameType"] | 0) != 2) continue;
    const String home = match["homeTeam"]["abbrev"].as<String>();
    const String away = match["awayTeam"]["abbrev"].as<String>();
    const bool ourHome = home == cache.team;
    if (!ourHome && away != cache.team) continue;
    if (match["homeTeam"]["score"].isNull() ||
        match["awayTeam"]["score"].isNull()) continue;
    const int ownScore = ourHome ? match["homeTeam"]["score"].as<int>()
                                 : match["awayTeam"]["score"].as<int>();
    const int opponentScore = ourHome ? match["awayTeam"]["score"].as<int>()
                                      : match["homeTeam"]["score"].as<int>();
    const String lastPeriod = match["gameOutcome"]["lastPeriodType"].as<String>();
    const String opponent = ourHome ? away : home;
    ConferenceRecords::add(result, cache.team.c_str(), opponent.c_str(),
                           true, true, ownScore, opponentScore,
                           lastPeriod.c_str());
  }
  cache.record = String(result.wins) + "-" + String(result.losses) +
                 "-" + String(result.otLosses);
  cache.valid = true;
  Serial.printf("Conference record %s vs same conference: %s\n",
                cache.team.c_str(), cache.record.c_str());
  return true;
}

// On-demand and low priority: never issue the extra two schedule downloads
// during carousel rotation, goal effects, or WLED state restoration.
void tickPregameConferenceRecords() {
  if (!detail.loaded || !detail.fetchedPregame ||
      WiFi.status() != WL_CONNECTED) return;
  Game *g = nullptr;
  if (screen == Screen::STATS) {
    g = detailGame();
  } else if (webPreviewGameId && int32_t(webPreviewUntil - millis()) > 0) {
    for (uint8_t i = 0; i < gameCount; ++i)
      if (games[i].id == webPreviewGameId) { g = &games[i]; break; }
  }
  if (!g || detail.gameId != g->id ||
      (g->state != "FUT" && g->state != "PRE")) return;
  if (rearEvent == RearEvent::GOAL) return;
  for (uint8_t i = 0; i < MAX_WLED; ++i)
    if (restoring[i]) return;
  const uint32_t now = millis();
  if (uint32_t(now - lastConferenceFetch) < 1200UL) return;
  const uint32_t season = seasonStartYear(g->id);
  const String teams[] = {g->away, g->home};
  for (const String &team : teams) {
    if (ConferenceRecords::forTeam(team.c_str()) ==
        ConferenceRecords::Conference::UNKNOWN) continue;
    ConferenceSnapshot *c = conferenceSnapshot(team, season);
    if (!c) continue;
    if (c->tried && uint32_t(now - c->checkedAt) <
          (c->valid ? 900000UL : 120000UL)) continue;
    c->tried = true;
    c->checkedAt = now;
    lastConferenceFetch = now;
    loadConferenceResults(*c);
    refreshVisibleConferenceRecord(*g);
    break; // Only one optional HTTP request per loop turn.
  }
}

void fetchDetailStats(uint32_t id) {
  Game *g = nullptr;
  for (uint8_t i = 0; i < gameCount; ++i)
    if (games[i].id == id) { g = &games[i]; break; }
  if (!g) return;
  const bool pregame = GameDetailMode::isPregame(g->state.c_str());
  if (detail.gameId == id && detail.loaded &&
      detail.fetchedPregame == pregame) {
    const uint32_t ttl = pregame ? 180000UL :
          (g->state == "LIVE" || g->state == "CRIT") ? 30000UL : 180000UL;
    if (uint32_t(millis() - detail.updatedAt) < ttl) return;
  }
  detail.gameId = id;
  detail.fetchedPregame = pregame;
  detail.loaded = false;
  detail.count = 0;
  detail.error = "";
  if (WiFi.status() != WL_CONNECTED) {
    detail.error = "Wi-Fi unavailable";
    detail.loaded = true;
    detail.updatedAt = millis();
    return;
  }
  WiFiClientSecure client;
  client.setInsecure(); // Same trusted-read limitation as the scoreboard.
  HTTPClient http;
  http.setTimeout(4000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.useHTTP10(true);

  if (pregame) {
    // Matchup preview: league standings for season W-L-OT, conference rank
    // and points; conference *record* is computed separately from completed
    // same-conference games, never confused with conference rank.
    const bool standingsAvailable = loadSeasonStandings();
    const SeasonStanding *away = standingFor(g->away);
    const SeasonStanding *home = standingFor(g->home);
    const uint32_t season = seasonStartYear(g->id);
    addDetailRow("SEASON",
                 away ? away->seasonRecord : String("--"),
                 home ? home->seasonRecord : String("--"));
    addDetailRow("VS CONF",
                 knownConferenceRecord(g->away, season),
                 knownConferenceRecord(g->home, season));
    addDetailRow("CONF RANK",
                 away && away->conferenceRank.length() ? away->conferenceRank : String("--"),
                 home && home->conferenceRank.length() ? home->conferenceRank : String("--"));
    addDetailRow("POINTS",
                 away && away->points.length() ? away->points : String("--"),
                 home && home->points.length() ? home->points : String("--"));
    if (!standingsAvailable)
      detail.error = "Standings unavailable; retrying";
  } else {
    if (g->awayShots >= 0 && g->homeShots >= 0)
      addDetailRow("SHOTS", String(g->awayShots), String(g->homeShots));
    const String url = "https://api-web.nhle.com/v1/gamecenter/" +
                       String(id) + "/right-rail";
    if (http.begin(client, url) && http.GET() == HTTP_CODE_OK) {
      StaticJsonDocument<384> filter;
      JsonObject row = filter["teamGameStats"][0].to<JsonObject>();
      row["category"] = true;
      row["awayValue"] = true;
      row["homeValue"] = true;
      DynamicJsonDocument parsed(4096);
      DeserializationError err = deserializeJson(
        parsed, http.getStream(), DeserializationOption::Filter(filter));
      if (!err) {
        const struct { const char *id; const char *name; } targets[] = {
          {"faceoffWinningPctg", "FACEOFFS"},
          {"powerPlay", "POWER PLAY"},
          {"pim", "PEN. MIN"},
          {"hits", "HITS"},
          {"blockedShotAttempts", "BLOCKS"},
          {"giveaways", "GIVEAWAYS"},
          {"sog", "SHOTS"}
        };
        JsonArray values = parsed["teamGameStats"].as<JsonArray>();
        for (const auto &target : targets) {
          for (JsonObject row : values) {
            String category = row["category"].as<String>();
            if (!category.equalsIgnoreCase(target.id)) continue;
            if (detail.count >= 5) break;
            if (category == "sog" && g->awayShots >= 0) break;
            String awayValue = row["awayValue"].as<String>();
            String homeValue = row["homeValue"].as<String>();
            if (category.equalsIgnoreCase("faceoffWinningPctg")) {
              // NHL right-rail returns a 0..1 fraction, not a displayed percent.
              float aw = awayValue.toFloat();
              float hm = homeValue.toFloat();
              if (aw >= 0 && aw <= 1) aw *= 100.0f;
              if (hm >= 0 && hm <= 1) hm *= 100.0f;
              awayValue = String(aw, 1) + "%";
              homeValue = String(hm, 1) + "%";
            }
            addDetailRow(target.name, awayValue, homeValue);
            break;
          }
        }
      }
    }
  }
  http.end();
  if (!detail.count) detail.error = (g->state == "FUT" || g->state == "PRE")
      ? "Team records not yet available" : "Game stats not yet available";
  detail.loaded = true;
  detail.updatedAt = millis();
  for (uint8_t i = 0; i < gameCount; ++i) {
    if (games[i].id == id) {
      updateCarouselStatsFromDetail(i, detail);
      if (screen == Screen::LIVE && featuredIndex == i) partialLiveUpdate = true;
      break;
    }
  }
  if (screen == Screen::STATS) needsDraw = true;
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
  cfg.rearLedEnabled = prefs.getBool("rearOn", true);
  cfg.rearGoal = prefs.getBool("rearGoal", true);
  cfg.rearPowerPlay = prefs.getBool("rearPP", true);
  cfg.rearBrightness = prefs.getUChar("rearBri", 128);
  cfg.rearGoalSeconds = constrain(prefs.getUChar("rearDur", 6), 2, 12);
  for (uint8_t i = 0; i < MAX_FOLLOWED_GAMES; ++i)
    cfg.followIds[i] = prefs.getUInt(("follow" + String(i)).c_str(), 0);
  cfg.invertScreen = prefs.getBool("invert", true);
  cfg.cycleGames = prefs.getBool("carousel", true);
  cfg.showStandings = prefs.getBool("standPages", true);
  cfg.cycleSeconds = constrain(prefs.getUChar("cycleSec", 9), 5, 30);
  cfg.timezone = constrain(prefs.getUChar("timezone", 1), 0, 4);
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
  prefs.putBool("rearOn", cfg.rearLedEnabled);
  prefs.putBool("rearGoal", cfg.rearGoal);
  prefs.putBool("rearPP", cfg.rearPowerPlay);
  prefs.putUChar("rearBri", cfg.rearBrightness);
  prefs.putUChar("rearDur", cfg.rearGoalSeconds);
  for (uint8_t i = 0; i < MAX_FOLLOWED_GAMES; ++i)
    prefs.putUInt(("follow" + String(i)).c_str(), cfg.followIds[i]);
  prefs.putBool("invert", cfg.invertScreen);
  prefs.putBool("carousel", cfg.cycleGames);
  prefs.putBool("standPages", cfg.showStandings);
  prefs.putUChar("cycleSec", cfg.cycleSeconds);
  prefs.putUChar("timezone", cfg.timezone);
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

// Compact score/state fingerprint: redraw only when visible scoreboard values changed.
// Avoids repeated fillScreen() calls during polling while scores stay the same.
uint32_t gamesFingerprint() {
  uint32_t hash = 2166136261UL;
  for (uint8_t i = 0; i < gameCount; ++i) {
    const Game &g = games[i];
    hash = (hash ^ g.id) * 16777619UL;
    hash = (hash ^ uint32_t(g.homeScore)) * 16777619UL;
    hash = (hash ^ uint32_t(g.awayScore)) * 16777619UL;
    hash = (hash ^ uint32_t(g.homeShots + 1)) * 16777619UL;
    hash = (hash ^ uint32_t(g.awayShots + 1)) * 16777619UL;
    hash = (hash ^ uint32_t(g.period)) * 16777619UL;
    hash = (hash ^ uint32_t(g.intermission)) * 16777619UL;
    const String *fields[] = {&g.home, &g.away, &g.state, &g.clock,
                              &g.periodType, &g.startUTC};
    for (const String *str : fields) {
      for (size_t j = 0; j < str->length(); ++j)
        hash = (hash ^ uint8_t((*str)[j])) * 16777619UL;
    }
  }
  return (hash ^ gameCount) * 16777619UL;
}

// The NHL's cross-day scoreboard currently exposes PP 'situation' even when
// the date-scoped /score endpoint omits it. Use a small filtered parse, and
// only fetch for live games the user actually follows.
void fetchFollowedPowerPlays() {
  bool relevant = false;
  for (uint8_t i = 0; i < gameCount; ++i)
    if (rearFollows(games[i].id) &&
        (games[i].state == "LIVE" || games[i].state == "CRIT")) relevant = true;
  if (!relevant || WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(3500);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.useHTTP10(true);
  if (!http.begin(client, "https://api-web.nhle.com/v1/scoreboard/now"))
    return;
  if (http.GET() != HTTP_CODE_OK) { http.end(); return; }

  StaticJsonDocument<512> filter;
  JsonObject fg = filter["gamesByDate"][0]["games"][0].to<JsonObject>();
  fg["id"] = true;
  fg["situation"]["homeTeam"]["strength"] = true;
  fg["situation"]["awayTeam"]["strength"] = true;
  fg["situation"]["homeTeam"]["situationDescriptions"][0] = true;
  fg["situation"]["awayTeam"]["situationDescriptions"][0] = true;
  DynamicJsonDocument doc(24576);
  const DeserializationError err = deserializeJson(
    doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) { Serial.printf("Rear PP fallback JSON: %s\n", err.c_str()); return; }

  JsonArray dates = doc["gamesByDate"].as<JsonArray>();
  for (JsonObject date : dates) for (JsonObject entry : date["games"].as<JsonArray>()) {
    const uint32_t id = entry["id"] | 0U;
    for (uint8_t i = 0; i < gameCount; ++i) {
      Game &g = games[i];
      if (g.id != id || !rearFollows(id)) continue;
      JsonVariantConst situation = entry["situation"];
      const uint8_t side = detectPowerPlay(situation);
      g.ppSide = side;
      g.ppDataKnown = true; // Present in standings; absent situation == no PP.
      g.ppUpdatedAt = millis();
      ppCache[i].id = id;
      ppCache[i].side = side;
      ppCache[i].known = true;
      ppCache[i].updatedAt = g.ppUpdatedAt;
      break;
    }
  }
}

void markNhlFetchFailure(const String &message) {
  lastError = message;
  if (nhlConsecutiveFailures < 255) ++nhlConsecutiveFailures;
  Serial.printf("[NHL] %s | HTTP %d | expected=%dB | elapsed=%lums "
                "| failures=%u | freeHeap=%u\n",
                message.c_str(), nhlLastHttpCode, nhlLastContentLength,
                (unsigned long)nhlLastDurationMs, nhlConsecutiveFailures,
                (unsigned)ESP.getFreeHeap());
  if (gameCount == 0 && screen == Screen::LIVE) needsDraw = true;
}

void pollNhl() {
  // Watch free loopTask stack before the heavy HTTPS + JSON code.
  // This helped diagnose the v0.2.8 stack canary reboot.
  Serial.printf("[NHL] poll entry stackWatermark=%u freeHeap=%u\n",
                (unsigned)uxTaskGetStackHighWaterMark(nullptr),
                (unsigned)ESP.getFreeHeap());
  const uint32_t previousFingerprint = gamesFingerprint();
  if (WiFi.status() != WL_CONNECTED) return;
  const uint32_t requestStart = millis();
  nhlLastHttpCode = 0;
  nhlLastContentLength = -1;
  WiFiClientSecure client;
  // NHL's public endpoint; no credentials are transmitted. See SECURITY.md.
  client.setInsecure();
  client.setTimeout(NhlFetchPolicy::kResponseTimeoutMs);
  HTTPClient http;
  http.setConnectTimeout(NhlFetchPolicy::kConnectTimeoutMs);
  http.setTimeout(NhlFetchPolicy::kResponseTimeoutMs);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.useHTTP10(true); // Stream JSON without HTTP/1.1 chunk framing.
  const String requestedDate = deviceLocalDate();
  const String endpoint = "https://api-web.nhle.com/v1/score/" +
                          (requestedDate.length() ? requestedDate : String("now"));
  if (!http.begin(client, endpoint)) {
    nhlLastDurationMs = millis() - requestStart;
    markNhlFetchFailure("NHL connection setup failed");
    return;
  }
  // ESPN/NHL-style APIs sometimes gzip HTTP/1.1 responses. The CYD cannot
  // decompress them and ArduinoJson expects plain JSON.
  http.addHeader("Accept-Encoding", "identity");
  const int response = http.GET();
  nhlLastHttpCode = response;
  nhlLastContentLength = http.getSize();
  if (response != HTTP_CODE_OK) {
    nhlLastDurationMs = millis() - requestStart;
    http.end();
    markNhlFetchFailure("NHL HTTP " + String(response));
    return;
  }

  // Allocate the 2KB filter from HEAP, not the ESP32 loopTask stack.
  // With WiFiClientSecure + HTTPClient this pushed the default 8KB task
  // stack past its canary on some stock ESP32-2432S028R boards.
  DynamicJsonDocument filter(2048);
  if (filter.capacity() < 2048) {
    nhlLastDurationMs = millis() - requestStart;
    http.end();
    markNhlFetchFailure("NHL filter allocation failed");
    return;
  }
  filter["currentDate"] = true;
  JsonObject gameFilter = filter["games"][0].to<JsonObject>();
  gameFilter["id"] = true;
  gameFilter["gameState"] = true;
  gameFilter["startTimeUTC"] = true;
  gameFilter["periodDescriptor"]["number"] = true;
  gameFilter["periodDescriptor"]["periodType"] = true;
  gameFilter["clock"]["timeRemaining"] = true;
  gameFilter["clock"]["inIntermission"] = true;
  gameFilter["situation"]["homeTeam"]["strength"] = true;
  gameFilter["situation"]["awayTeam"]["strength"] = true;
  gameFilter["situation"]["homeTeam"]["situationDescriptions"][0] = true;
  gameFilter["situation"]["awayTeam"]["situationDescriptions"][0] = true;
  gameFilter["homeTeam"]["name"]["default"] = true;
  gameFilter["homeTeam"]["sog"] = true;
  gameFilter["awayTeam"]["name"]["default"] = true;
  gameFilter["awayTeam"]["sog"] = true;
  gameFilter["homeTeam"]["abbrev"] = true;
  gameFilter["homeTeam"]["score"] = true;
  gameFilter["awayTeam"]["abbrev"] = true;
  gameFilter["awayTeam"]["score"] = true;

  if (filter.overflowed()) {
    nhlLastDurationMs = millis() - requestStart;
    http.end();
    markNhlFetchFailure("NHL field filter exceeded memory");
    return;
  }
  // The NHL has up to 16 matchups on busy days. Keep the filtered document
  // large enough for the entire date's games without storing raw JSON twice.
  DynamicJsonDocument doc(24576);
  if (doc.capacity() < 24576) {
    nhlLastDurationMs = millis() - requestStart;
    http.end();
    markNhlFetchFailure("NHL JSON allocation failed");
    return;
  }
  // Key fix for IncompleteInput: ArduinoJson's Stream read defaults to 1s.
  // HTTP timeout alone is not sufficient for TLS gaps during large payloads.
  Stream &body = http.getStream();
  body.setTimeout(NhlFetchPolicy::kResponseTimeoutMs);
  const DeserializationError err = deserializeJson(
      doc, body, DeserializationOption::Filter(filter));
  nhlLastDurationMs = millis() - requestStart;
  http.end();
  if (err) {
    Serial.printf("[NHL] parser=%s, filterOverflow=%d, docOverflow=%d\n",
                  err.c_str(), filter.overflowed(), doc.overflowed());
    markNhlFetchFailure(String("NHL JSON: ") + err.c_str());
    return;
  }
  JsonArray arr = doc["games"].as<JsonArray>();
  if (arr.isNull()) {
    markNhlFetchFailure("NHL games unavailable");
    return;
  }
  const uint32_t previouslyFeatured = gameCount ?
      games[min((int)featuredIndex, (int)gameCount - 1)].id : 0;
  String newScoreboardDate = doc["currentDate"].as<String>();
  if (newScoreboardDate.length() != 10) {
    markNhlFetchFailure("Invalid NHL scoreboard date");
    return;
  }
  // Never replace today's schedule with stale /now data or a mismatched
  // redirect. A successful empty games[] still counts as today's schedule.
  if (requestedDate.length() && newScoreboardDate != requestedDate) {
    markNhlFetchFailure("NHL returned " + newScoreboardDate + ", expected " + requestedDate);
    return;
  }
  bool newDay = newScoreboardDate != scoreboardDate;
  scoreboardDate = newScoreboardDate;
  scoreboardLoaded = true;
  if (requestedDate.length()) targetScoreboardDate = requestedDate;
  gameCount = 0;
  for (JsonObject item : arr) {
    if (gameCount >= MAX_GAMES) break;
    Game &g = games[gameCount++];
    g.id = item["id"] | 0U;
    g.home = item["homeTeam"]["abbrev"].as<String>();
    g.away = item["awayTeam"]["abbrev"].as<String>();
    g.state = item["gameState"].as<String>();
    g.startUTC = item["startTimeUTC"].as<String>();
    g.homeName = item["homeTeam"]["name"]["default"].as<String>();
    g.awayName = item["awayTeam"]["name"]["default"].as<String>();
    g.periodType = item["periodDescriptor"]["periodType"].as<String>();
    g.period = item["periodDescriptor"]["number"] | 0;
    g.clock = item["clock"]["timeRemaining"].as<String>();
    g.intermission = item["clock"]["inIntermission"] | false;
    // Reset PP state every NHL poll before applying the fresh situation or
    // a bounded fallback snapshot. Never reuse stale values from Game[] RAM.
    g.ppSide = 0;
    g.ppDataKnown = false;
    g.ppUpdatedAt = 0;
    g.homeScore = item["homeTeam"]["score"] | 0;
    g.awayScore = item["awayTeam"]["score"] | 0;
    g.homeShots = item["homeTeam"]["sog"] | -1;
    g.awayShots = item["awayTeam"]["sog"] | -1;
    JsonVariantConst situation = item["situation"];
    if (!situation.isNull()) {
      g.ppSide = detectPowerPlay(situation);
      g.ppDataKnown = true;
      g.ppUpdatedAt = millis();
      ppCache[gameCount - 1].id = g.id;
      ppCache[gameCount - 1].side = g.ppSide;
      ppCache[gameCount - 1].known = true;
      ppCache[gameCount - 1].updatedAt = g.ppUpdatedAt;
    } else {
      const PowerPlaySnapshot &cached = ppCache[gameCount - 1];
      if (cached.id == g.id && cached.known &&
          uint32_t(millis() - cached.updatedAt) < 80000UL) {
        g.ppSide = cached.side;
        g.ppDataKnown = true;
        g.ppUpdatedAt = cached.updatedAt;
      }
    }
  }
  if (newDay) {
    featuredIndex = 0;
    lastCarouselAdvance = millis();
    needDetailLoad = false;
    detail.loaded = false;
    for (auto &entry : carouselStats) entry = CarouselStats();
    standingsReady = false;
    standingsLastAttempt = 0;
    standingsForDate = "";
    for (auto &entry : seasonStandings) entry = SeasonStanding();
    for (auto &entry : conferenceSnapshots) entry = ConferenceSnapshot();
    lastConferenceFetch = 0;
    if (screen == Screen::STATS) screen = Screen::LIVE;
  } else if (gameCount) {
    bool found = false;
    for (uint8_t i = 0; i < gameCount; ++i) {
      if (games[i].id == previouslyFeatured) {
        featuredIndex = i;
        found = true;
        break;
      }
    }
    if (!found) featuredIndex = 0;
  } else featuredIndex = 0;
  if (gamePage * 3 >= gameCount) gamePage = 0;
  // A valid full JSON document (including games: []) resets retry backoff.
  if (nhlConsecutiveFailures)
    Serial.printf("[NHL] recovered after %u failed fetches\n", nhlConsecutiveFailures);
  Serial.printf("[NHL] poll success stackWatermark=%u freeHeap=%u\n",
                (unsigned)uxTaskGetStackHighWaterMark(nullptr),
                (unsigned)ESP.getFreeHeap());
  nhlConsecutiveFailures = 0;
  nhlLastSuccessAt = millis();
  lastError = "";
  if (selectedGameId && !selectedGame()) {
    // Do not keep an old, invisible matchup armed after a calendar rollover.
    selectedGameId = 0;
    prefs.putUInt("game", 0);
    haveBaseline = false;
  }
  // A followed game is a game-ID from today's schedule, not a season-wide team.
  // Remove followers after a date change so old IDs cannot trigger new events.
  bool followedChanged = false;
  for (uint8_t i = 0; i < MAX_FOLLOWED_GAMES; ++i) {
    if (!cfg.followIds[i]) continue;
    bool present = false;
    for (uint8_t j = 0; j < gameCount; ++j)
      if (games[j].id == cfg.followIds[i]) present = true;
    if (!present) {
      cfg.followIds[i] = 0;
      followedChanged = true;
    }
  }
  if (followedChanged) saveSettings();
  detectRearGoalChanges();
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
  if (gamesFingerprint() != previousFingerprint || newDay) {
    if (!newDay && screen == Screen::LIVE && gameCount > 0 &&
        previouslyFeatured == games[featuredIndex % gameCount].id)
      partialLiveUpdate = true;
    else needsDraw = true;
  }
  // If puck drop occurs while the pregame detail page is open, replace
  // season/conference rows with the live shots, faceoffs, PP and hits view
  // promptly, without waiting for the previous pregame cache TTL.
  if (screen == Screen::STATS && detailGameId) {
    Game *viewed = detailGame();
    if (viewed) {
      const bool pregame = GameDetailMode::isPregame(viewed->state.c_str());
      if (detail.gameId == viewed->id &&
          GameDetailMode::needsModeSwitch(detail.loaded, detail.fetchedPregame,
                                           viewed->state.c_str())) {
        detail.loaded = false;
        detail.count = 0;
        needDetailLoad = true;
        needsDraw = true;
      } else if (!pregame &&
                 (viewed->state == "LIVE" || viewed->state == "CRIT") &&
                 uint32_t(millis() - detail.updatedAt) >= 30000UL) {
        needDetailLoad = true;
      }
    }
  }
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
// Paint only the changing score and clock regions on the 2.8" TFT.
// The logos, team labels and navigation remain intact between NHL polls.
// Broadcast board: deliberately lightweight raster/vector drawing for the CYD.
// Static arena is drawn on slide changes; NHL polls repaint only the readouts.
constexpr uint16_t ARENA_BLACK = 0x0000;
constexpr uint16_t ARENA_PLATE = 0x10A4; // dark slate-blue RGB565, not saturated blue
constexpr uint16_t ARENA_LINE = 0x422C;
constexpr uint16_t ICE_BLUE = 0x36BE;
constexpr uint16_t ICE_TEXT = 0xC6FA;

bool gameIsLive(const Game &g) { return g.state == "LIVE" || g.state == "CRIT"; }
bool gameIsFuture(const Game &g) { return g.state == "FUT" || g.state == "PRE"; }

String broadcastTime(const Game &g) {
  if (gameIsFuture(g)) return formattedStart(g.startUTC);
  if (!gameIsLive(g)) return gameStatus(g);
  String period = g.periodType == "SO" ? "SO" :
                  g.periodType == "OT" ? "OT" :
                  g.period == 1 ? "1ST" : g.period == 2 ? "2ND" :
                  g.period == 3 ? "3RD" : "P" + String(g.period);
  if (g.intermission) return "INT  |  " + period;
  return period + "  |  " + (g.clock.length() ? g.clock : String("LIVE"));
}

void drawArenaBackground() {
  tft.fillScreen(ARENA_BLACK);
  // Keep the arena dark on real ILI9341 CYDs. Previous blue components
  // climbed to 20/31, producing the vivid electric-blue photographed screen.
  for (int y = 25; y < 159; y += 5) {
    const uint16_t blue = y < 65 ? 1 : y < 112 ? 2 : 3;
    const uint16_t green = y < 112 ? 0 : 1;
    tft.fillRect(0, y, 320, 5, (green << 5) | blue);
  }
  // Dim perspective ice and centerline only; UI remains high-contrast.
  tft.fillRect(0, 150, 320, 47, 0x0863);
  tft.drawLine(0, 163, 160, 145, 0x1987);
  tft.drawLine(160, 145, 319, 163, 0x1987);
  tft.drawLine(160, 145, 160, 195, 0x3988);
  tft.drawEllipse(160, 175, 50, 15, 0x1987);
  for (int i = 0; i < 15; ++i) {
    const int x = 6 + i * 22;
    const int y = 26 + (i < 7 ? 7 - i : i - 7);
    tft.fillCircle(x, y, 1, 0x6C14);
  }
}

void drawScoreboardHeader(const Game &g) {
  tft.fillRect(0, 0, 320, 31, ARENA_BLACK);
  // Small NHL-like shield outline without using a heavyweight background image.
  tft.drawLine(7, 4, 37, 4, WHITE);
  tft.drawLine(7, 4, 7, 21, WHITE);
  tft.drawLine(37, 4, 37, 21, WHITE);
  tft.drawLine(7, 21, 22, 28, WHITE);
  tft.drawLine(22, 28, 37, 21, WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(WHITE, ARENA_BLACK);
  tft.drawString("NHL", 22, 14, 2);
  uint16_t pill = gameIsLive(g) ? 0xE800 :
                  gameIsFuture(g) ? 0x04F2 : 0x39C7;
  tft.fillRoundRect(48, 5, 67, 23, 5, pill);
  tft.setTextColor(WHITE, pill);
  tft.drawString(gameIsLive(g) ? "LIVE" :
                 gameIsFuture(g) ? "NEXT" : "FINAL", 81, 16, 2);
  if (gameIsLive(g)) tft.fillCircle(56, 16, 2, WHITE);
  tft.setTextColor(ICE_TEXT, ARENA_BLACK);
  tft.drawString(String(carouselSlide + 1) + "/" + String(carouselPageCount()), 228, 16, 2);
  tft.setTextColor(WiFi.status() == WL_CONNECTED ? ICE_BLUE : ORANGE, ARENA_BLACK);
  tft.drawString(WiFi.status() == WL_CONNECTED ? "WiFi" : "AP", 281, 16, 2);
  // Small menu glyph replaces the former three large navigation buttons.
  tft.drawFastHLine(305, 9, 10, WHITE);
  tft.drawFastHLine(305, 15, 10, WHITE);
  tft.drawFastHLine(305, 21, 10, WHITE);
  tft.setTextDatum(TL_DATUM);
}

// 320x240: 8 rank rows, full NHL team list across eight carousel slides.
// All drawing is simple TFT primitives; no image buffers or extra tasks.
void drawStandingsSlide() {
  const uint8_t slideNumber = carouselSlide - gameCount;
  const StandingsCarousel::Page page = StandingsCarousel::page(slideNumber);
  tft.fillScreen(ARENA_BLACK);
  tft.fillRect(0, 0, 320, 28, ARENA_PLATE);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(WHITE, ARENA_PLATE);
  tft.drawString("NHL STANDINGS", 8, 6, 2);
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(ICE_TEXT, ARENA_PLATE);
  tft.drawString(String(carouselSlide + 1) + "/" +
                 String(carouselPageCount()), 262, 7, 2);
  tft.drawString(WiFi.status() == WL_CONNECTED ? "WiFi" : "AP", 298, 7, 2);
  tft.drawFastHLine(306, 7, 11, WHITE);
  tft.drawFastHLine(306, 13, 11, WHITE);
  tft.drawFastHLine(306, 19, 11, WHITE);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(WHITE, ARENA_BLACK);
  tft.drawString(page.title, 160, 41, 4);
  tft.setTextColor(ICE_BLUE, ARENA_BLACK);
  tft.drawString(page.range, 160, 61, 2);

  tft.fillRect(5, 71, 310, 17, ARENA_LINE);
  tft.setTextColor(WHITE, ARENA_LINE);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("#", 18, 79, 2);
  tft.drawString("TEAM", 74, 79, 2);
  tft.drawString("GP", 145, 79, 2);
  tft.drawString("W-L-OT", 215, 79, 2);
  tft.drawString("PTS", 293, 79, 2);

  const String localDay = deviceLocalDate();
  if (!standingsReady || (localDay.length() && standingsForDate != localDay)) {
    tft.setTextColor(WHITE, ARENA_BLACK);
    tft.drawString("Loading NHL standings...", 160, 145, 2);
    tft.setTextColor(ICE_TEXT, ARENA_BLACK);
    tft.drawString("Updates automatically", 160, 175, 2);
  } else {
    uint8_t sorted[32] = {};
    const uint8_t n = StandingsCarousel::collectAndSort(
       seasonStandings, 32, page, sorted, 32);
    const Game *armed = selectedGame();
    for (uint8_t row = 0; row < StandingsCarousel::kRowsPerPage; ++row) {
      const uint8_t position = page.offset + row;
      if (position >= n) break;
      const SeasonStanding &entry = seasonStandings[sorted[position]];
      const uint16_t bg = row % 2 ? 0x1084 : 0x0822;
      const int16_t y = 90 + row * 17;
      tft.fillRect(5, y, 310, 17, bg);
      if (armed && (entry.team == armed->home ||
                    entry.team == armed->away))
        tft.fillRect(6, y, 3, 17, ICE_BLUE);
      tft.fillRect(41, y + 3, 4, 11, to565(teamRgb(entry.team)));
      tft.setTextColor(WHITE, bg);
      tft.drawString(String(StandingsCarousel::rank(
          page.group, entry.conferenceSequence, entry.divisionSequence)),
          20, y + 8, 2);
      tft.drawString(entry.team, 72, y + 8, 2);
      tft.setTextColor(ICE_TEXT, bg);
      tft.drawString(String(entry.gamesPlayed), 145, y + 8, 2);
      tft.drawString(entry.seasonRecord, 215, y + 8, 2);
      tft.setTextColor(WHITE, bg);
      tft.drawString(entry.points.length() ? entry.points : "--",
                     293, y + 8, 2);
    }
    if (n == 0) {
      tft.setTextColor(WHITE, ARENA_BLACK);
      tft.drawString("Standings unavailable", 160, 145, 2);
    }
  }

  tft.fillRect(0, 229, 320, 11, ARENA_PLATE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(ICE_TEXT, ARENA_PLATE);
  tft.drawString("Swipe for games / more rankings", 160, 234, 1);
  tft.setTextDatum(TL_DATUM);
}

void drawScoreboardStatic(const Game &g) {
  drawArenaBackground();
  drawScoreboardHeader(g);
  // Scoreboard clock / status banner.
  tft.fillRoundRect(71, 34, 178, 37, 6, ARENA_PLATE);
  tft.drawRoundRect(71, 34, 178, 37, 6, ARENA_LINE);
  // A central opaque score plate makes partial refreshes clean and flicker-free.
  tft.fillRoundRect(78, 77, 164, 66, 7, ARENA_PLATE);
  // Official alpha-keyed 56x56 logo sprites: rink remains visible around marks.
  drawTeamLogo(tft, g.away, 13, 79);
  drawTeamLogo(tft, g.home, 251, 79);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(WHITE);
  tft.drawString(g.away, 41, 145, 4);
  tft.drawString(g.home, 279, 145, 4);
  tft.drawFastHLine(13, 160, 57, to565(teamRgb(g.away)));
  tft.drawFastHLine(250, 160, 57, to565(teamRgb(g.home)));
  tft.setTextDatum(TL_DATUM);
  // Bottom information plates cover the lower strip of the stylized rink.
  tft.fillRoundRect(7, 166, 306, 30, 6, ARENA_PLATE);
  tft.drawRoundRect(7, 166, 306, 30, 6, ARENA_LINE);
  tft.drawFastVLine(160, 171, 21, ARENA_LINE);
  tft.drawFastHLine(11, 167, 68, to565(teamRgb(g.away)));
  tft.drawFastHLine(241, 167, 68, to565(teamRgb(g.home)));
  tft.fillRoundRect(7, 201, 306, 33, 6, ARENA_PLATE);
  tft.drawRoundRect(7, 201, 306, 33, 6, ARENA_LINE);
  tft.drawFastVLine(108, 206, 22, ARENA_LINE);
  tft.drawFastVLine(211, 206, 22, ARENA_LINE);
}

void drawLiveReadout(const Game &g) {
  // This function updates the *same game* without erasing/redrawing logos.
  tft.fillRoundRect(73, 36, 174, 33, 5, ARENA_PLATE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(WHITE, ARENA_PLATE);
  const String status = broadcastTime(g);
  tft.drawString(status, 160, 52, status.length() > 15 ? 2 : 4);

  tft.fillRoundRect(80, 79, 160, 62, 6, ARENA_PLATE);
  if (gameIsFuture(g)) {
    tft.setTextColor(WHITE, ARENA_PLATE);
    tft.drawString("VS", 160, 110, 4);
  } else {
    tft.setTextColor(WHITE, ARENA_PLATE);
    tft.setTextSize(2);
    tft.drawString(String(g.awayScore), 130, 108, 4);
    tft.drawString(String(g.homeScore), 190, 108, 4);
    tft.setTextSize(1);
    tft.drawFastVLine(160, 88, 42, ARENA_LINE);
  }

  tft.fillRect(10, 169, 300, 25, ARENA_PLATE);
  tft.setTextColor(ICE_TEXT, ARENA_PLATE);
  tft.drawString("SOG", 37, 181, 2);
  tft.drawString("SOG", 203, 181, 2);
  tft.setTextColor(WHITE, ARENA_PLATE);
  tft.drawString(g.awayShots < 0 ? "--" : String(g.awayShots), 111, 181, 4);
  tft.drawString(g.homeShots < 0 ? "--" : String(g.homeShots), 280, 181, 4);

  // Automatic per-game cache: the viewer never has to visit STATS first.
  // When the NHL has not published a value we show "--", not navigation text
  // or fabricated game statistics.
  String footerLeft = "PP --", footerMid = "PIM --", footerRight = "FO --";
  const CarouselStats &cached = statsFor(featuredIndex % gameCount);
  if (gameIsFuture(g)) {
    // Fit both clubs' season records into separate cells. The old combined
    // "REC 11-2-2|9-5-0" overflowed a single cell on the 320px TFT.
    footerLeft = cached.recordAway.length() ? cached.recordAway : "--";
    footerMid = "W-L-OT";
    footerRight = cached.recordHome.length() ? cached.recordHome : "--";
  } else {
    if (cached.ppAway.length() && cached.ppHome.length())
      footerLeft = "PP " + cached.ppAway + "|" + cached.ppHome;
    if (cached.pimAway.length() && cached.pimHome.length())
      footerMid = "PIM " + cached.pimAway + "-" + cached.pimHome;
    if (cached.faceAway.length() && cached.faceHome.length()) {
      // One decimal place is useful on the detail page, but the lower 100px
      // cell is more readable with rounded whole percentages.
      const int awayPct = (int)(cached.faceAway.toFloat() + 0.5f);
      const int homePct = (int)(cached.faceHome.toFloat() + 0.5f);
      footerRight = "FO " + String(awayPct) + "-" + String(homePct) + "%";
    }
  }
  tft.fillRect(10, 204, 298, 27, ARENA_PLATE);
  tft.setTextColor(ICE_TEXT, ARENA_PLATE);
  tft.drawString(footerLeft, 58, 217, footerLeft.length() > 10 ? 1 : 2);
  tft.drawString(footerMid, 160, 217, footerMid.length() > 11 ? 1 : 2);
  tft.drawString(footerRight, 262, 217, footerRight.length() > 10 ? 1 : 2);
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
  if (screen == Screen::LIVE) {
    if (showingStandingsPage()) {
      drawStandingsSlide();
    } else if (!gameCount) {
      tft.fillScreen(ARENA_BLACK);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(WHITE, ARENA_BLACK);
      tft.drawString("NHL GAMEDAY", 160, 46, 4);
      const String message = lastError.length() ? lastError
          : scoreboardLoaded ? String("No NHL games scheduled")
          : String("Loading today's games...");
      tft.drawString(message, 160, 108, 2);
      tft.setTextColor(ICE_TEXT, ARENA_BLACK);
      tft.drawString(targetScoreboardDate.length() ? targetScoreboardDate : scoreboardDate, 160, 144, 2);
      tft.drawString("Tap top-right for the menu", 160, 193, 2);
      tft.setTextDatum(TL_DATUM);
    } else {
      const Game &g = games[featuredIndex % gameCount];
      drawScoreboardStatic(g);
      drawLiveReadout(g);
    }
    // No persistent TODAY/GAMES/SETUP buttons on the carousel.
    return;
  }

  tft.fillScreen(BG);
  tft.fillRect(0, 0, 320, 33, PANEL);
  tft.setTextColor(WHITE, PANEL);
  tft.drawString("GLOWDECK / NHL", 8, 8, 2);
  tft.setTextDatum(TR_DATUM);
  tft.drawString(WiFi.status() == WL_CONNECTED ? "ONLINE" : "SETUP AP", 310, 9, 2);
  tft.setTextDatum(TL_DATUM);

  if (screen == Screen::STATS) {
    Game *g = detailGame();
    tft.fillRect(0, 33, 320, 165, ARENA_BLACK);
    tft.fillRoundRect(6, 38, 308, 154, 8, ARENA_PLATE);
    tft.drawRoundRect(6, 38, 308, 154, 8, ARENA_LINE);
    if (!g) {
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(WHITE, ARENA_PLATE);
      tft.drawString("Game unavailable", 160, 103, 2);
      tft.setTextDatum(TL_DATUM);
    } else {
      drawTeamLogo(tft, g->away, 15, 42);
      drawTeamLogo(tft, g->home, 249, 42);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(WHITE, ARENA_PLATE);
      tft.drawString(gameIsFuture(*g) ? "VS" :
                     String(g->awayScore) + " - " + String(g->homeScore),
                     160, 59, 4);
      tft.setTextColor(ICE_TEXT, ARENA_PLATE);
      tft.drawString(gameStatus(*g), 160, 87, 2);
      tft.setTextColor(WHITE, ARENA_PLATE);
      tft.drawString(g->away, 43, 108, 2);
      tft.drawString(g->home, 277, 108, 2);
      tft.drawFastHLine(92, 106, 136, ARENA_LINE);
      if (detail.loaded && detail.gameId == g->id && detail.count) {
        for (uint8_t i = 0; i < detail.count; ++i) {
          const int16_t y = 123 + i * 14;
          tft.setTextColor(WHITE, ARENA_PLATE);
          tft.drawString(detail.away[i], 52, y, 2);
          tft.drawString(detail.home[i], 268, y, 2);
          tft.setTextColor(ICE_TEXT, ARENA_PLATE);
          tft.drawString(detail.label[i], 160, y, 2);
        }
      } else {
        tft.setTextColor(ICE_TEXT, ARENA_PLATE);
        tft.drawString(detail.loaded ? detail.error : "Loading team stats...",
                       160, 148, 2);
      }
      tft.setTextDatum(TL_DATUM);
    }
  } else if (screen == Screen::GAMES) {
    textAt("Select a goal-alert game", 12, 39, WHITE, 2);
    if (!gameCount) {
      textAt(!lastError.isEmpty() ? lastError
             : scoreboardLoaded ? "No NHL games today" : "Loading games...", 12, 81, ORANGE);
      textAt(targetScoreboardDate.length() ? targetScoreboardDate :
             WiFi.status() == WL_CONNECTED ? "NHL connection required" : "Wi-Fi connection required",
             12, 105, MUTED);
    }
    for (uint8_t n = 0; n < 3; ++n) {
      uint8_t i = gamePage * 3 + n;
      if (i >= gameCount) break;
      Game &g = games[i];
      button(8, 66 + n * 40, 304, 36,
             g.away + " " + String(g.awayScore) + "  @  " +
             g.home + " " + String(g.homeScore) + "  " +
             (g.id == selectedGameId ? "* ARMED" : g.state),
             g.id == selectedGameId);
    }
    if (gameCount > 3) {
      tft.setTextDatum(TR_DATUM);
      tft.setTextColor(MUTED, BG);
      tft.drawString("< PREV         NEXT >", 309, 40, 2);
      tft.setTextDatum(TL_DATUM);
    }
  } else if (screen == Screen::SETUP) {
    textAt("Web setup", 12, 43, WHITE, 4);
    textAt("Network: " + (cfg.ssid.isEmpty() ? String("Not configured") : cfg.ssid), 12, 76, MUTED);
    textAt("IP: " + localAddress(), 12, 97, MUTED);
    textAt("AP: " + apSsid, 12, 118, MUTED);
    textAt("AP password: " + apPassword, 12, 138, WHITE);
    button(8, 161, 98, 29, "TEST");
    button(111, 161, 98, 29, "TOUCH");
    button(214, 161, 98, 29, "INVERT");
  }

  if (screen == Screen::STATS) {
    button(6, 200, 99, 34, "BACK");
    button(111, 200, 99, 34, detailGameId == selectedGameId ? "ARMED" : "ARM");
    button(216, 200, 98, 34, "SETUP");
  } else {
    button(6, 200, 99, 34, "TODAY", screen == Screen::LIVE);
    button(111, 200, 99, 34, "GAMES", screen == Screen::GAMES);
    button(216, 200, 98, 34, "SETUP", screen == Screen::SETUP);
  }
}

void selectGame(uint32_t id) {
  swallowLiveGesture = true;
  selectedGameId = id;
  haveBaseline = false;
  prefs.putUInt("game", id);
  screen = Screen::LIVE;
  for (uint8_t i = 0; i < gameCount; ++i)
    if (games[i].id == id) { featuredIndex = i; break; }
  lastCarouselAdvance = millis();
  // Fetch soon when the user arms a new matchup; first response is a baseline.
  lastPoll = millis() - GameDayCadence::kIdleMs;
  needsDraw = true;
}

void handleTouch() {
  const bool pressed = touch.touched();
  // On the full-screen broadcast view, gestures are recognized on finger lift:
  // swipe left/right to change games, tap a matchup for stats, tap top-right
  // menu glyph to open GAMES (which provides SETUP in its footer).
  if (screen == Screen::LIVE) {
    if (swallowLiveGesture) {
      if (!pressed) { swallowLiveGesture = false; touchWasDown = false; }
      return;
    }
    if (pressed) {
      TS_Point p = touch.getPoint();
      const int16_t x = constrain((int)map(p.x, cfg.touchX0, cfg.touchX1, 0, 319), 0, 319);
      const int16_t y = constrain((int)map(p.y, cfg.touchY0, cfg.touchY1, 0, 239), 0, 239);
      if (!touchWasDown) {
        touchWasDown = true;
        gestureStartX = x;
        gestureStartY = y;
      }
      gestureEndX = x;
      gestureEndY = y;
    } else if (touchWasDown) {
      touchWasDown = false;
      const int dx = gestureEndX - gestureStartX;
      const int dy = gestureEndY - gestureStartY;
      if (gameCount > 1 && abs(dx) > 48 && abs(dy) < 90) {
        if (dx < 0) featuredIndex = (featuredIndex + 1) % gameCount;
        else featuredIndex = (featuredIndex + gameCount - 1) % gameCount;
      } else if (gestureStartY <= 35 && gestureStartX >= 263) {
        screen = Screen::GAMES; // Access setup and arming from existing menu.
      } else if (gameCount > 1 && gestureStartY >= 213 && gestureStartX < 58) {
        featuredIndex = (featuredIndex + gameCount - 1) % gameCount;
      } else if (gameCount > 1 && gestureStartY >= 213 && gestureStartX > 263) {
        featuredIndex = (featuredIndex + 1) % gameCount;
      } else if (gameCount) {
        showDetail(games[featuredIndex % gameCount].id);
      } else {
        screen = Screen::GAMES;
      }
      lastCarouselAdvance = millis();
      needsDraw = true;
    }
    return;
  }
  // Other screens use one action per press, including the 2-point calibrator.

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
    if (screen == Screen::STATS) {
      if (x < 107) screen = Screen::LIVE;
      else if (x < 214) selectGame(detailGameId);
      else screen = Screen::SETUP;
    } else {
      screen = x < 107 ? Screen::LIVE : x < 214 ? Screen::GAMES : Screen::SETUP;
    }
    if (screen == Screen::LIVE) swallowLiveGesture = true;
    lastCarouselAdvance = millis();
  } else if (screen == Screen::GAMES) {
    if (y >= 39 && y < 64 && gameCount > 3) {
      if (x > 170 && (gamePage + 1) * 3 < gameCount) ++gamePage;
      else if (x <= 170 && gamePage) --gamePage;
    } else if (y >= 66 && y < 186) {
      uint8_t index = gamePage * 3 + (y - 66) / 40;
      if (index < gameCount) selectGame(games[index].id);
    }
  } else if (screen == Screen::SETUP && y >= 160 && y < 193) {
    if (x < 107) {
      Game *g = selectedGame();
      celebrate(teamRgb(g ? g->home : String("UTA")));
    } else if (x < 213) {
      screen = Screen::CALIBRATE;
      calStep = 0;
    } else {
      cfg.invertScreen = !cfg.invertScreen;
      tft.invertDisplay(cfg.invertScreen);
      prefs.putBool("invert", cfg.invertScreen);
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
    StaticJsonDocument<1536> doc;
    doc["version"] = "0.2.9";
    doc["wifiConnected"] = WiFi.status() == WL_CONNECTED;
    doc["ip"] = localAddress();
    doc["apSsid"] = apSsid;
    doc["apIp"] = WiFi.softAPIP().toString();
    doc["mdns"] = mdnsStarted ? mdnsName + ".local" : "";
    doc["selectedId"] = selectedGameId;
    doc["mode"] = cfg.mode;
    doc["rearEnabled"] = cfg.rearLedEnabled;
    doc["rearEvent"] = rearEvent == RearEvent::GOAL ? "GOAL" :
                       rearEvent == RearEvent::POWER_PLAY ? "POWER_PLAY" : "IDLE";
    JsonArray followed = doc.createNestedArray("followed");
    for (uint8_t i = 0; i < MAX_FOLLOWED_GAMES; ++i)
      if (cfg.followIds[i]) followed.add(cfg.followIds[i]);
    doc["scoreDate"] = scoreboardDate;
    doc["today"] = targetScoreboardDate;
    doc["clockSynced"] = deviceLocalDate().length() == 10;
    doc["scoreboardLoaded"] = scoreboardLoaded;
    doc["featuredIndex"] = featuredIndex;
    doc["cycleGames"] = cfg.cycleGames;
    doc["cycleSeconds"] = cfg.cycleSeconds;
    doc["scorePollSeconds"] = NhlFetchPolicy::pollIntervalMs(
      currentScoreboardPollIntervalMs(), nhlConsecutiveFailures) / 1000UL;
    doc["nhlFailures"] = nhlConsecutiveFailures;
    doc["nhlLastHttp"] = nhlLastHttpCode;
    doc["nhlExpectedBytes"] = nhlLastContentLength;
    doc["nhlLastDurationMs"] = nhlLastDurationMs;
    doc["nhlLastSuccessAgeSeconds"] = nhlLastSuccessAt
        ? (int32_t)((millis() - nhlLastSuccessAt) / 1000UL) : -1;
    doc["error"] = lastError;
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["stackLowWater"] = uxTaskGetStackHighWaterMark(nullptr);
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
      g["status"] = gameStatus(games[i]);
      g["homeShots"] = games[i].homeShots;
      g["awayShots"] = games[i].awayShots;
      g["startTimeUTC"] = games[i].startUTC;
      g["period"] = games[i].period;
      g["periodType"] = games[i].periodType;
      g["clock"] = games[i].clock;
      g["intermission"] = games[i].intermission;
      g["powerPlaySide"] = games[i].ppSide;
      g["followed"] = rearFollows(games[i].id);
    }
    sendJson(doc);
  });
  server.on("/api/stats", HTTP_GET, []() {
    if (!authorized()) return;
    const uint32_t id = strtoul(server.arg("id").c_str(), nullptr, 10);
    Game *requested = nullptr;
    for (uint8_t i = 0; i < gameCount; ++i)
      if (games[i].id == id) { requested = &games[i]; break; }
    if (!requested || id == 0)
      return errorResponse("Game is not in the daily scoreboard", 404);
    if (GameDetailMode::isPregame(requested->state.c_str())) {
      webPreviewGameId = id;
      webPreviewUntil = millis() + 25000UL;
    } else {
      webPreviewGameId = 0;
    }
    fetchDetailStats(id);
    StaticJsonDocument<2048> doc;
    doc["gameId"] = id;
    doc["pregame"] = GameDetailMode::isPregame(requested->state.c_str());
    doc["error"] = detail.error;
    bool conferenceLoading = false;
    if (doc["pregame"].as<bool>()) {
      for (uint8_t i = 0; i < detail.count; ++i)
        if (detail.label[i] == "VS CONF" &&
            (detail.away[i] == "--" || detail.home[i] == "--"))
          conferenceLoading = true;
    }
    doc["conferenceLoading"] = conferenceLoading;
    JsonArray rows = doc.createNestedArray("stats");
    for (uint8_t i = 0; i < detail.count; ++i) {
      JsonObject row = rows.createNestedObject();
      row["label"] = detail.label[i];
      row["away"] = detail.away[i];
      row["home"] = detail.home[i];
    }
    sendJson(doc);
  });
  server.on("/api/refresh", HTTP_POST, []() {
    if (!authorized()) return;
    lastPoll = millis() - GameDayCadence::kIdleMs;
    StaticJsonDocument<96> ok;
    ok["queued"] = true;
    sendJson(ok);
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
    doc["rearEnabled"] = cfg.rearLedEnabled;
    doc["rearGoal"] = cfg.rearGoal;
    doc["rearPowerPlay"] = cfg.rearPowerPlay;
    doc["rearBrightness"] = cfg.rearBrightness;
    doc["rearGoalSeconds"] = cfg.rearGoalSeconds;
    doc["screenInverted"] = cfg.invertScreen;
    doc["cycleGames"] = cfg.cycleGames;
    doc["cycleSeconds"] = cfg.cycleSeconds;
    doc["timezone"] = cfg.timezone;
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
    cfg.rearLedEnabled = doc["rearEnabled"] | cfg.rearLedEnabled;
    cfg.rearGoal = doc["rearGoal"] | cfg.rearGoal;
    cfg.rearPowerPlay = doc["rearPowerPlay"] | cfg.rearPowerPlay;
    cfg.rearBrightness = constrain((int)(doc["rearBrightness"] | cfg.rearBrightness), 1, 255);
    cfg.rearGoalSeconds = constrain((int)(doc["rearGoalSeconds"] | cfg.rearGoalSeconds), 2, 12);
    if (!cfg.rearLedEnabled) stopRearLed();
    cfg.cycleGames = doc["cycleGames"] | cfg.cycleGames;
    cfg.cycleSeconds = constrain((int)(doc["cycleSeconds"] | cfg.cycleSeconds), 5, 30);
    uint8_t requestedTimezone = constrain((int)(doc["timezone"] | cfg.timezone), 0, 4);
    if (requestedTimezone != cfg.timezone) {
      cfg.timezone = requestedTimezone;
      applyTimezone();
      // A new timezone can change what 'today' means on this CYD.
      lastDateCheck = millis() - 5000UL;
      lastPoll = millis() - GameDayCadence::kIdleMs;
      needsDraw = true;
    }
    bool invert = doc["screenInverted"] | cfg.invertScreen;
    if (cfg.invertScreen != invert) {
      cfg.invertScreen = invert;
      tft.invertDisplay(cfg.invertScreen);
      needsDraw = true;
    }
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
  server.on("/api/rear/follow", HTTP_POST, []() {
    if (!authorized()) return;
    StaticJsonDocument<160> doc;
    if (deserializeJson(doc, server.arg("plain"))) return errorResponse("Invalid JSON");
    const uint32_t id = doc["id"] | 0U;
    bool exists = false;
    for (uint8_t i = 0; i < gameCount; ++i)
      if (games[i].id == id) exists = true;
    if (!id || !exists) return errorResponse("Choose a game on today's schedule", 404);
    const bool followed = doc["follow"] | false;
    if (followed) {
      bool present = false;
      for (uint8_t i = 0; i < MAX_FOLLOWED_GAMES; ++i)
        if (cfg.followIds[i] == id) present = true;
      if (!present) {
        bool added = false;
        for (uint8_t i = 0; i < MAX_FOLLOWED_GAMES; ++i) {
          if (cfg.followIds[i] == 0) { cfg.followIds[i] = id; added = true; break; }
        }
        if (!added) return errorResponse("Up to 4 additional followed games", 409);
      }
    } else {
      for (uint8_t i = 0; i < MAX_FOLLOWED_GAMES; ++i)
        if (cfg.followIds[i] == id) cfg.followIds[i] = 0;
    }
    saveSettings();
    StaticJsonDocument<96> result; result["ok"] = true; sendJson(result);
  });
  server.on("/api/rear/test", HTTP_POST, []() {
    if (!authorized()) return;
    StaticJsonDocument<160> doc;
    if (deserializeJson(doc, server.arg("plain"))) return errorResponse("Invalid JSON");
    const String mode = doc["mode"] | String("goal");
    const String side = doc["side"] | String("home");
    if (side != "home" && side != "away") return errorResponse("Invalid team side");
    if (mode != "goal" && mode != "powerplay" && mode != "off")
      return errorResponse("Invalid effect");
    if (mode != "off" && !cfg.rearLedEnabled)
      return errorResponse("Enable the rear LED and save settings first");
    Game *g = selectedGame();
    if (!g && gameCount) g = &games[featuredIndex % gameCount];
    const uint32_t color = teamRgb(g ? (side == "home" ? g->home : g->away)
                                      : String("UTA"));
    if (mode == "off") stopRearLed();
    else if (mode == "goal") {
      rearTestPpUntil = 0;
      rearGoalColor = color;
      rearGoalAt = millis();
      rearGoalDuration = uint32_t(cfg.rearGoalSeconds) * 1000UL;
      rearEvent = RearEvent::GOAL;
    } else {
      rearGoalDuration = 0;
      rearTestPpColor = color;
      rearTestPpUntil = millis() + 9000UL;
      rearEvent = RearEvent::IDLE;
    }
    StaticJsonDocument<96> result; result["ok"] = true; sendJson(result);
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
  Serial.println("\nGlowDeck GameDay v0.2.9 — CYD only");
  Serial.printf("Configured loopTask stack: %u bytes\n", ARDUINO_LOOP_STACK_SIZE);
  Serial.printf("Setup stack watermark: %u, freeHeap: %u\n",
                (unsigned)uxTaskGetStackHighWaterMark(nullptr),
                (unsigned)ESP.getFreeHeap());

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
  setupRearLed();

  char suffix[7];
  snprintf(suffix, sizeof(suffix), "%06X", (unsigned)(ESP.getEfuseMac() & 0xFFFFFF));
  apSsid = "GlowDeck-" + String(suffix);
  apPassword = "GD" + String(suffix) + "99";
  mdnsName = "glowdeck-" + String(suffix);
  mdnsName.toLowerCase();
  loadSettings();
  // Some ILI9341-compatible CYDs boot with visually inverted colors.
  // Runtime setting is stored in NVS, so the user can change it from web or touch.
  tft.invertDisplay(cfg.invertScreen);
  applyTimezone();

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
  tickRearLed();

  const uint32_t now = millis();
  const bool connected = WiFi.status() == WL_CONNECTED;
  if (connected && !wifiWasConnected) {
    wifiWasConnected = true;
    mdnsAttempted = false;
    // SNTP runs asynchronously; neither boot nor web dashboard waits for it.
    configTzTime(timezoneRule(), "pool.ntp.org", "time.nist.gov");
    lastDateCheck = millis() - 5000UL;
    lastPoll = millis() - GameDayCadence::kIdleMs;
    needsDraw = true;
  } else if (!connected && wifiWasConnected) {
    wifiWasConnected = false;
    mdnsStarted = false;
    mdnsAttempted = false;
    needsDraw = true;
  }
  // Independent of NHL polling, check calendar rollover even with an
  // unarmed game, an empty schedule, or while Wi-Fi is temporarily offline.
  if (uint32_t(now - lastDateCheck) >= 5000UL) {
    lastDateCheck = now;
    const String today = deviceLocalDate();
    if (today.length()) prepareNewScoreboardDate(today);
  }
  if (connected) {
    if (!mdnsAttempted) {
      mdnsAttempted = true;
      mdnsStarted = MDNS.begin(mdnsName.c_str());
      if (mdnsStarted) MDNS.addService("http", "tcp", 80);
      needsDraw = true;
      Serial.printf("STA: http://%s/\n", WiFi.localIP().toString().c_str());
    }
    // 10s: live + armed/followed, 20s: other live, 30s: armed pregame,
    // 60s: idle. A single fetch refreshes the entire daily carousel.
    const uint32_t interval = NhlFetchPolicy::pollIntervalMs(
      currentScoreboardPollIntervalMs(), nhlConsecutiveFailures);
    // An interrupted HTTPS stream can block until its timeout. Never start
    // one so close to a WLED snapshot restoration that the lights remain in
    // celebration mode while the ESP32 waits for NHL bytes.
    bool restoreSoon = false;
    for (uint8_t i = 0; i < MAX_WLED; ++i)
      if (restoring[i] &&
          int32_t(restoreAt[i] - now) <=
             int32_t(NhlFetchPolicy::kResponseTimeoutMs + 500UL))
        restoreSoon = true;
    if (!restoreSoon && uint32_t(now - lastPoll) >= interval) {
      lastPoll = now;
      pollNhl();
    }
  } else if (!cfg.ssid.isEmpty() && now - reconnectAt > 20000) {
    WiFi.reconnect();
    reconnectAt = now;
    needsDraw = true;
  }
  // PP is event-driven by real NHL manpower data, not by cumulative PP stats.
  // Poll only while relevant and never while WLED state restoration is pending.
  if (connected && cfg.rearLedEnabled && cfg.rearPowerPlay &&
      uint32_t(now - lastPpFallback) >= 20000UL) {
    bool restoringLights = false, needSituation = false;
    for (uint8_t i = 0; i < MAX_WLED; ++i)
      if (restoring[i]) restoringLights = true;
    for (uint8_t i = 0; i < gameCount; ++i)
      if (rearFollows(games[i].id) && (games[i].state == "LIVE" ||
           games[i].state == "CRIT") && !games[i].intermission &&
           (!games[i].ppDataKnown ||
            uint32_t(now - games[i].ppUpdatedAt) >= 20000UL)) needSituation = true;
    if (needSituation && !restoringLights && rearEvent != RearEvent::GOAL) {
      lastPpFallback = millis();
      fetchFollowedPowerPlays();
    }
  }
  if (screen == Screen::LIVE && cfg.cycleGames && gameCount > 1 &&
      now - lastCarouselAdvance >= uint32_t(cfg.cycleSeconds) * 1000UL) {
    featuredIndex = (featuredIndex + 1) % gameCount;
    lastCarouselAdvance = now;
    needsDraw = true;
  }
  if (needDetailLoad && screen == Screen::STATS) {
    needDetailLoad = false;
    fetchDetailStats(detailGameId);
  }
  // Prefetch stats for the displayed carousel game; the background request
  // happens only once per game/TTL, never once per frame. Defer requests
  // while a WLED restoration is pending to avoid delaying light recovery.
  if (screen == Screen::LIVE && gameCount && connected &&
      uint32_t(now - lastCarouselStatsCheck) >= 2500UL) {
    bool lightRestorePending = false;
    for (uint8_t i = 0; i < MAX_WLED; ++i)
      if (restoring[i]) lightRestorePending = true;
    const uint8_t currentIndex = featuredIndex % gameCount;
    if (!lightRestorePending &&
        !statsAreFresh(games[currentIndex], statsFor(currentIndex))) {
      lastCarouselStatsCheck = millis();
      fetchDetailStats(games[currentIndex].id);
    }
  }
  if (needsDraw) {
    partialLiveUpdate = false;
    draw();
  } else if (partialLiveUpdate && screen == Screen::LIVE && gameCount > 0) {
    partialLiveUpdate = false;
    drawLiveReadout(games[featuredIndex % gameCount]);
  }
  // Update one optional pregame conference record after the current frame
  // is painted. This keeps the first season/standings view responsive.
  tickPregameConferenceRecords();
  if (rebootAt && int32_t(now - rebootAt) >= 0) ESP.restart();
  delay(5);
}
