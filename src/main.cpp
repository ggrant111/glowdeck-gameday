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
#ifdef GLOWDECK_WAVESHARE_S3
#include "GameDayS3Hardware.h"
#include "GoalGifPlayer.h"
#include "S3NativeLayout.h"
#else
#include <XPT2046_Touchscreen.h>
#endif
#include "WebUi.h"
#include "TeamLogos.h"
#include "GameDayDate.h"
#include "GameDayCadence.h"
#include "ConferenceRecords.h"
#include "GameDetailMode.h"
#include "StandingsCarousel.h"
#include "LiveCarousel.h"
#include "NhlFetchPolicy.h"
#include "CelebrationEngine.h"
#include "TeamGradient.h"
#include "GameEventLog.h"
#include "PowerPlayLogPolicy.h"
#include "NhlPowerPlayEvidence.h"
#include "NhlPenaltyLedger.h"
#include "NhlOnIce.h"
#include "EventOverlay.h"
#include "DisplayEventPolicy.h"
#include "GameFollowPolicy.h"
#include "RearLedPattern.h"
#include "TeamPalette.h"
#include <time.h>

// GlowDeck GameDay — ESP32-2432S028R (2.8" resistive CYD).
// Display: HSPI / TFT_eSPI; touch: dedicated VSPI; speaker amp input: GPIO26.
// Keep initialization order boring: display -> preferences -> AP -> web -> STA.
// No Wi-Fi or NHL network call is allowed to block boot.

namespace {
#ifdef GLOWDECK_WAVESHARE_S3
constexpr uint8_t PIN_BL = 6; // Waveshare backlight; no direct PWM horn or CYD rear RGB.
#else
constexpr uint8_t PIN_BL = 21, PIN_AUDIO = 26, PIN_TOUCH_CS = 33, PIN_TOUCH_IRQ = 36;
#endif
constexpr uint8_t AUDIO_CH = 6;
// GPIO map is tested independently of the NHL color palette. A CYD
// hardware test found green/blue were reversed in the earlier firmware.
constexpr uint8_t PIN_RGB_R = RearLedPattern::kRedPin,
                  PIN_RGB_G = RearLedPattern::kGreenPin,
                  PIN_RGB_B = RearLedPattern::kBluePin;
constexpr uint8_t RGB_CH_R = 2, RGB_CH_G = 3, RGB_CH_B = 4;
constexpr uint8_t MAX_GAMES = GameFollowPolicy::kMaxDailyGames, MAX_WLED = 4;
constexpr uint8_t MAX_FOLLOWED_GAMES = MAX_GAMES; // Can follow every game.
constexpr uint16_t BG = 0x0841, PANEL = 0x10A2, WHITE = 0xFFFF;
constexpr uint16_t MUTED = 0x9CF3, TEAL = 0x340F, ORANGE = 0xFD20;

struct Game {
  uint32_t id = 0;
  String home, away, homeName, awayName, state, startUTC, clock, periodType;
  int homeScore = 0, awayScore = 0, homeShots = -1, awayShots = -1;
  uint8_t period = 0;
  bool intermission = false;
  uint8_t ppSide = 0; // Confirmed PP: 0 none, 1 home, 2 away
  uint8_t ppCandidate = 0; // Unverified NHL situation-description label
  bool ppDataKnown = false, ppSituationKnown = false;
  uint32_t ppUpdatedAt = 0;
  uint8_t emptyNetSide = 0; // 0 none, 1 home goalie pulled, 2 away, 3 both
  bool emptyNetKnown = false;
  uint32_t emptyNetUpdatedAt = 0;
};
struct Settings {
  String ssid, wifiPassword, adminPassword;
  String wled[MAX_WLED];
  bool wledEnabled[MAX_WLED] = {false, false, false, false};
  // Masked opt-in for every connected controller: goal / PP / start / final.
  uint8_t wledEvents[MAX_WLED] = {15, 15, 15, 15};
  bool wledPP = true, wledGameStart = true, wledFinal = true;
  uint8_t wledGoalStyle = 0; // 0 strobe+chase, 1 strobe, 2 chase, 3 solid
  uint8_t favoriteSide = 0; // home 0 / away 1; for start and final outcomes
  uint8_t mode = 3; // 0=off, 1=home, 2=away, 3=both
  uint8_t brightness = 190;
  uint8_t seconds = 8;
  bool horn = false;
  bool rearLedEnabled = true;
  bool rearGoal = true;
  bool rearPowerPlay = true;
  uint8_t rearBrightness = 128;
  uint8_t rearGoalSeconds = 6;
  uint32_t followIds[MAX_FOLLOWED_GAMES] = {};
  bool invertScreen = true; // This CYD panel needs INVON to render dark colors correctly.
  bool cycleGames = true;
  bool showStandings = true; // Add 4 conference + 4 division table slides.
  bool overlayEnabled = true, overlayGoals = true, overlayPowerPlays = true;
  bool overlayStart = true, overlayFinal = true, overlayFlash = true;
  bool overlayLogos = true;
  bool displayAllLiveEvents = true;
  uint32_t favoriteTeams = 0;
  uint8_t overlayGoalSeconds = 8, overlayFinalSeconds = 7;
  uint8_t cycleSeconds = 9;
  uint8_t timezone = 1; // 0=ET, 1=CT, 2=MT, 3=PT, 4=AZ
  int touchX0 = 300, touchX1 = 3800, touchY0 = 300, touchY1 = 3800;
} cfg;

Preferences prefs;
#ifdef GLOWDECK_WAVESHARE_S3
GameDayS3Display tft;
GameDayS3Touch touch;
#else
TFT_eSPI tft;
SPIClass touchSPI(VSPI);
XPT2046_Touchscreen touch(PIN_TOUCH_CS, PIN_TOUCH_IRQ);
#endif
WebServer server(80);
Game games[MAX_GAMES];
struct GoalWatch {
  uint32_t id = 0;
  int home = 0, away = 0;
  bool baseline = false;
};
GoalWatch goalWatch[MAX_GAMES];
struct StateWatch {
  uint32_t id = 0;
  char state[8] = {};
  bool baseline = false;
};
StateWatch stateWatch[MAX_GAMES];
// Session-only ring buffer, 64 entries; no flash wear or framebuffer use.
GameEventLog::Buffer gameEventHistory;
uint32_t wledEventOwner[MAX_WLED] = {0, 0, 0, 0};
uint32_t armedPpLogSeq = 0, queuedWledLogSeq = 0;
// NHL game ID is the stable key: game positions can reorder every poll.
PowerPlayLogPolicy::Watch ppEventWatch[MAX_GAMES];
struct PowerPlaySnapshot {
  uint32_t id = 0, updatedAt = 0;
  uint8_t side = 0;
  bool known = false;
};
PowerPlaySnapshot ppCache[MAX_GAMES];
NhlPenaltyLedger::Book penaltyLedgers[MAX_GAMES];
uint32_t lastPenaltyLedgerAttempt = 0;
enum class RearEvent : uint8_t { IDLE, GOAL, POWER_PLAY, DIAGNOSTIC };
RearEvent rearEvent = RearEvent::IDLE;
uint32_t rearGoalAt = 0, rearGoalColor = 0, rearGoalDuration = 0;
uint32_t rearPulseStart = 0;
uint32_t rearLastTick = 0;
uint8_t rearLastDuty[3] = {255, 255, 255};
uint32_t lastPpFallback = 0;
uint32_t rearTestPpUntil = 0, rearTestPpColor = 0;
uint32_t rearDiagnosticUntil = 0, rearDiagnosticColor = 0;
uint8_t gameCount = 0, gamePage = 0, featuredIndex = 0;
uint8_t carouselSlide = 0; // Independent of armed game and detail index.
LiveCarousel::Plan carouselPlan; // 28 fixed entries, no heap or sprite buffer.
uint32_t lastCarouselAdvance = 0;
uint32_t lastBackgroundOverlayAt = 0;
bool hasBackgroundOverlay = false;
uint32_t backgroundEventsSkipped = 0, overlayEventsDropped = 0;
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
String savedWledIp[MAX_WLED];
CelebrationEngine::Event wledActiveEvent = CelebrationEngine::Event::NONE;
uint32_t wledEventStart = 0, wledEventColor = 0, wledEventDuration = 0;
uint8_t wledPhase = 0;
String wledDeviceResult[MAX_WLED];
String lastArmedGameState;
uint32_t wledPpTestUntil = 0, wledSuppressPpUntil = 0;
CelebrationEngine::Event queuedWledEvent = CelebrationEngine::Event::NONE;
uint32_t queuedWledColor = 0;
bool restoring[MAX_WLED] = {false, false, false, false};
uint32_t restoreAt[MAX_WLED] = {0, 0, 0, 0};
uint8_t restoreRetries[MAX_WLED] = {0, 0, 0, 0};
bool hornPlaying = false;
uint32_t hornStart = 0;
uint16_t hornFrequency = 0;

enum class Screen : uint8_t { LIVE, GAMES, SETUP, CALIBRATE, STATS };
EventOverlay::Controller overlayController;
#ifdef GLOWDECK_WAVESHARE_S3
GoalGifPlayer goalGif;
uint32_t gifOverlayStart = 0;
File goalUpload;
bool goalUploadFailed = false, goalUploadFinished = false;
size_t goalUploadBytes = 0;
String goalUploadError;
#endif
uint32_t overlayLastFrame = 0;
uint32_t overlayCount = 0;
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
// Each successful NHL score fetch rebuilds this small index-only plan.
// During live games: show ONLY live matchups full-screen plus 4-up summaries.
// Outside live action: retain the existing all-games + 8-standings rotation.
uint8_t carouselPageCount() { return carouselPlan.count; }
LiveCarousel::Page currentCarouselPage() {
  return carouselPlan.at(carouselSlide);
}
bool showingStandingsPage() {
  return currentCarouselPage().kind == LiveCarousel::Kind::STANDINGS;
}
bool showingSummaryPage() {
  return currentCarouselPage().kind == LiveCarousel::Kind::SUMMARY;
}
bool showingGamePage() {
  return currentCarouselPage().kind == LiveCarousel::Kind::GAME;
}
void rebuildCarouselPlan() {
  carouselPlan = LiveCarousel::build(games, gameCount, cfg.showStandings,
                                     cfg.favoriteTeams);
  if (carouselSlide >= carouselPageCount()) carouselSlide = 0;
  const LiveCarousel::Page page = currentCarouselPage();
  if (page.kind == LiveCarousel::Kind::GAME)
    featuredIndex = page.index;
}
void advanceCarousel(int direction) {
  carouselSlide = LiveCarousel::advance(carouselSlide, carouselPlan, direction);
  const LiveCarousel::Page page = currentCarouselPage();
  if (page.kind == LiveCarousel::Kind::GAME) featuredIndex = page.index;
  lastCarouselAdvance = millis();
  partialLiveUpdate = false;
  needsDraw = true;
}

SeasonStanding seasonStandings[32];
bool standingsReady = false;
uint32_t standingsLastAttempt = 0;
String standingsForDate;
String standingsError;  // Shown on the standings slide and /api/status.
int standingsLastHttpCode = 0;
uint8_t standingsLastTeamCount = 0;

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

uint32_t teamRgb(const String &code) {
  return TeamPalette::rgb(code.c_str());
}
String rgbHex(uint32_t color) {
  char hex[8];
  snprintf(hex, sizeof(hex), "#%06lX", (unsigned long)(color & 0xFFFFFFUL));
  return String(hex);
}

void saveSettings(); // Star toggle persists game follows.
bool rearFollows(uint32_t gameId) {
  if (!gameId) return false;
  if (gameId == selectedGameId) return true; // Armed WLED game also drives rear RGB.
  for (uint8_t i = 0; i < MAX_FOLLOWED_GAMES; ++i)
    if (cfg.followIds[i] == gameId) return true;
  return false;
}
bool isScheduledGame(uint32_t id) {
  if (!id) return false;
  for (uint8_t i=0; i<gameCount; ++i)
    if (games[i].id==id) return true;
  return false;
}
// One follow policy serves CYD game carousel, detail view, game list and web.
// All 20 daily games may be followed; only the armed game controls WLED.
using FollowResult = GameFollowPolicy::Result;
FollowResult setGameFollow(uint32_t id, bool follow) {
  uint8_t changedSlot = 255;
  const FollowResult result = GameFollowPolicy::change(
      cfg.followIds, MAX_FOLLOWED_GAMES, id, isScheduledGame(id),
      selectedGameId, follow, changedSlot);
  if (result == FollowResult::ADDED || result == FollowResult::REMOVED) {
    // Only write the changed NVS slot to avoid re-saving all preferences
    // on every touchscreen star tap.
    prefs.putUInt(("follow" + String(changedSlot)).c_str(),
                  cfg.followIds[changedSlot]);
    Serial.printf("[FOLLOW] game=%lu %s (%u max)\n",
                  (unsigned long)id, follow ? "added" : "removed",
                  MAX_FOLLOWED_GAMES);
    needsDraw = true;
    partialLiveUpdate = false;
  }
  return result;
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
      if (rearFollows(g.id) ||
          DisplayEventPolicy::favoriteMatch(
              cfg.favoriteTeams, g.away.c_str(), g.home.c_str()))
        priorityLive = true;
    }
    if (g.id == selectedGameId &&
        (g.state == "FUT" || g.state == "PRE")) armedPregame = true;
  }
  return GameDayCadence::intervalMs(priorityLive, anyLive, armedPregame);
}

void setupRearLed() {
#ifdef GLOWDECK_WAVESHARE_S3
  // CYD rear RGB pins are NOT safe to drive on the Waveshare board.
  return;
#else
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
#endif
}

void writeRearLed(uint32_t rgb, uint8_t intensity) {
#ifdef GLOWDECK_WAVESHARE_S3
  (void)rgb; (void)intensity;  // No onboard rear RGB LED.
  return;
#else
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
#endif
}

void triggerRearGoal(uint32_t rgb) {
  if (!cfg.rearLedEnabled || !cfg.rearGoal) return;
  rearDiagnosticUntil = 0;
  rearTestPpUntil = 0;
  rearGoalColor = rgb;
  rearGoalAt = millis();
  rearGoalDuration = uint32_t(cfg.rearGoalSeconds) * 1000UL;
  rearEvent = RearEvent::GOAL;
}

void stopRearLed() {
  rearEvent = RearEvent::IDLE;
  rearGoalDuration = 0;
  rearTestPpUntil = 0;
  rearDiagnosticUntil = 0;
  writeRearLed(0, 0);
}

// The NHL 'situation' object marks which team has "PP" and/or more skaters.
// Treat missing data as UNKNOWN: no invented power-play event.
uint8_t detectPowerPlay(JsonVariantConst situation) {
  if (situation.isNull()) return 0;
  // The NHL /scoreboard/now feed explicitly identifies PP via
  // situationDescriptions. Never infer a penalty from strength alone:
  // 6-on-5 with a pulled goalie is NOT a power play.
  JsonVariantConst home = situation["homeTeam"];
  JsonVariantConst away = situation["awayTeam"];
  auto hasPP = [](JsonVariantConst team) -> bool {
    for (JsonVariantConst label : team["situationDescriptions"].as<JsonArrayConst>()) {
      if (NhlPowerPlayEvidence::ppLabel(label.as<const char*>())) return true;
    }
    return false;
  };
  return NhlPowerPlayEvidence::confirmedSide(hasPP(home), hasPP(away));
}

// Never confuse a pulled goalie with a penalized side. The NHL score feed's
// optional situation reports six skaters for the team with an empty net.
uint8_t detectEmptyNet(JsonVariantConst situation) {
  if (situation.isNull()) return 0;
  const JsonVariantConst home = situation["homeTeam"];
  const JsonVariantConst away = situation["awayTeam"];
  const int h = home["strength"] | -1, a = away["strength"] | -1;
  bool hp = !home["goaliePulled"].isNull() &&
            home["goaliePulled"].as<bool>();
  bool ap = !away["goaliePulled"].isNull() &&
            away["goaliePulled"].as<bool>();
  for (JsonVariantConst label : home["situationDescriptions"].as<JsonArrayConst>())
    if (NhlOnIce::emptyNetLabel(label.as<const char*>())) hp=true;
  for (JsonVariantConst label : away["situationDescriptions"].as<JsonArrayConst>())
    if (NhlOnIce::emptyNetLabel(label.as<const char*>())) ap=true;
  return NhlOnIce::emptyNetSide(a,h,ap,hp);
}
NhlPenaltyLedger::Book *penaltyLedger(uint32_t id) {
  if (!id) return nullptr;
  for (auto &ledger : penaltyLedgers)
    if (ledger.gameId == id) return &ledger;
  for (auto &ledger : penaltyLedgers)
    if (!ledger.gameId) {
      ledger.reset(id);
      return &ledger;
    }
  return nullptr;
}
// Both a documented NHL "PP" label and an active, applicable official minor
// or major are required. A missing/expired penalty ledger is UNKNOWN, not PP.
void validatePowerPlay(Game &g) {
  g.ppSide = 0;
  g.ppDataKnown = false;
  if (g.state != "LIVE" && g.state != "CRIT") return;
  if (g.intermission) return;
  NhlPenaltyLedger::Book *ledger = penaltyLedger(g.id);
  if (!ledger || !ledger->loaded ||
      uint32_t(millis()-ledger->checkedAt) > 65000UL) return;
  uint16_t seconds=0;
  if (!NhlPenaltyLedger::Book::fromRemaining(
          g.period,g.clock.c_str(),seconds))return;
  const uint8_t penaltySide=ledger->sideAt(seconds);
  // A verified active penalty alone is insufficient if it is offsetting,
  // delayed, or not yet reflected by the NHL manpower situation.
  if (penaltySide && g.ppSituationKnown && g.ppCandidate==penaltySide) {
    g.ppSide=penaltySide;
    g.ppDataKnown=true;
  } else if (!penaltySide) {
    // Official penalties (including PP-goal termination) show normal play.
    g.ppSide=0;
    g.ppDataKnown=true;
  }
  if (g.ppDataKnown) g.ppUpdatedAt=millis();
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

void markPpRearEvent(uint32_t id, const String &team);
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
  // Solid diagnostics expose wiring/PWM channel swaps independently of teams.
  if (rearDiagnosticUntil && int32_t(rearDiagnosticUntil - now) > 0) {
    rearEvent = RearEvent::DIAGNOSTIC;
    writeRearLed(rearDiagnosticColor, 255);
    return;
  }
  rearDiagnosticUntil = 0;
  uint32_t ppColor = 0;
  const bool testPulse = rearTestPpUntil &&
      int32_t(rearTestPpUntil - now) > 0;
  if (testPulse) ppColor = rearTestPpColor;
  if (testPulse || (cfg.rearPowerPlay && activePowerPlaySide(now, ppColor))) {
    if (rearEvent != RearEvent::POWER_PLAY) rearPulseStart = now;
    rearEvent = RearEvent::POWER_PLAY;
    writeRearLed(ppColor, RearLedPattern::pulse(uint32_t(now - rearPulseStart)));
    if (!testPulse) {
      for (uint8_t i = 0; i < gameCount; ++i) {
        const Game &g = games[i];
        if (!rearFollows(g.id) || !g.ppSide || !g.ppDataKnown ||
            (g.state != "LIVE" && g.state != "CRIT") ||
            g.intermission || uint32_t(now - g.ppUpdatedAt) > 80000UL) continue;
        markPpRearEvent(g.id, g.ppSide == 1 ? g.home : g.away);
        break; // Only one PP color is rendered by the rear RGB at a time.
      }
    }
    return;
  }
  rearEvent = RearEvent::IDLE;
  writeRearLed(0, 0);
}

// Every game gets an independent baseline, preventing false flashes on reboot,
// date rollover, first fetch, or when following a game already in progress.
// Only record what this firmware OBSERVED from the NHL API. The endpoint
// does not backfill previous goals and cannot confirm physical LED output.
uint32_t gameEventEpoch() {
  const time_t epoch = time(nullptr);
  return epoch > 1700000000 ? (uint32_t)epoch : 0;
}
GameEventLog::Entry &addGameEvent(const Game &g, GameEventLog::Type type,
                                   const String &team, uint8_t delta = 1,
                                   bool preview = false) {
  GameEventLog::Entry &entry = gameEventHistory.add(
      type, gameEventEpoch(), millis()/1000UL, g.id,
      g.away.c_str(), g.home.c_str(), team.c_str(),
      (uint16_t)max(0, g.awayScore), (uint16_t)max(0, g.homeScore),
      g.period, preview, delta);
  Serial.printf("[EVENT] #%lu %s %s %s@%s %d-%d\n",
      (unsigned long)entry.sequence, GameEventLog::typeLabel(type),
      team.c_str(), g.away.c_str(), g.home.c_str(),
      g.awayScore, g.homeScore);
  return entry;
}
GameEventLog::Entry &addPreviewEvent(const Game *g, GameEventLog::Type type,
                                     const String &team) {
  if (g) return addGameEvent(*g, type, team, 0, true);
  return gameEventHistory.add(type, gameEventEpoch(), millis()/1000UL,
                              0, "", "", team.c_str(), 0, 0, 0, true, 0);
}
void markWledEvent(uint32_t sequence, uint8_t i, GameEventLog::Wled status,
                   bool accepted = false) {
  if (i >= MAX_WLED) return;
  GameEventLog::Entry *entry = gameEventHistory.find(sequence);
  if (!entry) return; // Circular log may already have overwritten the row.
  entry->wled[i] = status;
  if (accepted) entry->wledAcknowledgedMask |= 1U << i;
}
void markPpRearEvent(uint32_t id, const String &team) {
  for (uint8_t i = 0; i < gameEventHistory.size(); ++i) {
    GameEventLog::Entry *entry = gameEventHistory.newest(i);
    if (!entry || entry->gameId != id ||
        entry->type != GameEventLog::Type::POWER_PLAY_START ||
        team != entry->team) continue;
    entry->rear = GameEventLog::Rear::ACTIVE;
    return;
  }
}
void celebrate(uint32_t color, uint32_t eventSequence = 0);
bool startWledEvent(CelebrationEngine::Event event, uint32_t color,
                    uint32_t eventSequence = 0);

// Display event overlays share the NHL observation that produced the
// WLED/rear RGB celebration. They never create a second score trigger.
bool overlayAllowed(EventOverlay::Kind kind) {
  if (!cfg.overlayEnabled || screen == Screen::CALIBRATE) return false;
  switch(kind) {
    case EventOverlay::Kind::GOAL: return cfg.overlayGoals;
    case EventOverlay::Kind::POWER_PLAY_START:
    case EventOverlay::Kind::POWER_PLAY_END: return cfg.overlayPowerPlays;
    case EventOverlay::Kind::GAME_START: return cfg.overlayStart;
    case EventOverlay::Kind::FINAL_WIN:
    case EventOverlay::Kind::FINAL_LOSS: return cfg.overlayFinal;
    default: return false;
  }
}
// NHL display alerts are independent of the armed/rear LED selection.
// Manual visual previews bypass the NHL game-admission policy.
bool queueGameOverlay(const Game *g, EventOverlay::Kind kind,
                      const String &team, uint32_t sequence,
                      bool observed = false) {
  if (!overlayAllowed(kind)) return false;
  const bool followed = g && rearFollows(g->id);
  if (observed && (!g || (!cfg.displayAllLiveEvents && !followed)))
    return false;
  const uint8_t tier = observed ? DisplayEventPolicy::tier(
      cfg.favoriteTeams, g->away.c_str(), g->home.c_str(), followed) : 2;
  const uint32_t now = millis();
  if (observed && !DisplayEventPolicy::backgroundAllowed(
          kind, tier, now, lastBackgroundOverlayAt, hasBackgroundOverlay)) {
    ++backgroundEventsSkipped;
    return false;
  }
  EventOverlay::Item item;
  item.kind = kind;
  item.gameId = g ? g->id : 0;
  item.eventSequence = sequence;
  item.color = teamRgb(team);
  item.tier = tier;
  item.duration = observed ? DisplayEventPolicy::duration(
      kind, tier, cfg.overlayGoalSeconds, cfg.overlayFinalSeconds)
      : EventOverlay::durationMs(kind, cfg.overlayGoalSeconds,
                                cfg.overlayFinalSeconds);
  EventOverlay::copy(item.team, team.c_str(), sizeof(item.team));
  if (g) {
    EventOverlay::copy(item.home, g->home.c_str(), sizeof(item.home));
    EventOverlay::copy(item.away, g->away.c_str(), sizeof(item.away));
    EventOverlay::copy(item.clock, g->clock.c_str(), sizeof(item.clock));
    item.period = g->period;
    item.awayScore = (uint8_t)constrain(g->awayScore, 0, 255);
    item.homeScore = (uint8_t)constrain(g->homeScore, 0, 255);
  }
  const bool admitted = overlayController.push(item, now);
  if (admitted) {
    if (observed && tier == 0 && kind != EventOverlay::Kind::GOAL) {
      lastBackgroundOverlayAt = now;
      hasBackgroundOverlay = true;
    }
    ++overlayCount;
    needsDraw = true;
    partialLiveUpdate = false;
    overlayLastFrame = 0;
  } else if (observed) {
    ++overlayEventsDropped;
  }
  return admitted;
}
void detectRearGoalChanges() {
  for (uint8_t i = 0; i < gameCount; ++i) {
    const Game &g = games[i];
    GoalWatch &watch = goalWatch[i];
    if (watch.id != g.id) {
      watch = GoalWatch();
      watch.id = g.id;
    }
    const bool live = g.state == "LIVE" || g.state == "CRIT";
    // Record observed NHL score increments, even when that game's lighting
    // isn't selected. No synthetic goals are added from current scores at boot.
    if (watch.baseline && live) {
      const int deltaHome = g.homeScore - watch.home;
      const int deltaAway = g.awayScore - watch.away;
      for (uint8_t side = 0; side < 2; ++side) {
        const int delta = side == 0 ? deltaHome : deltaAway;
        if (delta <= 0) continue;
        const String &scorer = side == 0 ? g.home : g.away;
        GameEventLog::Entry &entry = addGameEvent(
            g, GameEventLog::Type::GOAL, scorer,
            (uint8_t)min(delta, 255));
        const bool followed = rearFollows(g.id);
        if (!followed) entry.rear = GameEventLog::Rear::NOT_SELECTED;
        else if (!cfg.rearLedEnabled || !cfg.rearGoal)
          entry.rear = GameEventLog::Rear::NOT_ENABLED;
        // WLED is driven only by the armed game and only by allowed teams.
        const bool armedForGoal = g.id == selectedGameId && haveBaseline &&
            (side == 0 ? (cfg.mode == 1 || cfg.mode == 3)
                       : (cfg.mode == 2 || cfg.mode == 3));
        if (armedForGoal) celebrate(teamRgb(scorer), entry.sequence);
        // Start the internal LED flash AFTER all WLED network requests so
        // multi-device snapshots don't consume its 2–12s animation window.
        if (followed && cfg.rearLedEnabled && cfg.rearGoal) {
          triggerRearGoal(teamRgb(scorer));
          entry.rear = GameEventLog::Rear::ACTIVE;
        }
        // Only followed/armed games interrupt the screen. Goal overlays
        // appear even when the WLED slots are empty or disabled.
        if (followed || cfg.displayAllLiveEvents)
          queueGameOverlay(&g, EventOverlay::Kind::GOAL,
                           scorer, entry.sequence, true);
      }
    }
    watch.home = g.homeScore;
    watch.away = g.awayScore;
    watch.baseline = true;
  }
}

// Track PP changes only when fresh situation data is known. Missing NHL
// situation data is not interpreted as PP ending.
// Power-play logging is keyed by NHL game ID, not by array index, because the
// NHL's daily score feed can reorder matches between 10-second polls.
PowerPlayLogPolicy::Watch *getPowerPlayLogWatch(uint32_t id) {
  if (!id) return nullptr;
  for (auto &watch : ppEventWatch)
    if (watch.gameId == id) return &watch;
  for (auto &watch : ppEventWatch)
    if (!watch.gameId) {
      watch = PowerPlayLogPolicy::Watch();
      watch.gameId = id;
      return &watch;
    }
  return nullptr; // Full (normally one slot per game on the date).
}

void observePowerPlayEvents() {
  for (uint8_t i = 0; i < gameCount; ++i) {
    const Game &g = games[i];
    const bool followed = rearFollows(g.id);
    if (!followed && !cfg.displayAllLiveEvents) continue;
    if (g.state != "LIVE" && g.state != "CRIT") continue;
    PowerPlayLogPolicy::Watch *watch = getPowerPlayLogWatch(g.id);
    if (!watch) continue;
    const bool reliable = g.ppDataKnown &&
       uint32_t(millis() - g.ppUpdatedAt) < 80000UL;
    const PowerPlayLogPolicy::Change change = PowerPlayLogPolicy::observe(
        *watch, g.id, reliable, g.ppSide, millis());
    if (change.ended) {
      const String &team = change.ended == 1 ? g.home : g.away;
      const uint32_t seq = addGameEvent(
          g, GameEventLog::Type::POWER_PLAY_END, team, 0).sequence;
      queueGameOverlay(&g, EventOverlay::Kind::POWER_PLAY_END, team, seq, true);
      if (g.id == selectedGameId) armedPpLogSeq = 0;
    }
    if (change.started) {
      const String &team = change.started == 1 ? g.home : g.away;
      GameEventLog::Entry &entry = addGameEvent(
          g, GameEventLog::Type::POWER_PLAY_START, team, 0);
      entry.rear = !followed ? GameEventLog::Rear::NOT_SELECTED
          : (!cfg.rearLedEnabled || !cfg.rearPowerPlay)
            ? GameEventLog::Rear::NOT_ENABLED
            : GameEventLog::Rear::SCHEDULED;
      if (g.id == selectedGameId) armedPpLogSeq = entry.sequence;
      queueGameOverlay(&g, EventOverlay::Kind::POWER_PLAY_START,
                       team, entry.sequence, true);
    }
  }
}

Game *selectedGame() {
  for (uint8_t i = 0; i < gameCount; ++i)
    if (games[i].id == selectedGameId) return &games[i];
  return nullptr;
}
// Same fallback for the preview API and web UI; never default both sides to UTA.
Game *testMatchup() {
  Game *g = selectedGame();
  return g ? g : (gameCount ? &games[featuredIndex % gameCount] : nullptr);
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
void scheduleWledRestoreAll(); // Date rollover must retire old WLED events.

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
    carouselSlide = 0;
    rebuildCarouselPlan();
    lastCarouselAdvance = millis();
    needDetailLoad = false;
    detail.loaded = false;
    for (auto &entry : carouselStats) entry = CarouselStats();
    standingsReady = false;
    standingsLastAttempt = 0;
    standingsForDate = "";
    standingsError = "";
    standingsLastHttpCode = 0;
    standingsLastTeamCount = 0;
    for (auto &entry : seasonStandings) entry = SeasonStanding();
    for (auto &entry : conferenceSnapshots) entry = ConferenceSnapshot();
    lastConferenceFetch = 0;
    webPreviewGameId = 0;
    scheduleWledRestoreAll();
    overlayController.clear();
    overlayLastFrame = 0;
    lastArmedGameState = "";
    for (auto &entry : goalWatch) entry = GoalWatch();
    for (auto &entry : stateWatch) entry = StateWatch();
    hasBackgroundOverlay = false;
    for (auto &entry : ppEventWatch) entry = PowerPlayLogPolicy::Watch();
    armedPpLogSeq = 0;
    for (auto &entry : ppCache) entry = PowerPlaySnapshot();
    for (auto &entry : penaltyLedgers) entry.reset();
    lastPenaltyLedgerAttempt = 0;
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
  standingsError = "";
  standingsLastHttpCode = 0;
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setConnectTimeout(4500);
  http.setTimeout(15000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.useHTTP10(true);
  const String url = "https://api-web.nhle.com/v1/standings/" +
                     (today.length() == 10 ? today : String("now"));
  if (!http.begin(client, url)) {
    standingsError = "NHL standings connect failed";
    Serial.println("[NHL] " + standingsError);
    return standingsReady;
  }
  http.addHeader("Accept-Encoding", "identity");
  standingsLastHttpCode = http.GET();
  if (standingsLastHttpCode != HTTP_CODE_OK) {
    standingsError = "NHL standings HTTP " + String(standingsLastHttpCode);
    Serial.println("[NHL] " + standingsError);
    http.end();
    return standingsReady;
  }
  // These NHL fields are shared with the pregame preview. Allocate the
  // expanded filter on heap to protect the loopTask stack on the CYD.
  DynamicJsonDocument filter(1024);
  if (filter.capacity() < 1024) {
    standingsError = "NHL standings filter memory low";
    http.end();
    return standingsReady;
  }
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
    standingsError = "Standings JSON filter full";
    Serial.println("[NHL] " + standingsError);
    http.end();
    return standingsReady;
  }
  DynamicJsonDocument parsed(24576);
  if (parsed.capacity() < 24576) {
    standingsError = "NHL standings JSON memory low";
    http.end();
    return standingsReady;
  }
  Stream &stream = http.getStream();
  stream.setTimeout(15000);
  const DeserializationError err = deserializeJson(
      parsed, stream, DeserializationOption::Filter(filter));
  http.end();
  if (err || parsed.overflowed()) {
    standingsError = "Standings JSON: " +
                     String(err ? err.c_str() : "NoMemory");
    Serial.println("[NHL] " + standingsError);
    return standingsReady;
  }
  JsonArray teams = parsed["standings"].as<JsonArray>();
  if (teams.isNull()) {
    standingsError = "NHL standings rows missing";
    Serial.println("[NHL] " + standingsError);
    return standingsReady;
  }
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
  standingsLastTeamCount = loaded;
  uint8_t ranked = 0;
  for (uint8_t i = 0; i < loaded; ++i) {
    const SeasonStanding &t = fresh[i];
    if (t.conferenceSequence > 0 && t.divisionSequence > 0 &&
        t.division != StandingsCarousel::Group::EAST &&
        t.division != StandingsCarousel::Group::WEST)
      ++ranked;
  }
  if (loaded < 24 || ranked < 24) {
    // Do not replace a good table with a truncated or unrated one.
    standingsError = "NHL: " + String(loaded) + " teams / " +
                     String(ranked) + " ranked";
    Serial.println("[NHL] " + standingsError);
    return standingsReady;
  }
  for (uint8_t i = 0; i < 32; ++i)
    seasonStandings[i] = i < loaded ? fresh[i] : SeasonStanding();
  standingsForDate = today;
  standingsReady = true;
  standingsError = "";
  if (screen == Screen::LIVE && showingStandingsPage()) needsDraw = true;
  Serial.printf("[NHL] League standings loaded: %u teams, stack=%u heap=%u\n",
                loaded, (unsigned)uxTaskGetStackHighWaterMark(nullptr),
                (unsigned)ESP.getFreeHeap());
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
      if (screen == Screen::LIVE && showingGamePage() &&
        currentCarouselPage().index == i) partialLiveUpdate = true;
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
    cfg.wledEvents[i] = prefs.getUChar(("event" + String(i)).c_str(),
                                         CelebrationEngine::ALL_EVENTS) &
                        CelebrationEngine::ALL_EVENTS;
  }
  cfg.mode = constrain(prefs.getUChar("mode", 3), 0, 3);
  cfg.wledPP = prefs.getBool("wledPP", true);
  cfg.wledGameStart = prefs.getBool("wledStart", true);
  cfg.wledFinal = prefs.getBool("wledFinal", true);
  cfg.wledGoalStyle = constrain(prefs.getUChar("wledStyle", 0), 0, 3);
  cfg.favoriteSide = constrain(prefs.getUChar("favSide", 0), 0, 1);
  cfg.brightness = constrain(prefs.getUChar("bright", 190), 1, 255);
  cfg.seconds = constrain(prefs.getUChar("seconds", 8), 2, 30);
  cfg.horn = prefs.getBool("horn", false);
  #ifdef GLOWDECK_WAVESHARE_S3
  cfg.rearLedEnabled = false; // S3 has no CYD-style built-in rear LED.
#else
  cfg.rearLedEnabled = prefs.getBool("rearOn", true);
#endif
  cfg.rearGoal = prefs.getBool("rearGoal", true);
  cfg.rearPowerPlay = prefs.getBool("rearPP", true);
  cfg.rearBrightness = prefs.getUChar("rearBri", 128);
  cfg.rearGoalSeconds = constrain(prefs.getUChar("rearDur", 6), 2, 12);
  for (uint8_t i = 0; i < MAX_FOLLOWED_GAMES; ++i)
    cfg.followIds[i] = prefs.getUInt(("follow" + String(i)).c_str(), 0);
  #ifdef GLOWDECK_WAVESHARE_S3
  // First physical S3 test: toggling the color mode once corrected the LCD.
  // Default fresh boards to that observed good ST7796 state. Saved per-board
  // choices still take precedence; no NVS reset or forced migration.
  cfg.invertScreen = prefs.getBool("invert", true);
#else
  cfg.invertScreen = prefs.getBool("invert", true);
#endif
  cfg.cycleGames = prefs.getBool("carousel", true);
  cfg.showStandings = prefs.getBool("standPages", true);
  cfg.overlayEnabled = prefs.getBool("ovOn", true);
  cfg.overlayGoals = prefs.getBool("ovGoal", true);
  cfg.overlayPowerPlays = prefs.getBool("ovPP", true);
  cfg.overlayStart = prefs.getBool("ovStart", true);
  cfg.overlayFinal = prefs.getBool("ovFinal", true);
  cfg.overlayFlash = prefs.getBool("ovFlash", true);
  cfg.overlayLogos = prefs.getBool("ovLogos", true);
  cfg.displayAllLiveEvents = prefs.getBool("ovAllLive", true);
  cfg.favoriteTeams = prefs.getUInt("favTeams", 0);
  cfg.overlayGoalSeconds = constrain(prefs.getUChar("ovGoalSec", 8), 3, 12);
  cfg.overlayFinalSeconds = constrain(prefs.getUChar("ovFinalSec", 7), 3, 12);
  cfg.cycleSeconds = constrain(prefs.getUChar("cycleSec", 9), 5, 30);
  cfg.timezone = constrain(prefs.getUChar("timezone", 1), 0, 4);
  cfg.touchX0 = prefs.getInt("tx0", 300);
  cfg.touchX1 = prefs.getInt("tx1", 3800);
  cfg.touchY0 = prefs.getInt("ty0", 300);
  cfg.touchY1 = prefs.getInt("ty1", 3800);
  selectedGameId = prefs.getUInt("game", 0);
  rebuildCarouselPlan(); // Standings-only fallback until the first NHL fetch.
}

void saveSettings() {
  prefs.putString("ssid", cfg.ssid);
  prefs.putString("wpass", cfg.wifiPassword);
  prefs.putString("admin", cfg.adminPassword);
  for (uint8_t i = 0; i < MAX_WLED; ++i) {
    prefs.putString(("ip" + String(i)).c_str(), cfg.wled[i]);
    prefs.putBool(("enable" + String(i)).c_str(), cfg.wledEnabled[i]);
    prefs.putUChar(("event" + String(i)).c_str(), cfg.wledEvents[i]);
  }
  prefs.putUChar("mode", cfg.mode);
  prefs.putBool("wledPP", cfg.wledPP);
  prefs.putBool("wledStart", cfg.wledGameStart);
  prefs.putBool("wledFinal", cfg.wledFinal);
  prefs.putUChar("wledStyle", cfg.wledGoalStyle);
  prefs.putUChar("favSide", cfg.favoriteSide);
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
  prefs.putBool("ovOn", cfg.overlayEnabled);
  prefs.putBool("ovGoal", cfg.overlayGoals);
  prefs.putBool("ovPP", cfg.overlayPowerPlays);
  prefs.putBool("ovStart", cfg.overlayStart);
  prefs.putBool("ovFinal", cfg.overlayFinal);
  prefs.putBool("ovFlash", cfg.overlayFlash);
  prefs.putBool("ovLogos", cfg.overlayLogos);
  prefs.putBool("ovAllLive", cfg.displayAllLiveEvents);
  prefs.putUInt("favTeams", cfg.favoriteTeams);
  prefs.putUChar("ovGoalSec", cfg.overlayGoalSeconds);
  prefs.putUChar("ovFinalSec", cfg.overlayFinalSeconds);
  prefs.putUChar("cycleSec", cfg.cycleSeconds);
  prefs.putUChar("timezone", cfg.timezone);
}

// An AP-connected device has already supplied the GlowDeck WPA2 hotspot
// password. Skip a *second* browser login only for connections that actually
// arrived on the SoftAP interface from its own subnet. Keep HTTP Basic auth
// on the home-network STA interface; never trust an HTTP Host header alone.
bool requestFromSetupAp() {
  const IPAddress apIp = WiFi.softAPIP();
  const IPAddress localIp = server.client().localIP();
  const IPAddress remoteIp = server.client().remoteIP();
  if (apIp == IPAddress(0, 0, 0, 0) || localIp != apIp ||
      remoteIp == apIp) return false;
  const IPAddress mask = WiFi.softAPSubnetMask();
  for (uint8_t i = 0; i < 4; ++i) {
    if ((remoteIp[i] & mask[i]) != (apIp[i] & mask[i])) return false;
  }
  return true;
}
bool hasDashboardAccess() {
  return requestFromSetupAp() ||
         server.authenticate("admin", cfg.adminPassword.c_str());
}
bool authorized() {
  if (hasDashboardAccess()) return true;
  server.requestAuthentication(BASIC_AUTH, "GlowDeck",
      "Sign in as admin on home Wi-Fi, or connect to the GlowDeck setup hotspot for password-free dashboard access.");
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
#ifndef GLOWDECK_WAVESHARE_S3
    ledcWriteTone(AUDIO_CH, nextFreq); // CYD amp; S3 requires ES8311 codec support.
#endif
    hornFrequency = nextFreq;
  }
  if (elapsed >= 900) hornPlaying = false;
}

// WLED Celebration Engine v0.3.1
// Same event, RGB, speed/intensity and global event clock across all opted-in
// WLED controllers. HTTP delivery may have small network skew; this is not
// sub-frame-accurate UDP pixel synchronization.
//
// Snapshot ALL selected devices before sending the first effect update. This
// keeps sequential state GET latency out of the visible celebration sequence.
bool wledParticipates(uint8_t i, CelebrationEngine::Event event) {
  return i < MAX_WLED && !cfg.wled[i].isEmpty() &&
      CelebrationEngine::eligible(cfg.wledEnabled[i], cfg.wledEvents[i], event);
}

uint8_t wledEligibleCount(CelebrationEngine::Event event) {
  uint8_t count = 0;
  for (uint8_t i = 0; i < MAX_WLED; ++i)
    if (wledParticipates(i, event)) ++count;
  return count;
}

// WLED's "udpn.nn" suppresses UDP sync notifications for THIS state
// change. Preserve the user's regular sync configuration; keep excluded
// controllers outside the event even if their usual sync groups overlap.
String wledRestoreBody(const String &original) {
  String body = original;
  if (body.length() < 2) return "";
  const int pos = body.indexOf("\"udpn\"");
  if (pos >= 0) {
    const int opening = body.indexOf('{', pos);
    if (opening >= 0) {
      int nesting = 1, end = opening + 1;
      while (end < (int)body.length() && nesting) {
        if (body[end] == '{') ++nesting;
        if (body[end] == '}') --nesting;
        ++end;
      }
      if (!nesting) {
        body = body.substring(0, pos) +
               "\"udpn\":{\"nn\":true}" + body.substring(end);
        return body;
      }
    }
  }
  const int end = body.lastIndexOf('}');
  if (end < 0) return "";
  body = body.substring(0, end) + ",\"udpn\":{\"nn\":true}}";
  return body;
}

String wledEffectBody(const String &snapshot,
                      CelebrationEngine::Event event, uint32_t color,
                      uint8_t phase) {
  DynamicJsonDocument filter(384);
  if (filter.capacity() < 384) return "";
  JsonObject templateSegment = filter["seg"][0].to<JsonObject>();
  templateSegment["id"] = true;
  templateSegment["start"] = true;
  templateSegment["stop"] = true;
  templateSegment["len"] = true;
  DynamicJsonDocument state(3072);
  if (state.capacity() < 3072) return "";
  if (deserializeJson(state, snapshot, DeserializationOption::Filter(filter)))
    return "";

  DynamicJsonDocument command(4096);
  if (command.capacity() < 4096) return "";
  command["on"] = true;
  command["bri"] = cfg.brightness;
  command["transition"] = 0;
  command["udpn"]["nn"] = true;  // Do not wake non-participating WLEDs.
  const CelebrationEngine::Effect effect =
      CelebrationEngine::effect(event, phase, cfg.wledGoalStyle);
  JsonArray segments = command.createNestedArray("seg");
  JsonArray current = state["seg"].as<JsonArray>();
  uint8_t emitted = 0;
  for (uint8_t j = 0; j < current.size() &&
                       emitted < CelebrationEngine::kMaxSegments; ++j) {
    JsonObject seg = current[j];
    const int start = seg["start"] | 0;
    const int stop = seg["stop"] | 0;
    const int len = seg["len"] | 0;
    if (stop <= start && len <= 0) continue;
    JsonObject out = segments.createNestedObject();
    out["id"] = seg["id"] | (int)j;
    out["on"] = true;
    out["fx"] = effect.fx;
    out["sx"] = effect.speed;
    out["ix"] = effect.intensity;
    out["pal"] = 0;
    JsonArray cols = out.createNestedArray("col");
    JsonArray primary = cols.createNestedArray();
    primary.add((color >> 16) & 0xFF);
    primary.add((color >> 8) & 0xFF);
    primary.add(color & 0xFF);
    JsonArray secondary = cols.createNestedArray();
    secondary.add(0); secondary.add(0); secondary.add(0);
    ++emitted;
  }
  if (!emitted) {
    // Some older WLED builds omit segment bounds from their state JSON.
    JsonObject out = segments.createNestedObject();
    out["id"] = 0;
    out["on"] = true;
    out["fx"] = effect.fx;
    out["sx"] = effect.speed;
    out["ix"] = effect.intensity;
    out["pal"] = 0;
    JsonArray cols = out.createNestedArray("col");
    JsonArray primary = cols.createNestedArray();
    primary.add((color >> 16) & 0xFF);
    primary.add((color >> 8) & 0xFF);
    primary.add(color & 0xFF);
  }
  if (command.overflowed()) return "";
  String body;
  serializeJson(command, body);
  return body;
}

void scheduleWledRestoreAll() {
  const uint32_t now = millis();
  for (uint8_t i = 0; i < MAX_WLED; ++i) {
    if (restoring[i]) {
      restoreAt[i] = now;
      restoreRetries[i] = 0;
    }
  }
  wledActiveEvent = CelebrationEngine::Event::NONE;
  wledEventDuration = 0;
  wledPpTestUntil = 0;
  queuedWledEvent = CelebrationEngine::Event::NONE;
  queuedWledLogSeq = 0;
}

void tickRestores() {
  const uint32_t now = millis();
  for (uint8_t i = 0; i < MAX_WLED; ++i) {
    // UINT32_MAX marks a WLED effect that must stay active until the shared
    // event manager ends it (not a timestamp to compare using subtraction).
    if (!restoring[i] || restoreAt[i] == UINT32_MAX ||
        int32_t(now - restoreAt[i]) < 0) continue;
    const String body = wledRestoreBody(savedState[i]);
    const bool restored = !body.isEmpty() &&
                          postWled(savedWledIp[i].length() ? savedWledIp[i] :
                                   cfg.wled[i], body);
    if (restored) {
      wledDeviceResult[i] = "Restored";
      markWledEvent(wledEventOwner[i], i, GameEventLog::Wled::RESTORED);
      wledEventOwner[i] = 0;
      restoring[i] = false;
      savedState[i] = "";
      savedWledIp[i] = "";
      restoreRetries[i] = 0;
    } else if (++restoreRetries[i] >= 3) {
      wledDeviceResult[i] = "Restore failed";
      markWledEvent(wledEventOwner[i], i, GameEventLog::Wled::RESTORE_FAILED);
      wledEventOwner[i] = 0;
      Serial.printf("[WLED] controller %u restore failed after 3 tries\n", i + 1);
      restoring[i] = false;
      savedState[i] = "";
      savedWledIp[i] = "";
    } else {
      restoreAt[i] = millis() + 1500UL;
      wledDeviceResult[i] = "Retrying restore";
      markWledEvent(wledEventOwner[i], i, GameEventLog::Wled::RESTORE_RETRYING);
    }
  }
}

bool startWledEvent(CelebrationEngine::Event event, uint32_t color,
                    uint32_t eventSequence) {
  if (event == CelebrationEngine::Event::NONE) {
    scheduleWledRestoreAll();
    return false;
  }
  // The NHL observer exclusively owns power-play start/end history.
  // This controller must not invent new NHL events when retrying effects,
  // especially with ZERO configured WLED devices.
  if (!wledEligibleCount(event)) return false;
  bool prepared[MAX_WLED] = {};
  for (uint8_t i = 0; i < MAX_WLED; ++i)
    if (wledParticipates(i, event))
      markWledEvent(eventSequence, i, GameEventLog::Wled::PENDING);
  for (uint8_t i = 0; i < MAX_WLED; ++i) {
    if (!wledParticipates(i, event)) {
      if (restoring[i]) {
        restoreAt[i] = millis();
        restoreRetries[i] = 0;
      }
      continue;
    }
    if (restoring[i] && savedWledIp[i] != cfg.wled[i]) {
      // Device IP changed during a celebration: never restore the old
      // controller's state to a new IP. Retire the old one first.
      restoreAt[i] = millis();
      tickRestores();
      if (restoring[i]) continue;
    }
    if (!restoring[i]) {
      savedState[i] = getWledState(cfg.wled[i]);
      savedWledIp[i] = cfg.wled[i];
      if (savedState[i].isEmpty()) {
        savedWledIp[i] = "";
        wledDeviceResult[i] = "Snapshot failed";
        markWledEvent(eventSequence, i, GameEventLog::Wled::SNAPSHOT_FAILED);
        Serial.printf("[WLED] controller %u skipped: cannot save state\n", i + 1);
        continue;
      }
    }
    prepared[i] = true;
  }

  const uint32_t commonStart = millis();
  uint8_t successful = 0;
  for (uint8_t i = 0; i < MAX_WLED; ++i) {
    if (!prepared[i]) continue;
    // The prior event might have been preempted before its own restore.
    if (wledEventOwner[i] && wledEventOwner[i] != eventSequence)
      markWledEvent(wledEventOwner[i], i, GameEventLog::Wled::SUPERSEDED);
    wledEventOwner[i] = eventSequence;
    const String body = wledEffectBody(savedState[i], event, color, 0);
    if (!body.isEmpty() && postWled(cfg.wled[i], body)) {
      restoring[i] = true;
      restoreRetries[i] = 0;
      // All devices share a single clock; PP has no duration until it ends.
      restoreAt[i] = UINT32_MAX;
      wledDeviceResult[i] = "Playing";
      markWledEvent(eventSequence, i, GameEventLog::Wled::ACCEPTED, true);
      ++successful;
    } else {
      wledDeviceResult[i] = "Command failed";
      markWledEvent(eventSequence, i, GameEventLog::Wled::COMMAND_FAILED);
      // If the POST may have applied despite a timeout, retain the snapshot
      // and attempt to restore rather than leaving the lights stranded.
      restoring[i] = true;
      restoreRetries[i] = 0;
      restoreAt[i] = millis();
    }
  }
  if (!successful) {
    wledActiveEvent = CelebrationEngine::Event::NONE;
    wledEventDuration = 0;
    return false;
  }
  wledActiveEvent = event;
  wledEventColor = color;
  wledEventStart = commonStart;
  wledEventDuration = CelebrationEngine::duration(event, cfg.seconds);
  wledPhase = 0;
  Serial.printf("[WLED] %s on %u/%u selected controllers, heap=%u\n",
                CelebrationEngine::label(event), successful,
                wledEligibleCount(event), (unsigned)ESP.getFreeHeap());
  return true;
}

uint8_t currentWledPowerPlay(uint32_t &color) {
  if (!cfg.wledPP || !selectedGameId) return 0;
  Game *g = selectedGame();
  if (!g || !(g->state == "LIVE" || g->state == "CRIT") ||
      g->intermission || !g->ppDataKnown || !g->ppSide ||
      uint32_t(millis() - g->ppUpdatedAt) >= 80000UL) return 0;
  color = teamRgb(g->ppSide == 1 ? g->home : g->away);
  return g->ppSide;
}

void tickWledCelebrations() {
  const uint32_t now = millis();
  uint32_t powerPlayColor = 0;
  const bool testPP = wledPpTestUntil && int32_t(wledPpTestUntil - now) > 0;
  bool hasPP = false;
  if (testPP) {
    hasPP = true;
    powerPlayColor = wledEventColor;
  } else if (int32_t(wledSuppressPpUntil - now) <= 0) {
    hasPP = currentWledPowerPlay(powerPlayColor) != 0;
  }
  if (wledActiveEvent != CelebrationEngine::Event::NONE &&
      CelebrationEngine::temporary(wledActiveEvent)) {
    const uint32_t elapsed = uint32_t(now - wledEventStart);
    if (elapsed < wledEventDuration) {
      const uint8_t nextPhase = CelebrationEngine::phase(
          wledActiveEvent, elapsed, wledEventDuration, cfg.wledGoalStyle);
      if (nextPhase != wledPhase) {
        wledPhase = nextPhase;
        for (uint8_t i = 0; i < MAX_WLED; ++i) {
          if (!restoring[i] || !wledParticipates(i, wledActiveEvent))
            continue;
          const String body = wledEffectBody(savedState[i], wledActiveEvent,
                                              wledEventColor, wledPhase);
          if (!body.isEmpty() && postWled(cfg.wled[i], body))
            wledDeviceResult[i] = "Playing";
          else {
            wledDeviceResult[i] = "Phase update failed";
            markWledEvent(wledEventOwner[i], i,
                          GameEventLog::Wled::COMMAND_FAILED);
          }
        }
      }
      return;
    }
    // A final result can arrive in the same score poll as a winning goal.
    // Finish the goal celebration before playing the queued final animation.
    if (queuedWledEvent != CelebrationEngine::Event::NONE) {
      const CelebrationEngine::Event pending = queuedWledEvent;
      const uint32_t pendingColor = queuedWledColor;
      queuedWledEvent = CelebrationEngine::Event::NONE;
      if (startWledEvent(pending, pendingColor, queuedWledLogSeq)) {
        queuedWledLogSeq = 0;
        return;
      }
      queuedWledLogSeq = 0;
    }
    // After goal or game-start, return to power-play breathing if NHL still
    // reports an advantage, otherwise restore the original WLED state.
    if (hasPP && startWledEvent(CelebrationEngine::Event::POWER_PLAY,
                                powerPlayColor, armedPpLogSeq)) {
      return;
    }
    scheduleWledRestoreAll();
    return;
  }
  if (wledActiveEvent == CelebrationEngine::Event::POWER_PLAY) {
    if (!hasPP) {
      scheduleWledRestoreAll();
    } else if (wledEventColor != powerPlayColor) {
      startWledEvent(CelebrationEngine::Event::POWER_PLAY, powerPlayColor);
    }
  } else if (hasPP && wledActiveEvent == CelebrationEngine::Event::NONE &&
             wledEligibleCount(CelebrationEngine::Event::POWER_PLAY) > 0) {
    // Avoid re-triggering a failed WLED every 5ms; NHL situation updates are
    // already throttled and each event transition can initiate up to 4 HTTPs.
    static uint32_t lastPpAttempt = 0;
    if (uint32_t(now - lastPpAttempt) > 8000UL) {
      lastPpAttempt = now;
      // Repeated WLED retries update the *same* observed PP event row.
      startWledEvent(CelebrationEngine::Event::POWER_PLAY,
                     powerPlayColor, armedPpLogSeq);
    }
  }
}

// The onboard horn is still handled independently from WLED response codes.
void celebrate(uint32_t color, uint32_t eventSequence) {
  startWledEvent(CelebrationEngine::Event::GOAL, color, eventSequence);
  startHorn();
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
// One official gamecenter/landing summary request for an armed, followed or
// currently visible LIVE game. This compact record provides actual penalty
// types, timeInPeriod and scored power-play goals. It is NOT triggered by
// repeated uncorroborated 6-on-5 NHL situation frames.
void fetchFollowedPowerPlays() {
  if (WiFi.status() != WL_CONNECTED) return;
  Game *target=nullptr;
  NhlPenaltyLedger::Book *book=nullptr;
  uint8_t best=0;
  const LiveCarousel::Page visible=currentCarouselPage();
  for(uint8_t i=0;i<gameCount;++i) {
    Game &g=games[i];
    if (g.state!="LIVE" && g.state!="CRIT")continue;
    if(g.intermission)continue;
    const bool shown=screen==Screen::LIVE &&
        visible.kind==LiveCarousel::Kind::GAME && visible.index==i;
    if(!rearFollows(g.id) && !shown && !cfg.displayAllLiveEvents)continue;
    NhlPenaltyLedger::Book *candidate=penaltyLedger(g.id);
    if(!candidate)continue;
    if(candidate->loaded &&
       uint32_t(millis()-candidate->checkedAt)<30000UL)continue;
    // The game visible on the CYD is highest priority for timely PP badges,
    // followed by the armed game and any additional followed games.
    const uint8_t priority=DisplayEventPolicy::favoriteMatch(
        cfg.favoriteTeams,g.away.c_str(),g.home.c_str()) ? 5
        : shown ? 4 : g.id==selectedGameId ? 3 : rearFollows(g.id) ? 2 : 1;
    if(priority>best) {
      best=priority;target=&g;book=candidate;
    }
  }
  if(!target||!book)return;
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setConnectTimeout(2800);
  http.setTimeout(4200);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.useHTTP10(true);
  const String url="https://api-web.nhle.com/v1/gamecenter/"+
                   String(target->id)+"/landing";
  if(!http.begin(client,url))return;
  http.addHeader("Accept-Encoding","identity");
  if(http.GET()!=HTTP_CODE_OK) {
    http.end(); return;
  }
  // Keep only event keys needed for verified penalties. Filtering in the
  // stream avoids allocating the complete (occasionally large) game landing.
  DynamicJsonDocument filter(2048);
  JsonObject summary = filter["summary"].to<JsonObject>();
  JsonObject penaltyPeriod = summary["penalties"][0].to<JsonObject>();
  penaltyPeriod["periodDescriptor"]["number"]=true;
  JsonObject penalty = penaltyPeriod["penalties"][0].to<JsonObject>();
  penalty["timeInPeriod"]=true;
  penalty["type"]=true;
  penalty["duration"]=true;
  penalty["descKey"]=true;
  penalty["teamAbbrev"]["default"]=true;
  JsonObject scorePeriod = summary["scoring"][0].to<JsonObject>();
  scorePeriod["periodDescriptor"]["number"]=true;
  JsonObject goal=scorePeriod["goals"][0].to<JsonObject>();
  goal["timeInPeriod"]=true;
  goal["strength"]=true;
  goal["teamAbbrev"]["default"]=true;
  DynamicJsonDocument doc(14336);
  if(filter.overflowed()||!doc.capacity()) {
    http.end();return;
  }
  Stream &body=http.getStream();
  body.setTimeout(4200);
  const DeserializationError err=deserializeJson(
      doc,body,DeserializationOption::Filter(filter));
  http.end();
  if(err||doc.overflowed()) {
    Serial.printf("[PP VERIFY] NHL landing parse: %s\n",err.c_str());
    return;
  }
  const JsonArrayConst penalties=doc["summary"]["penalties"].as<JsonArrayConst>();
  const JsonArrayConst scoring=doc["summary"]["scoring"].as<JsonArrayConst>();
  if(penalties.isNull() && scoring.isNull())return;
  NhlPenaltyLedger::Book next;
  next.reset(target->id);
  for (JsonVariantConst period : penalties) {
    const uint8_t p=period["periodDescriptor"]["number"]|0;
    for (JsonVariantConst call : period["penalties"].as<JsonArrayConst>()) {
      const String team=call["teamAbbrev"]["default"].as<String>();
      const uint8_t penalized=team==target->home ? 1 :
          team==target->away ? 2 : 0;
      next.addPenalty(p,call["timeInPeriod"]|"",
          penalized,call["type"]|"",call["duration"]|0,
          call["descKey"]|"");
    }
  }
  for (JsonVariantConst period : scoring) {
    const uint8_t p=period["periodDescriptor"]["number"]|0;
    for (JsonVariantConst goal : period["goals"].as<JsonArrayConst>()) {
      const char *strength=goal["strength"]|"";
      if(strcmp(strength,"pp") && strcmp(strength,"PP"))continue;
      const String team=goal["teamAbbrev"]["default"].as<String>();
      const uint8_t scoringSide=team==target->home ? 1 :
          team==target->away ? 2 : 0;
      next.addPpGoal(p,goal["timeInPeriod"]|"",scoringSide);
    }
  }
  next.loaded=true;
  next.checkedAt=millis();
  *book=next;
  const uint8_t oldPP=target->ppSide;
  validatePowerPlay(*target);
  if(oldPP!=target->ppSide && screen==Screen::LIVE &&
     currentCarouselPage().kind==LiveCarousel::Kind::GAME &&
     &games[currentCarouselPage().index]==target && !overlayController.active()) {
    partialLiveUpdate=true;
  }
  Serial.printf("[PP VERIFY] game=%lu penalties=%u PPG=%u confirmed=%u emptyNet=%u\n",
      (unsigned long)target->id,next.penaltyCount,next.goalCount,
      target->ppSide,target->emptyNetSide);
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
  gameFilter["situation"]["homeTeam"]["goaliePulled"] = true;
  gameFilter["situation"]["awayTeam"]["goaliePulled"] = true;
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
  const LiveCarousel::Page oldSlide = currentCarouselPage();
  const LiveCarousel::Kind oldSlideKind = oldSlide.kind;
  const bool previouslyLiveMode = carouselPlan.liveMode;
  const uint8_t previousStandingsIndex = oldSlide.kind ==
      LiveCarousel::Kind::STANDINGS ? oldSlide.index : 0;
  const uint8_t previousGameIndex = oldSlide.kind == LiveCarousel::Kind::GAME
      ? oldSlide.index : oldSlide.kind == LiveCarousel::Kind::SUMMARY &&
        oldSlide.count ? oldSlide.rows[0] : 255;
  const uint32_t previouslyShownGameId =
      previousGameIndex < gameCount ? games[previousGameIndex].id : 0;
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
  bool liveSituationChanged = false;
  gameCount = 0;
  for (JsonObject item : arr) {
    if (gameCount >= MAX_GAMES) break;
    Game &g = games[gameCount++];
    const uint32_t priorId=g.id;
    const uint8_t priorPP=g.ppSide, priorEmpty=g.emptyNetSide;
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
    g.ppCandidate = 0;
    g.ppSituationKnown = false;
    g.ppDataKnown = false;
    g.ppUpdatedAt = 0;
    g.emptyNetSide = 0;
    g.emptyNetKnown = false;
    g.emptyNetUpdatedAt = 0;
    g.homeScore = item["homeTeam"]["score"] | 0;
    g.awayScore = item["awayTeam"]["score"] | 0;
    g.homeShots = item["homeTeam"]["sog"] | -1;
    g.awayShots = item["awayTeam"]["sog"] | -1;
    JsonVariantConst situation = item["situation"];
    if (!situation.isNull() && (g.state == "LIVE" || g.state == "CRIT") &&
        !g.intermission) {
      g.ppCandidate = detectPowerPlay(situation);
      g.ppSituationKnown = true;
      // Six skaters (or NHL's explicit goaliePulled flag) means a goalie
      // has been pulled. This is a separate badge, NOT a power play.
      g.emptyNetSide = detectEmptyNet(situation);
      g.emptyNetKnown = true;
      g.emptyNetUpdatedAt = millis();
    }
    validatePowerPlay(g);
    if (priorId != g.id || priorPP != g.ppSide ||
        priorEmpty != g.emptyNetSide) liveSituationChanged = true;
  }
  if (newDay) {
    featuredIndex = 0;
    carouselSlide = 0;
    lastCarouselAdvance = millis();
    needDetailLoad = false;
    detail.loaded = false;
    for (auto &entry : carouselStats) entry = CarouselStats();
    standingsReady = false;
    standingsLastAttempt = 0;
    standingsForDate = "";
    standingsError = "";
    standingsLastHttpCode = 0;
    standingsLastTeamCount = 0;
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
  // Preserve the currently visible *game ID* across NHL feed reordering
  // and when games transition from FUT -> LIVE -> FINAL. A former full
  // game page can become a 4-up summary, or vice versa, without resetting
  // the armed game. A live slate intentionally hides standings slides.
  carouselPlan = LiveCarousel::build(games, gameCount, cfg.showStandings,
                                     cfg.favoriteTeams);
  carouselSlide = newDay ? 0 : LiveCarousel::restore(
      carouselPlan, games, oldSlideKind, previouslyShownGameId,
      previousStandingsIndex, featuredIndex);
  const LiveCarousel::Page visibleSlide = currentCarouselPage();
  if (visibleSlide.kind == LiveCarousel::Kind::GAME)
    featuredIndex = visibleSlide.index;
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
  // Goal detection owns both hardware dispatch and the event log, ensuring
  // each observed score increment is recorded exactly once.
  detectRearGoalChanges();
  observePowerPlayEvents();
  // League-wide display-only puck drop and final observations, keyed by NHL ID.
  // An armed matchup retains its existing independent WLED transition engine.
  // Never fabricate an event from the first snapshot after boot or rollover.
  for (uint8_t i=0; i<gameCount; ++i) {
    const Game &observed = games[i];
    StateWatch &watch = stateWatch[i];
    if (watch.id != observed.id) {
      watch = StateWatch();
      watch.id = observed.id;
    }
    if (observed.id && observed.id != selectedGameId &&
        (cfg.displayAllLiveEvents || rearFollows(observed.id)) &&
        watch.baseline) {
      const bool ended = CelebrationEngine::isLive(watch.state) &&
          CelebrationEngine::isFinal(observed.state.c_str());
      const bool started = CelebrationEngine::isPregame(watch.state) &&
          CelebrationEngine::isLive(observed.state.c_str());
      if (started || ended) {
        const String representative = ended
          ? (observed.homeScore > observed.awayScore ? observed.home : observed.away)
          : DisplayEventPolicy::isFavorite(cfg.favoriteTeams, observed.away.c_str())
            ? observed.away : observed.home;
        const GameEventLog::Type type = started
          ? GameEventLog::Type::GAME_START : GameEventLog::Type::FINAL_WIN;
        const EventOverlay::Kind kind = started
          ? EventOverlay::Kind::GAME_START : EventOverlay::Kind::FINAL_WIN;
        GameEventLog::Entry &log=addGameEvent(observed,type,representative,0);
        log.rear=GameEventLog::Rear::NOT_SELECTED;
        queueGameOverlay(&observed,kind,representative,log.sequence,true);
      }
    }
    EventOverlay::copy(watch.state,observed.state.c_str(),sizeof(watch.state));
    watch.baseline=observed.id!=0;
  }
  Game *g = selectedGame();
  if (g) {
    // Trigger game-start and final outcomes only when a previously observed
    // state transitions; booting into a LIVE/FINAL game never replays events.
    const bool homeWon = g->homeScore > g->awayScore;
    const bool favoredWon = cfg.favoriteSide == 0 ? homeWon : !homeWon;
    const CelebrationEngine::Event transition = CelebrationEngine::transition(
        haveBaseline, lastArmedGameState.c_str(), g->state.c_str(), favoredWon);
    if (transition != CelebrationEngine::Event::NONE) {
      const String &fav = cfg.favoriteSide == 0 ? g->home : g->away;
      const GameEventLog::Type eventType =
          transition == CelebrationEngine::Event::GAME_START
              ? GameEventLog::Type::GAME_START :
          transition == CelebrationEngine::Event::FINAL_WIN
              ? GameEventLog::Type::FINAL_WIN
              : GameEventLog::Type::FINAL_LOSS;
      GameEventLog::Entry &logged = addGameEvent(*g, eventType, fav);
      const EventOverlay::Kind kind =
        transition == CelebrationEngine::Event::GAME_START
          ? EventOverlay::Kind::GAME_START
          : transition == CelebrationEngine::Event::FINAL_WIN
            ? EventOverlay::Kind::FINAL_WIN
            : EventOverlay::Kind::FINAL_LOSS;
      queueGameOverlay(g, kind, fav, logged.sequence, true);
      const bool enabled = transition == CelebrationEngine::Event::GAME_START
                           ? cfg.wledGameStart : cfg.wledFinal;
      if (enabled) {
        const uint32_t teamColor = teamRgb(fav);
        if (wledActiveEvent == CelebrationEngine::Event::GOAL) {
          queuedWledEvent = transition;
          queuedWledColor = teamColor;
          queuedWledLogSeq = logged.sequence;
          for (uint8_t i = 0; i < MAX_WLED; ++i)
            if (wledParticipates(i, transition))
              markWledEvent(logged.sequence, i, GameEventLog::Wled::PENDING);
        } else {
          startWledEvent(transition, teamColor, logged.sequence);
        }
      }
    }
    previousHome = g->homeScore;
    previousAway = g->awayScore;
    lastArmedGameState = g->state;
    haveBaseline = true; // First fetch after choosing a game never replays old goals.
  } else {
    haveBaseline = false;
    lastArmedGameState = "";
  }
  if (gamesFingerprint() != previousFingerprint || liveSituationChanged ||
      newDay || previouslyLiveMode != carouselPlan.liveMode ||
      oldSlideKind != visibleSlide.kind) {
    // Summary pages may have several score/status updates in a single poll.
    // Only partial-update a full-screen matchup that remains the same game.
    if (!newDay && !overlayController.active() &&
        previouslyLiveMode == carouselPlan.liveMode &&
        oldSlideKind == LiveCarousel::Kind::GAME &&
        visibleSlide.kind == LiveCarousel::Kind::GAME &&
        screen == Screen::LIVE && gameCount &&
        previouslyShownGameId == games[visibleSlide.index].id)
      partialLiveUpdate = true;
    else {
      partialLiveUpdate = false;
      needsDraw = true;
    }
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
constexpr uint16_t ARENA_PLATE = 0x18C3; // neutral charcoal, readable over team gradients
constexpr uint16_t ARENA_LINE = 0x4228; // muted gray border, not blue
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

// Live, upcoming and final game slides share the same team-color treatment.
// Paint 80 four-pixel-wide RGB565 columns on the 320x240 CYD; there is no
// framebuffer, heap allocation, image fetch, float math or animation timer.
// Logos/score plates overlay this once per slide change. Partial NHL score
// updates still repaint ONLY the opaque text plates below.
void drawTeamGradientBackground(const Game &g) {
  const uint32_t away = teamRgb(g.away);
  const uint32_t home = teamRgb(g.home);
  for (uint16_t x = 0; x < TeamGradient::kWidth; x += TeamGradient::kStep) {
    const uint16_t midpoint = x + TeamGradient::kStep / 2;
    const uint16_t color = TeamGradient::rgb565(
        TeamGradient::colorAt(away, home, midpoint));
    tft.fillRect(x, 0, TeamGradient::kStep, TeamGradient::kHeight, color);
  }
  // Lightweight, low-contrast jersey-mesh dots to suggest the reference
  // image's texture. Draw only the open background around the matchup.
  // Avoid the centre score plate, since changing pixels behind it would be
  // wasted effort and weaken white text contrast.
  for (uint16_t y = 43; y < 162; y += 12) {
    for (uint16_t x = 8 + (y % 24 ? 6 : 0); x < 315; x += 16) {
      if (x >= 68 && x <= 251 && y >= 76 && y <= 145) continue;
      const uint32_t local = TeamGradient::colorAt(away, home, x);
      const uint32_t accent = ((local & 0xFEFEFEUL) >> 1) +
                              ((local & 0xFCFCFCUL) >> 2);
      tft.drawPixel(x, y, TeamGradient::rgb565(accent));
    }
  }
  // Muted broadcast highlights; remain clear of team logos and time plate.
  tft.drawFastHLine(0, 32, 65, TeamGradient::rgb565(
      TeamGradient::colorAt(away, home, 15)));
  tft.drawFastHLine(255, 32, 65, TeamGradient::rgb565(
      TeamGradient::colorAt(away, home, 304)));
}

// Consistent on-device star for scoreboard, detail page and game list.
// Gold = followed (rear RGB + display); teal = armed WLED matchup.
void drawFollowStar(const Game &g, int16_t x=148, int16_t y=15,
                    uint16_t background=ARENA_PLATE) {
  const bool followed=rearFollows(g.id);
  const bool armed=g.id==selectedGameId;
  const uint16_t plate=armed ? TEAL : followed ? 0x4220 : background;
  tft.fillRoundRect(x-22,y-15,44,30,6,plate);
  const int8_t px[10]={0,3,10,5,6,0,-6,-5,-10,-3};
  const int8_t py[10]={-11,-3,-3,2,10,6,10,2,-3,-3};
  const uint16_t color=followed ? 0xFE60 : 0xCE79;
  for (uint8_t i=0;i<10;++i) {
    const uint8_t j=(i+1)%10;
    if (followed) tft.fillTriangle(x,y,x+px[i],y+py[i],
                                  x+px[j],y+py[j],color);
    tft.drawLine(x+px[i],y+py[i],x+px[j],y+py[j],color);
  }
  if (armed) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(WHITE,plate);
    tft.drawString("A",x+15,y-9,1);
    tft.setTextDatum(TL_DATUM);
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
  drawFollowStar(g);
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
// Four upcoming/finished matches per compact slide while NHL games are live.
// No off-screen buffer: one header and four fixed 46px charcoal rows.
// Tap a row for that game's pregame/live/final details.
void drawOtherGamesSummary() {
  const LiveCarousel::Page slide = currentCarouselPage();
  tft.fillScreen(ARENA_BLACK);
  tft.fillRect(0, 0, 320, 31, ARENA_PLATE);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(WHITE, ARENA_PLATE);
  tft.drawString("OTHER GAMES", 8, 7, 2);
  tft.setTextColor(ICE_TEXT, ARENA_PLATE);
  tft.drawString("LIVE " + String(carouselPlan.liveCount), 150, 7, 2);
  tft.setTextDatum(TR_DATUM);
  tft.drawString(String(carouselSlide + 1) + "/" +
                 String(carouselPageCount()), 297, 7, 2);
  tft.drawFastHLine(306, 7, 11, WHITE);
  tft.drawFastHLine(306, 13, 11, WHITE);
  tft.drawFastHLine(306, 19, 11, WHITE);
  for (uint8_t row = 0; row < slide.count; ++row) {
    const uint8_t idx = slide.rows[row];
    if (idx >= gameCount) continue;
    const Game &g = games[idx];
    const int16_t y = 36 + row * 49;
    const uint16_t bg = row % 2 ? 0x18C3 : ARENA_PLATE;
    tft.fillRoundRect(7, y, 306, 46, 5, bg);
    tft.fillRect(11, y + 9, 4, 28, to565(teamRgb(g.away)));
    tft.fillRect(305, y + 9, 4, 28, to565(teamRgb(g.home)));
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(WHITE, bg);
    tft.drawString(g.away, 22, y + 4, 2);
    if (gameIsFuture(g)) {
      tft.setTextColor(ICE_TEXT, bg);
      tft.drawString("VS", 91, y + 4, 2);
      tft.setTextColor(WHITE, bg);
      tft.drawString(g.home, 137, y + 4, 2);
      String start = formattedStart(g.startUTC);
      if (start.length() > 13) start = start.substring(0,13);
      tft.setTextDatum(TR_DATUM);
      tft.drawString(start.length() ? start : String("TBD"), 300, y + 6, 2);
      tft.setTextDatum(TL_DATUM);
      tft.setTextColor(ICE_TEXT, bg);
      tft.drawString("UPCOMING", 22, y + 27, 2);
    } else {
      // Only non-live games are grouped here. This path is normally FINAL,
      // but handles delayed NHL final labels as well.
      tft.drawString(String(g.awayScore), 82, y + 2, 4);
      tft.setTextColor(ICE_TEXT, bg);
      tft.drawString("-", 119, y + 7, 2);
      tft.setTextColor(WHITE, bg);
      tft.drawString(String(g.homeScore), 141, y + 2, 4);
      tft.drawString(g.home, 189, y + 4, 2);
      tft.setTextColor(ICE_TEXT, bg);
      const String status = gameStatus(g);
      tft.drawString(status.length() > 22 ? status.substring(0, 22) : status,
                     22, y + 27, 2);
    }
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(ICE_TEXT, bg);
    tft.drawString("TAP FOR STATS", 299, y + 32, 1);
  }
  tft.setTextDatum(TL_DATUM);
}

void drawStandingsSlide() {
  const uint8_t slideNumber = currentCarouselPage().index;
  const StandingsCarousel::Page page = StandingsCarousel::page(slideNumber);
  tft.fillScreen(ARENA_BLACK);
  tft.fillRect(0, 0, 320, 28, ARENA_PLATE);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(WHITE, ARENA_PLATE);
  tft.drawString("NHL STANDINGS", 8, 6, 2);
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(ICE_TEXT, ARENA_PLATE);
  tft.drawString(String(carouselSlide + 1) + "/" +
                 String(carouselPageCount()), 255, 8, 1);
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
    tft.drawString(standingsLastAttempt ?
        "Retrying NHL standings..." : "Loading NHL standings...", 160, 145, 2);
    tft.setTextColor(ICE_TEXT, ARENA_BLACK);
    String reason = standingsError.isEmpty() ?
                    String("Updates automatically") : standingsError;
    if (reason.length() > 32) reason = reason.substring(0, 32);
    tft.drawString(reason, 160, 175, 2);
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
      tft.drawString("No ranked teams for this page", 160, 145, 2);
    }
  }

  tft.fillRect(0, 229, 320, 11, ARENA_PLATE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(ICE_TEXT, ARENA_PLATE);
  tft.drawString("Swipe for games / more rankings", 160, 234, 1);
  tft.setTextDatum(TL_DATUM);
}

void drawScoreboardStatic(const Game &g) {
  drawTeamGradientBackground(g);
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

  // Team-specific active NHL status beneath the score plate. These short
  // labels are updated with live scores; PP is penalty-verified and EN means
  // the team's goalie is OFF the ice. Both can coexist without confusion.
  if (gameIsLive(g) && !g.intermission) {
    const uint32_t now=millis();
    for(uint8_t i=0;i<2;++i) {
      const uint8_t side=i==0 ? 2 : 1; // left=away, right=home
      const int16_t x=i==0 ? 79 : 179;
      const bool pp=g.ppDataKnown && g.ppSide==side &&
          uint32_t(now-g.ppUpdatedAt)<45000UL;
      const bool en=g.emptyNetKnown &&
          NhlOnIce::teamHasEmptyNet(g.emptyNetSide,side) &&
          uint32_t(now-g.emptyNetUpdatedAt)<45000UL;
      const uint16_t chip=pp ? 0xC3A0 : en ? 0xA145 : ARENA_PLATE;
      tft.fillRoundRect(x, 143, 62, 18, 4, chip);
      if (pp || en) {
        tft.setTextColor(WHITE, chip);
        tft.drawString(pp && en ? "PP+EN" : pp ? "PP" : "EN",
                       x+31, 152, 2);
      }
    }
  } else {
    tft.fillRect(79,143,62,18,ARENA_PLATE);
    tft.fillRect(179,143,62,18,ARENA_PLATE);
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

// Full-screen CYD event takeover. Draw once when the event changes; only
// 5px border accents animate, so NHL/WLED polling stays responsive and no
// 320x240 sprite/framebuffer is required.
void drawOverlayStatic() {
  if (!overlayController.active()) return;
  const EventOverlay::Item &e = overlayController.current();
  const uint32_t color = e.color ? e.color : 0x39777AUL;
  // A team-color backdrop fades through a dark center while keeping all
  // white typography on a neutral opaque plate.
  for (uint16_t x=0; x<320; x+=4) {
    const uint16_t c = TeamGradient::rgb565(
        TeamGradient::colorAt(color, color, x+2));
    tft.fillRect(x, 0, 4, 240, c);
  }
  tft.fillRoundRect(24, 102, 272, 127, 8, 0x1082);
  tft.drawRoundRect(24, 102, 272, 127, 8, 0x5AEB);
  tft.fillRoundRect(92, 32, 136, 72, 9, 0x0841);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(WHITE, 0x1082);
  tft.drawString("NHL GAME EVENT", 160, 18, 2);

  if (cfg.overlayLogos && e.team[0]) {
    // Reuse the official 56px NHL sprite already embedded at build time.
    // No network image request and no extra RAM allocation.
    drawTeamLogo(tft, String(e.team), 132, 38);
  } else {
    tft.setTextColor(WHITE, 0x0841);
    tft.drawString(e.team[0] ? String(e.team) : String("NHL"),
                   160, 69, 4);
  }
  const String title = EventOverlay::label(e.kind);
  tft.setTextColor(WHITE, 0x1082);
  tft.drawString(title, 160, 124,
                 title.length() > 10 ? 2 : 4);

  String description;
  switch(e.kind) {
    case EventOverlay::Kind::GOAL:
      description = String(e.team) + " SCORES!"; break;
    case EventOverlay::Kind::POWER_PLAY_START:
      description = String(e.team) + " MAN ADVANTAGE"; break;
    case EventOverlay::Kind::POWER_PLAY_END:
      description = "BACK AT FULL STRENGTH"; break;
    case EventOverlay::Kind::GAME_START:
      description = String(e.away) + " VS " + String(e.home); break;
    case EventOverlay::Kind::FINAL_WIN:
      description = String(e.team) + " WINS"; break;
    case EventOverlay::Kind::FINAL_LOSS:
      description = String(e.team) + " FALLS"; break;
    default: description = "";
  }
  tft.setTextColor(0xCE79, 0x1082);
  tft.drawString(description, 160, 154, 2);

  if (e.away[0] && e.home[0]) {
    tft.setTextColor(WHITE, 0x1082);
    const String score = String(e.away) + " " + String(e.awayScore) +
                          "  -  " + String(e.homeScore) + " " + String(e.home);
    tft.drawString(score, 160, 186, 2);
  }
  String clock;
  if (e.period) clock = "P" + String(e.period);
  if (e.clock[0]) {
    if (clock.length()) clock += "  ";
    clock += e.clock;
  }
  tft.setTextColor(0xBDF7, 0x1082);
  if (clock.length()) tft.drawString(clock, 160, 213, 2);
  tft.setTextColor(0xC618);
  tft.drawString("X", 303, 20, 2); // Tap upper-right to dismiss.
  tft.setTextDatum(TL_DATUM);
}

#ifdef GLOWDECK_WAVESHARE_S3
// Overlay the live team score on the hockey footage, but keep the arena's
// "GOAL!" sign and sirens unobstructed. WLED and score detection are unchanged.
void drawGoalGifHud(const EventOverlay::Item &e, uint32_t now) {
  const uint16_t teamColor = TeamGradient::rgb565(e.color);
  tft.fillRect(0, 0, 320, 4, teamColor);
  tft.fillRect(0, 236, 320, 4, teamColor);
  tft.fillRect(0, 0, 4, 240, teamColor);
  tft.fillRect(316, 0, 4, 240, teamColor);
  tft.fillRoundRect(18, 203, 284, 32, 6, 0x0841);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(WHITE, 0x0841);
  const String score = String(e.team) + " GOAL!   " + String(e.away) +
    " " + String(e.awayScore) + " - " + String(e.homeScore) +
    " " + String(e.home);
  tft.drawString(score, 160, 218, 2);
  tft.setTextDatum(TL_DATUM);
  const uint16_t remain = overlayController.remaining(now);
  const uint16_t bar = e.duration ? uint16_t(310UL * remain / e.duration) : 0;
  tft.fillRect(5, 232, 310, 3, 0x0841);
  if (bar) tft.fillRect(5, 232, bar, 3, teamColor);
  tft.setTextColor(WHITE);
  tft.drawString("X", 302, 10, 2);
}
#endif

// Called at most once per ~140ms for goals and 240ms for calmer events.
// Colors animate only the perimeter; no full-screen flashing.
void drawOverlayAnimation(uint32_t now) {
  if (!overlayController.active()) return;
  const EventOverlay::Item &e = overlayController.current();
#ifdef GLOWDECK_WAVESHARE_S3
  if (e.kind == EventOverlay::Kind::GOAL && goalGif.active()) {
    if (goalGif.tick(now)) {
      drawGoalGifHud(e, now);
      overlayLastFrame = now;
    }
    return;
  }
#endif
  const uint32_t elapsed = now - overlayController.startedAt();
  uint8_t brightness = 72;
  if (EventOverlay::isFlash(e.kind)) {
    if (cfg.overlayFlash && elapsed < 1100UL)
      brightness = ((elapsed / 180UL) % 2UL) ? 38 : 100;
    else brightness = 82;
  } else if (EventOverlay::isPulse(e.kind)) {
    // Triangle wave in 1.6s using only integer operations.
    const uint32_t phase = elapsed % 1600UL;
    const uint32_t up = phase < 800UL ? phase : (1600UL-phase);
    brightness = (uint8_t)(35UL + up * 60UL / 800UL);
  } else {
    const uint32_t phase = elapsed % 2000UL;
    const uint32_t up = phase < 1000UL ? phase : (2000UL-phase);
    brightness = (uint8_t)(52UL + up * 42UL / 1000UL);
  }
  const uint32_t r = TeamGradient::scale(
      (uint8_t)((e.color>>16)&255), brightness);
  const uint32_t g = TeamGradient::scale(
      (uint8_t)((e.color>>8)&255), brightness);
  const uint32_t b = TeamGradient::scale(
      (uint8_t)(e.color&255), brightness);
  const uint16_t c = TeamGradient::rgb565((r<<16)|(g<<8)|b);
  tft.fillRect(0, 0, 320, 5, c);
  tft.fillRect(0, 235, 320, 5, c);
  tft.fillRect(0, 0, 5, 240, c);
  tft.fillRect(315, 0, 5, 240, c);
  // Progress stripe keeps the expected return time visually obvious.
  const uint32_t remain = overlayController.remaining(now);
  const uint16_t progress = e.duration
      ? (uint16_t)(310UL * remain / e.duration) : 0;
  tft.fillRect(5, 230, 310, 3, 0x1082);
  if (progress) tft.fillRect(5, 230, progress, 3, c);
  overlayLastFrame = now;
}

#ifdef GLOWDECK_WAVESHARE_S3
// Native 480x320 "Arena Broadcast" surface. The CYD's 320x240 renderer,
// stats, standings, and the goal GIF remain unchanged until later phases.
constexpr uint16_t S3_BG = 0x0842;
constexpr uint16_t S3_NAV = 0x10C4;
constexpr uint16_t S3_CARD = 0x18E6;
constexpr uint16_t S3_DARK = 0x1083;
constexpr uint16_t S3_LINE = 0x31E9;
constexpr uint16_t S3_MUTED = 0xB5D5;
constexpr uint16_t S3_AQUA = 0x36DD;

void drawS3Nav(const char *label) {
  const S3NativeLayout::Rect prev = S3NativeLayout::kPrev;
  const S3NativeLayout::Rect next = S3NativeLayout::kNext;
  const S3NativeLayout::Rect all = S3NativeLayout::kGames;
  tft.fillRect(0, 271, 480, 49, S3_BG);
  tft.setTextDatum(MC_DATUM);
  tft.fillRoundRect(prev.x,prev.y,prev.w,prev.h,8,S3_NAV);
  tft.fillRoundRect(next.x,next.y,next.w,next.h,8,S3_NAV);
  tft.fillRoundRect(all.x,all.y,all.w,all.h,8,S3_CARD);
  tft.setTextColor(WHITE,S3_NAV);
  tft.drawString("< PREV",prev.x+prev.w/2,prev.y+prev.h/2,2);
  tft.drawString("NEXT >",next.x+next.w/2,next.y+next.h/2,2);
  tft.setTextColor(S3_AQUA,S3_CARD);
  tft.drawString(label,all.x+all.w/2,all.y+all.h/2,2);
  tft.setTextDatum(TL_DATUM);
}
void drawS3Header(const Game *g,const String &title) {
  tft.fillRect(0,0,480,48,S3_NAV);
  tft.fillRect(0,46,480,2,S3_AQUA);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(WHITE,S3_NAV);
  tft.drawString("GLOWDECK",14,22,2);
  tft.setTextColor(S3_MUTED,S3_NAV);
  tft.drawString(title,124,22,2);
  if(g) {
    const uint16_t statusColor=gameIsLive(*g)?0xB124:
        (gameIsFuture(*g)?0x25B7:0x39C7);
    tft.fillRoundRect(268,9,71,30,7,statusColor);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(WHITE,statusColor);
    tft.drawString(gameIsLive(*g)?"LIVE":gameIsFuture(*g)?"NEXT":"FINAL",
                   303,24,2);
    drawFollowStar(*g,398,24,S3_NAV);
  } else {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(S3_MUTED,S3_NAV);
    tft.drawString("NHL",306,23,2);
  }
  const S3NativeLayout::Rect menu=S3NativeLayout::kMenu;
  tft.fillRoundRect(menu.x,menu.y,menu.w,menu.h,8,S3_CARD);
  tft.drawFastHLine(menu.x+14,menu.y+13,21,WHITE);
  tft.drawFastHLine(menu.x+14,menu.y+21,21,WHITE);
  tft.drawFastHLine(menu.x+14,menu.y+29,21,WHITE);
  tft.setTextDatum(TL_DATUM);
}
void drawS3Readout(const Game &g) {
  // Opaque plates are the only repainted content during a score/clock update.
  // Team gradient, official logos, labels and touch affordances remain intact.
  tft.setTextDatum(MC_DATUM);
  tft.fillRoundRect(171,61,138,31,7,S3_DARK);
  tft.setTextColor(S3_AQUA,S3_DARK);
  String clock=broadcastTime(g);
  if(clock.length()>20)clock=clock.substring(0,20);
  tft.drawString(clock,240,76,clock.length()>15?2:4);

  tft.fillRoundRect(154,96,172,94,10,S3_DARK);
  if (gameIsFuture(g)) {
    tft.setTextColor(WHITE,S3_DARK);
    tft.drawString("VS",240,137,4);
    tft.setTextColor(S3_MUTED,S3_DARK);
    tft.drawString("UPCOMING",240,174,2);
  } else {
    tft.setTextColor(WHITE,S3_DARK);
    // Large, true-resolution numbers. Two-digit scores scale down to fit.
    tft.setTextSize(g.awayScore>9||g.homeScore>9?1:2);
    tft.drawString(String(g.awayScore),206,133,4);
    tft.drawString(String(g.homeScore),274,133,4);
    tft.setTextSize(1);
    tft.setTextColor(S3_MUTED,S3_DARK);
    tft.drawString("-",240,134,4);
    const bool freshPp = g.ppDataKnown &&
      uint32_t(millis()-g.ppUpdatedAt)<45000UL && gameIsLive(g) && !g.intermission;
    const bool freshEn = g.emptyNetKnown &&
      uint32_t(millis()-g.emptyNetUpdatedAt)<45000UL && gameIsLive(g) && !g.intermission;
    for (uint8_t i=0;i<2;++i) {
      const uint8_t side=i==0?2:1;
      const bool pp=freshPp && g.ppSide==side;
      const bool en=freshEn && NhlOnIce::teamHasEmptyNet(g.emptyNetSide,side);
      if(pp||en) {
        tft.setTextColor(pp?ORANGE:S3_AQUA,S3_DARK);
        tft.drawString(pp&&en?"PP+EN":pp?"POWER PLAY":"EMPTY NET",
                       i==0?196:284,174,1);
      }
    }
  }
  tft.fillRoundRect(14,220,452,45,8,S3_CARD);
  tft.drawFastVLine(240,227,31,S3_LINE);
  tft.setTextColor(S3_MUTED,S3_CARD);
  tft.drawString("AWAY SHOTS",95,233,2);
  tft.drawString("HOME SHOTS",338,233,2);
  tft.setTextColor(WHITE,S3_CARD);
  tft.drawString(g.awayShots>=0?String(g.awayShots):String("--"),195,242,4);
  tft.drawString(g.homeShots>=0?String(g.homeShots):String("--"),438,242,4);
  tft.setTextDatum(TL_DATUM);
}
void drawS3Scoreboard(const Game &g) {
  const uint32_t away=teamRgb(g.away), home=teamRgb(g.home);
  tft.fillScreen(S3_BG);
  // 8-pixel gradient strips preserve each team's recognizable primary color
  // without compromising the high-contrast score plate.
  for(int x=0;x<480;x+=8) {
    const uint16_t sample=uint16_t(uint32_t(x+4)*319UL/479UL);
    const uint16_t c=TeamGradient::rgb565(TeamGradient::colorAt(away,home,sample));
    tft.fillRect(x,48,8,172,c);
  }
  tft.fillRoundRect(12,53,456,160,12,S3_CARD);
  tft.drawRoundRect(12,53,456,160,12,S3_LINE);
  tft.fillRoundRect(19,60,7,146,3,to565(away));
  tft.fillRoundRect(454,60,7,146,3,to565(home));
  drawTeamLogo(tft,g.away,69,97);
  drawTeamLogo(tft,g.home,355,97);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(WHITE,S3_CARD);
  tft.drawString(g.away,97,184,4);
  tft.drawString(g.home,383,184,4);
  tft.setTextColor(S3_MUTED,S3_CARD);
  tft.drawString("AWAY",97,205,1);
  tft.drawString("HOME",383,205,1);
  tft.setTextDatum(TL_DATUM);
  drawS3Readout(g);
  drawS3Header(&g,String(carouselSlide+1)+"/"+String(carouselPageCount()));
  drawS3Nav("ALL GAMES");
}
void drawS3GameRow(const Game &g, uint8_t row) {
  const S3NativeLayout::Rect r=S3NativeLayout::summaryRow(row);
  const uint16_t surface=row%2?S3_DARK:S3_CARD;
  tft.fillRoundRect(r.x,r.y,r.w,r.h,9,surface);
  tft.fillRect(r.x+2,r.y+7,4,r.h-14,
               g.id==selectedGameId?S3_AQUA:to565(teamRgb(g.away)));
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(WHITE,surface);
  tft.drawString(g.away,70,r.y+17,4);
  tft.drawString(g.home,340,r.y+17,4);
  if(gameIsFuture(g)) {
    tft.setTextColor(S3_AQUA,surface);
    tft.drawString("VS",219,r.y+16,4);
  } else {
    tft.drawString(String(g.awayScore),171,r.y+14,4);
    tft.setTextColor(S3_MUTED,surface);
    tft.drawString("-",219,r.y+16,2);
    tft.setTextColor(WHITE,surface);
    tft.drawString(String(g.homeScore),268,r.y+14,4);
  }
  tft.setTextColor(S3_MUTED,surface);
  String status=gameIsFuture(g)?formattedStart(g.startUTC):gameStatus(g);
  if(g.id==selectedGameId)status="ARMED  /  "+status;
  if(status.length()>34)status=status.substring(0,34);
  tft.drawString(status,242,r.y+38,1);
  drawFollowStar(g,444,r.y+24,surface);
  tft.setTextDatum(TL_DATUM);
}
void drawS3Summary() {
  const LiveCarousel::Page slide=currentCarouselPage();
  tft.fillScreen(S3_BG);
  drawS3Header(nullptr,"OTHER GAMES");
  for(uint8_t row=0;row<slide.count;++row) {
    const uint8_t index=slide.rows[row];
    if(index<gameCount)drawS3GameRow(games[index],row);
  }
  drawS3Nav("ALL GAMES");
}
void drawS3GamePicker() {
  if(gameCount && uint16_t(gamePage)*4>=gameCount)gamePage=0;
  tft.fillScreen(S3_BG);
  const uint8_t pages=(gameCount+3)/4;
  drawS3Header(nullptr,"GAMES "+String(gamePage+1)+"/"+
               String(pages?pages:1));
  for(uint8_t row=0;row<4;++row) {
    const uint8_t index=gamePage*4+row;
    if(index<gameCount)drawS3GameRow(games[index],row);
  }
  if(!gameCount) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(S3_MUTED,S3_BG);
    tft.drawString("No games loaded yet",240,160,4);
    tft.setTextDatum(TL_DATUM);
  }
  drawS3Nav("TODAY");
}
// Phase 3: native game comparisons and standings. Each screen reads the
// existing NHL caches; no new network fetches or game-arm side effects.
void drawS3StatsActions(bool armed) {
  const S3NativeLayout::Rect back=S3NativeLayout::kPrev;
  const S3NativeLayout::Rect arm=S3NativeLayout::kGames;
  const S3NativeLayout::Rect setup=S3NativeLayout::kNext;
  tft.fillRect(0,271,480,49,S3_BG);
  tft.setTextDatum(MC_DATUM);
  tft.fillRoundRect(back.x,back.y,back.w,back.h,8,S3_NAV);
  tft.fillRoundRect(arm.x,arm.y,arm.w,arm.h,8,armed?S3_AQUA:S3_CARD);
  tft.fillRoundRect(setup.x,setup.y,setup.w,setup.h,8,S3_NAV);
  tft.setTextColor(WHITE,S3_NAV);
  tft.drawString("< BACK",back.x+back.w/2,back.y+back.h/2,2);
  tft.drawString("SETUP",setup.x+setup.w/2,setup.y+setup.h/2,2);
  tft.setTextColor(armed?S3_BG:S3_AQUA,armed?S3_AQUA:S3_CARD);
  tft.drawString(armed?"ARMED":"ARM GOALS",
                 arm.x+arm.w/2,arm.y+arm.h/2,2);
  tft.setTextDatum(TL_DATUM);
}
bool s3ComparableRow(const String &label,const String &away,
                     const String &home) {
  if (label!="SHOTS" && label!="FACEOFFS" && label!="HITS" &&
      label!="BLOCKS" && label!="GIVEAWAYS" && label!="PEN. MIN") return false;
  return away.length() && home.length() &&
         away.indexOf("--")<0 && home.indexOf("--")<0 &&
         away.indexOf('/')<0 && home.indexOf('/')<0;
}
void drawS3Stats() {
  tft.fillScreen(S3_BG);
  const Game *g=detailGame();
  drawS3Header(g,"GAME STATS");
  if(!g) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(S3_MUTED,S3_BG);
    tft.drawString("Game unavailable",240,151,4);
    tft.setTextDatum(TL_DATUM);
    drawS3StatsActions(false);
    return;
  }
  const uint16_t awayColor=to565(teamRgb(g->away));
  const uint16_t homeColor=to565(teamRgb(g->home));
  tft.fillRoundRect(12,54,456,76,10,S3_CARD);
  tft.fillRect(14,68,4,48,awayColor);
  tft.fillRect(462,68,4,48,homeColor);
  drawTeamLogo(tft,g->away,37,62);
  drawTeamLogo(tft,g->home,387,62);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(WHITE,S3_CARD);
  tft.drawString(g->away,136,86,4);
  tft.drawString(g->home,344,86,4);
  const String score=gameIsFuture(*g)?"VS":
      String(g->awayScore)+" - "+String(g->homeScore);
  tft.drawString(score,240,77,4);
  tft.setTextColor(S3_AQUA,S3_CARD);
  String state=gameStatus(*g);
  if(state.length()>30)state=state.substring(0,30);
  tft.drawString(state,240,113,2);
  const bool haveStats=detail.loaded && detail.gameId==g->id && detail.count>0;
  if(haveStats) {
    for(uint8_t row=0;row<detail.count && row<5;++row) {
      const int16_t y=134+row*27;
      const uint16_t plate=(row&1)?S3_DARK:S3_CARD;
      tft.fillRoundRect(12,y,456,25,5,plate);
      tft.setTextColor(WHITE,plate);
      tft.drawString(detail.away[row],96,y+11,2);
      tft.drawString(detail.home[row],384,y+11,2);
      tft.setTextColor(S3_MUTED,plate);
      tft.drawString(detail.label[row],240,y+10,2);
      if (!gameIsFuture(*g) &&
          s3ComparableRow(detail.label[row],detail.away[row],detail.home[row])) {
        const float aw=detail.away[row].toFloat();
        const float hm=detail.home[row].toFloat();
        const float total=aw+hm;
        if(aw>=0 && hm>=0 && total>0) {
          const uint16_t awayWidth=uint16_t(80.0f*aw/total);
          const uint16_t homeWidth=uint16_t(80.0f*hm/total);
          tft.fillRect(54,y+22,80,2,S3_LINE);
          tft.fillRect(346,y+22,80,2,S3_LINE);
          if(awayWidth)tft.fillRect(54,y+22,awayWidth,2,awayColor);
          if(homeWidth)tft.fillRect(346,y+22,homeWidth,2,homeColor);
        }
      }
    }
  } else {
    tft.fillRoundRect(12,137,456,126,8,S3_CARD);
    tft.setTextColor(WHITE,S3_CARD);
    tft.drawString(detail.loaded?
       (detail.error.length()?detail.error:String("No stats available")):
       String("Loading team stats..."),240,187,2);
    tft.setTextColor(S3_MUTED,S3_CARD);
    tft.drawString(gameIsFuture(*g)?
       "Season comparison updates before puck drop":
       "NHL gamecenter data updates automatically",240,215,2);
  }
  tft.setTextDatum(TL_DATUM);
  drawS3StatsActions(g->id==selectedGameId);
}
void drawS3Standings() {
  const StandingsCarousel::Page page=StandingsCarousel::page(
      currentCarouselPage().index);
  tft.fillScreen(S3_BG);
  drawS3Header(nullptr,"NHL STANDINGS");
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(WHITE,S3_BG);
  tft.drawString(page.title,17,65,2);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(S3_AQUA,S3_BG);
  tft.drawString(page.range,464,65,2);
  tft.fillRoundRect(12,83,456,20,4,S3_CARD);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(S3_MUTED,S3_CARD);
  tft.drawString("#",37,93,2);
  tft.drawString("TEAM",109,93,2);
  tft.drawString("GP",217,93,2);
  tft.drawString("W-L-OT",331,93,2);
  tft.drawString("PTS",438,93,2);

  const String localDay=deviceLocalDate();
  const bool ready=standingsReady &&
                   (!localDay.length() || standingsForDate==localDay);
  if(!ready) {
    tft.setTextColor(WHITE,S3_BG);
    tft.drawString(standingsLastAttempt?
       "Refreshing NHL standings...":"Loading NHL standings...",
       240,160,2);
    tft.setTextColor(S3_MUTED,S3_BG);
    String reason=standingsError.length()?standingsError:
                  String("Waiting for NHL standings data");
    if(reason.length()>40)reason=reason.substring(0,40);
    tft.drawString(reason,240,199,2);
  } else {
    uint8_t sorted[32]={};
    const uint8_t n=StandingsCarousel::collectAndSort(
       seasonStandings,32,page,sorted,32);
    const Game *armed=selectedGame();
    for(uint8_t row=0;row<StandingsCarousel::kRowsPerPage;++row) {
      const uint8_t position=page.offset+row;
      if(position>=n)break;
      const SeasonStanding &entry=seasonStandings[sorted[position]];
      const int16_t y=106+row*20;
      const uint16_t plate=(row&1)?S3_DARK:S3_CARD;
      tft.fillRoundRect(12,y,456,19,3,plate);
      if(armed && (entry.team==armed->away || entry.team==armed->home))
        tft.fillRect(12,y,4,19,S3_AQUA);
      tft.fillRect(65,y+3,5,13,to565(teamRgb(entry.team)));
      tft.setTextColor(WHITE,plate);
      tft.drawString(String(StandingsCarousel::rank(
          page.group,entry.conferenceSequence,entry.divisionSequence)),
          37,y+10,2);
      tft.drawString(entry.team,112,y+10,2);
      tft.setTextColor(S3_MUTED,plate);
      tft.drawString(String(entry.gamesPlayed),217,y+10,2);
      tft.drawString(entry.seasonRecord,331,y+10,2);
      tft.setTextColor(WHITE,plate);
      tft.drawString(entry.points.length()?entry.points:String("--"),
                     438,y+10,2);
    }
    if(!n) {
      tft.setTextColor(S3_MUTED,S3_BG);
      tft.drawString("No ranked teams for this page",240,167,2);
    }
  }
  tft.setTextDatum(TL_DATUM);
  drawS3Nav("ALL GAMES");
}
void drawS3Empty() {
  tft.fillScreen(S3_BG);
  drawS3Header(nullptr,"NHL TODAY");
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(WHITE,S3_BG);
  tft.drawString("NHL GAMEDAY",240,117,4);
  tft.setTextColor(S3_MUTED,S3_BG);
  String message=lastError.length()?lastError:
      scoreboardLoaded?String("No NHL games scheduled"):String("Loading today's games...");
  if(message.length()>49)message=message.substring(0,49);
  tft.drawString(message,240,167,2);
  tft.drawString(targetScoreboardDate.length()?targetScoreboardDate:scoreboardDate,
                 240,199,2);
  tft.setTextDatum(TL_DATUM);
  drawS3Nav("ALL GAMES");
}
#endif

void draw() {
  needsDraw = false;
#ifdef GLOWDECK_WAVESHARE_S3
  // Only the S3 live-game and compact-summary surfaces are native in phases 1/2.
  // Old event overlays, standings, stats and setup keep compatibility scaling.
  tft.setNativeLayout(!overlayController.active() &&
      (screen == Screen::LIVE || screen == Screen::GAMES ||
       screen == Screen::STATS));
#endif
  if (overlayController.active()) {
#ifdef GLOWDECK_WAVESHARE_S3
    const EventOverlay::Item &e = overlayController.current();
    if (e.kind == EventOverlay::Kind::GOAL && goalGif.available()) {
      if (!goalGif.active() || gifOverlayStart != overlayController.startedAt()) {
        goalGif.stop();
        gifOverlayStart = overlayController.startedAt();
        goalGif.start(tft);
      }
      if (goalGif.active()) {
        drawOverlayAnimation(millis());
        return;
      }
    }
    goalGif.stop();
#endif
    drawOverlayStatic();
    drawOverlayAnimation(millis());
    return;
  }
#ifdef GLOWDECK_WAVESHARE_S3
  if (goalGif.active()) goalGif.stop();
#endif
  if (screen == Screen::CALIBRATE) { drawCalibration(); return; }
  if (screen == Screen::LIVE) {
#ifdef GLOWDECK_WAVESHARE_S3
    if (tft.nativeLayout()) {
      if (showingStandingsPage()) drawS3Standings();
      else if (showingSummaryPage()) drawS3Summary();
      else if (!gameCount) drawS3Empty();
      else {
        const LiveCarousel::Page page=currentCarouselPage();
        const Game &g=games[page.kind==LiveCarousel::Kind::GAME
                            ? page.index : featuredIndex % gameCount];
        drawS3Scoreboard(g);
      }
      return;
    }
#endif
    if (showingStandingsPage()) {
      drawStandingsSlide();
    } else if (showingSummaryPage()) {
      drawOtherGamesSummary();
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
      const LiveCarousel::Page slide = currentCarouselPage();
      const Game &g = games[slide.kind == LiveCarousel::Kind::GAME
                                ? slide.index : featuredIndex % gameCount];
      drawScoreboardStatic(g);
      drawLiveReadout(g);
    }
    // No persistent TODAY/GAMES/SETUP buttons on the carousel.
    return;
  }

#ifdef GLOWDECK_WAVESHARE_S3
  if (tft.nativeLayout()) {
    if (screen == Screen::GAMES) drawS3GamePicker();
    else if (screen == Screen::STATS) drawS3Stats();
    else return;
    return;
  }
#endif
  tft.fillScreen(BG);
  tft.fillRect(0, 0, 320, 33, PANEL);
  tft.setTextColor(WHITE, PANEL);
  tft.drawString(screen == Screen::STATS ? "GAME STATS" : "GLOWDECK / NHL",
                 8, 8, 2);
  if (screen == Screen::STATS) {
    const Game *g=detailGame();
    if (g) drawFollowStar(*g,148,15,PANEL);
  }
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
      const int16_t y=66+n*40;
      button(8,y,254,36,
             g.away+" "+String(g.awayScore)+" @ "+
             g.home+" "+String(g.homeScore)+" "+
             (g.id == selectedGameId ? "ARM" : g.state),
             g.id == selectedGameId);
      drawFollowStar(g,290,y+18,BG);
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
  // A new game should not inherit an in-progress goal/PP effect.
  scheduleWledRestoreAll();
  selectedGameId = id;
  armedPpLogSeq = 0;
  lastArmedGameState = "";
  haveBaseline = false;
  prefs.putUInt("game", id);
  screen = Screen::LIVE;
  for (uint8_t i = 0; i < gameCount; ++i)
    if (games[i].id == id) { featuredIndex = i; break; }
  const uint8_t selectedSlide = LiveCarousel::locateGame(carouselPlan, games, id);
  if (selectedSlide != 255) carouselSlide = selectedSlide;
  lastCarouselAdvance = millis();
  // Fetch soon when the user arms a new matchup; first response is a baseline.
  lastPoll = millis() - GameDayCadence::kIdleMs;
  needsDraw = true;
}

#ifdef GLOWDECK_WAVESHARE_S3
void handleS3NativeTouch(bool pressed) {
  // All coordinates here are 480x320 hardware pixels, not the CYD's
  // calibrated 320x240 positions. Gesture semantics remain unchanged.
  if (swallowLiveGesture) {
    if (!pressed) { swallowLiveGesture=false; touchWasDown=false; }
    return;
  }
  if (pressed) {
    const TS_Point p=touch.getScreenPoint();
    const int16_t x=constrain(p.x,0,479), y=constrain(p.y,0,319);
    if (!touchWasDown) {
      touchWasDown=true;
      gestureStartX=x;
      gestureStartY=y;
    }
    gestureEndX=x;
    gestureEndY=y;
    return;
  }
  if (!touchWasDown) return;
  touchWasDown=false;
  const int dx=gestureEndX-gestureStartX;
  const int dy=gestureEndY-gestureStartY;
  if(screen==Screen::STATS) {
    const S3NativeLayout::Hit hit=S3NativeLayout::statsHitTest(
        gestureStartX,gestureStartY);
    switch(hit.action) {
      case S3NativeLayout::Action::FOLLOW: {
        const Game *g=detailGame();
        if(g)setGameFollow(g->id,!rearFollows(g->id));
        break;
      }
      case S3NativeLayout::Action::BACK:
        screen=Screen::LIVE;
        swallowLiveGesture=false;
        break;
      case S3NativeLayout::Action::ARM:
        if(detailGameId) {
          selectGame(detailGameId);
          swallowLiveGesture=false;
        }
        break;
      case S3NativeLayout::Action::SETUP:
        screen=Screen::SETUP;
        break;
      case S3NativeLayout::Action::MENU:
        screen=Screen::GAMES;
        break;
      default:break;
    }
    lastCarouselAdvance=millis();
    needsDraw=true;
    return;
  }
  if(screen==Screen::LIVE && showingStandingsPage()) {
    if(carouselPageCount()>1 && S3NativeLayout::carouselSwipe(dx,dy)) {
      advanceCarousel(dx<0?1:-1);
    } else {
      const S3NativeLayout::Hit hit=S3NativeLayout::standingsHitTest(
          gestureStartX,gestureStartY);
      switch(hit.action) {
        case S3NativeLayout::Action::GAMES:
          screen=Screen::GAMES;
          break;
        case S3NativeLayout::Action::PREVIOUS:
          if(carouselPageCount()>1)advanceCarousel(-1);
          break;
        case S3NativeLayout::Action::NEXT:
          if(carouselPageCount()>1)advanceCarousel(1);
          break;
        default:break;
      }
    }
    lastCarouselAdvance=millis();
    needsDraw=true;
    return;
  }
  if(screen==Screen::GAMES) {
    const bool swipe=S3NativeLayout::carouselSwipe(dx,dy);
    const S3NativeLayout::Hit hit=S3NativeLayout::hitTest(
        gestureStartX,gestureStartY,true);
    if(swipe) {
      if(dx<0 && (uint16_t(gamePage)+1)*4<gameCount)++gamePage;
      else if(dx>0 && gamePage)--gamePage;
    } else {
      switch(hit.action) {
        case S3NativeLayout::Action::MENU:
          screen=Screen::SETUP; break;
        case S3NativeLayout::Action::GAMES:
          screen=Screen::LIVE;
          swallowLiveGesture=false; break;
        case S3NativeLayout::Action::PREVIOUS:
          if(gamePage)--gamePage; break;
        case S3NativeLayout::Action::NEXT:
          if((uint16_t(gamePage)+1)*4<gameCount)++gamePage;
          break;
        case S3NativeLayout::Action::SUMMARY_FOLLOW:
        case S3NativeLayout::Action::SUMMARY_DETAILS: {
          const uint8_t index=gamePage*4+hit.row;
          if(index<gameCount) {
            const Game &g=games[index];
            if(hit.action==S3NativeLayout::Action::SUMMARY_FOLLOW)
              setGameFollow(g.id,!rearFollows(g.id));
            else {
              selectGame(g.id);
              // Picker taps are processed on finger release, unlike the old
              // CYD press handler: there is no subsequent release to swallow.
              swallowLiveGesture=false;
            }
          }
          break;
        }
        default: break;
      }
    }
    lastCarouselAdvance=millis();
    needsDraw=true;
    return;
  }
  if (carouselPageCount()>1 && S3NativeLayout::carouselSwipe(dx,dy)) {
    advanceCarousel(dx<0?1:-1);
    lastCarouselAdvance=millis();
    needsDraw=true;
    return;
  }
  const bool summary=showingSummaryPage();
  const S3NativeLayout::Hit hit=S3NativeLayout::hitTest(
      gestureStartX,gestureStartY,summary);
  const LiveCarousel::Page page=currentCarouselPage();
  switch(hit.action) {
    case S3NativeLayout::Action::MENU:
    case S3NativeLayout::Action::GAMES:
      screen=Screen::GAMES;
      break;
    case S3NativeLayout::Action::FOLLOW:
      if(!summary && showingGamePage() && page.index<gameCount) {
        const Game &g=games[page.index];
        setGameFollow(g.id,!rearFollows(g.id));
      }
      break;
    case S3NativeLayout::Action::PREVIOUS:
      if(carouselPageCount()>1)advanceCarousel(-1);
      break;
    case S3NativeLayout::Action::NEXT:
      if(carouselPageCount()>1)advanceCarousel(1);
      break;
    case S3NativeLayout::Action::SUMMARY_FOLLOW:
    case S3NativeLayout::Action::SUMMARY_DETAILS:
      if(summary && hit.row<page.count && page.rows[hit.row]<gameCount) {
        const Game &g=games[page.rows[hit.row]];
        if(hit.action==S3NativeLayout::Action::SUMMARY_FOLLOW)
          setGameFollow(g.id,!rearFollows(g.id));
        else
          showDetail(g.id);
      }
      break;
    case S3NativeLayout::Action::DETAILS:
      if(showingGamePage() && page.index<gameCount)
        showDetail(games[page.index].id);
      break;
    default: break;
  }
  lastCarouselAdvance=millis();
  needsDraw=true;
}
#endif

void handleTouch() {
  const bool pressed = touch.touched();
  if (overlayController.active()) {
    // Suppress ordinary carousel swipes/stat taps during an event. The
    // upper-right corner dismisses the current overlay without changing
    // the previously selected screen or armed NHL game.
    if (!pressed) {
      touchWasDown = false;
    } else if (!touchWasDown && millis() - lastTouch > 160UL) {
      touchWasDown = true;
      lastTouch = millis();
      const TS_Point p = touch.getPoint();
      const int16_t x = constrain((int)map(p.x, cfg.touchX0, cfg.touchX1, 0, 319), 0, 319);
      const int16_t y = constrain((int)map(p.y, cfg.touchY0, cfg.touchY1, 0, 239), 0, 239);
      if (x >= 266 && y <= 39) {
        overlayController.dismiss(millis());
        overlayLastFrame = 0;
        lastCarouselAdvance = millis();
        needsDraw = true;
      }
    }
    return;
  }
#ifdef GLOWDECK_WAVESHARE_S3
  if (screen==Screen::LIVE || screen==Screen::GAMES ||
      screen==Screen::STATS) {
    handleS3NativeTouch(pressed);
    return;
  }
#endif
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
      if (carouselPageCount() > 1 && abs(dx) > 48 && abs(dy) < 90) {
        advanceCarousel(dx < 0 ? 1 : -1);
      } else if (showingGamePage() && gameCount &&
                 GameFollowPolicy::headerStarTap(
                     gestureStartX,gestureStartY,gestureEndX,gestureEndY)) {
        const Game &g=games[currentCarouselPage().index];
        setGameFollow(g.id,!rearFollows(g.id));
      } else if (gestureStartY <= 35 && gestureStartX >= 263) {
        screen = Screen::GAMES; // Menu is available on every slide.
      } else if (!showingSummaryPage() && carouselPageCount() > 1 &&
                 gestureStartY >= 213 && gestureStartX < 58) {
        advanceCarousel(-1);
      } else if (!showingSummaryPage() && carouselPageCount() > 1 &&
                 gestureStartY >= 213 && gestureStartX > 263) {
        advanceCarousel(1);
      } else if (showingSummaryPage()) {
        const LiveCarousel::Page slide = currentCarouselPage();
        if (gestureStartY >= 36 && gestureStartY < 232) {
          const uint8_t row = (gestureStartY - 36) / 49;
          if (row < slide.count && slide.rows[row] < gameCount)
            showDetail(games[slide.rows[row]].id);
        }
      } else if (showingGamePage() && gameCount) {
        showDetail(games[currentCarouselPage().index].id);
      } else if (!showingStandingsPage()) {
        screen = Screen::GAMES;
      } // Standings rows are informational, not game-arm buttons.
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
  // Star in stats view follows without arming or changing the WLED matchup.
  if (screen == Screen::STATS && GameFollowPolicy::headerStarTap(x,y,x,y)) {
    const Game *g=detailGame();
    if (g) setGameFollow(g->id,!rearFollows(g->id));
  } else if (y >= 198) {
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
      if (index < gameCount) {
        const uint32_t id=games[index].id;
        if (GameFollowPolicy::listStarTap(x))
          setGameFollow(id,!rearFollows(id));
        else selectGame(id);
      }
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
  // Read-only, authenticated, page-sized session event log. Never writes
  // NVS/flash or changes the physical CYD display.
  server.on("/api/events", HTTP_GET, []() {
    if (!authorized()) return;
    const int offset = constrain(server.arg("offset").toInt(), 0, 63);
    const int limit = constrain(server.arg("limit").toInt(), 1, 16);
    const String category = server.hasArg("category")
        ? server.arg("category") : String("all");
    DynamicJsonDocument doc(11000);
    if (doc.capacity() < 11000)
      return errorResponse("Event history memory unavailable", 503);
    doc["capacity"] = GameEventLog::kCapacity;
    doc["sessionOnly"] = true;
    doc["timeSynced"] = time(nullptr) > 1700000000;
    doc["offset"] = offset;
    doc["limit"] = limit;
    doc["lastSequence"] = gameEventHistory.lastSequence();
    JsonArray rows = doc.createNestedArray("events");
    int eligible = 0, appended = 0;
    for (uint8_t i = 0; i < gameEventHistory.size(); ++i) {
      const GameEventLog::Entry *e = gameEventHistory.newest(i);
      if (!e || !GameEventLog::matchesCategory(
                     e->type, category.c_str())) continue;
      if (eligible++ < offset || appended >= limit) continue;
      ++appended;
      JsonObject row = rows.createNestedObject();
      row["id"] = e->sequence;
      row["timestamp"] = e->timestamp;
      row["uptime"] = e->uptimeSeconds;
      row["gameId"] = e->gameId;
      row["type"] = GameEventLog::typeLabel(e->type);
      row["home"] = e->home;
      row["away"] = e->away;
      row["team"] = e->team;
      row["homeScore"] = e->homeScore;
      row["awayScore"] = e->awayScore;
      row["period"] = e->period;
      row["goalsDelta"] = e->goalsDelta;
      row["preview"] = e->preview;
      row["rear"] = GameEventLog::rearLabel(e->rear);
      JsonArray devices = row.createNestedArray("devices");
      for (uint8_t d = 0; d < MAX_WLED; ++d) {
        JsonObject device = devices.createNestedObject();
        device["slot"] = d + 1;
        device["result"] = GameEventLog::wledLabel(e->wled[d]);
        device["accepted"] = (e->wledAcknowledgedMask & (1U << d)) != 0;
      }
    }
    doc["total"] = eligible;
    if (doc.overflowed()) return errorResponse("Event response too large", 503);
    sendJson(doc);
  });
  server.on("/api/events/clear", HTTP_POST, []() {
    if (!authorized()) return;
    gameEventHistory.clear();
    for (uint8_t i = 0; i < MAX_WLED; ++i) wledEventOwner[i] = 0;
    armedPpLogSeq = 0;
    queuedWledLogSeq = 0;
    StaticJsonDocument<96> response;
    response["ok"] = true;
    sendJson(response);
  });
  server.on("/api/status", HTTP_GET, []() {
    if (!authorized()) return;
    // New slide/standings metadata should not enlarge the loopTask stack.
    DynamicJsonDocument doc(4096);
    if (doc.capacity() < 4096) return errorResponse("Status memory unavailable", 503);
    doc["version"] = "0.4.1";
    doc["wifiConnected"] = WiFi.status() == WL_CONNECTED;
    doc["ip"] = localAddress();
    doc["apSsid"] = apSsid;
    doc["apIp"] = WiFi.softAPIP().toString();
    doc["mdns"] = mdnsStarted ? mdnsName + ".local" : "";
    doc["selectedId"] = selectedGameId;
    doc["mode"] = cfg.mode;
    doc["wledEvent"] = CelebrationEngine::label(wledActiveEvent);
    doc["wledParticipants"] = wledEligibleCount(
        wledActiveEvent == CelebrationEngine::Event::NONE
        ? CelebrationEngine::Event::GOAL : wledActiveEvent);
    JsonArray wledResults = doc.createNestedArray("wledResults");
    for (uint8_t i = 0; i < MAX_WLED; ++i)
      wledResults.add(wledDeviceResult[i]);
    doc["overlayActive"] = overlayController.active();
    doc["overlayEvent"] = EventOverlay::label(overlayController.current().kind);
    doc["overlayTeam"] = overlayController.current().team;
    doc["overlayQueue"] = overlayController.pending();
    doc["overlayRemainingMs"] = overlayController.remaining(millis());
    doc["overlayCount"] = overlayCount;
    doc["backgroundEventsSkipped"] = backgroundEventsSkipped;
    doc["overlayEventsDropped"] = overlayEventsDropped;
    doc["rearEnabled"] = cfg.rearLedEnabled;
    doc["rearEvent"] = rearEvent == RearEvent::GOAL ? "GOAL" :
                       rearEvent == RearEvent::POWER_PLAY ? "POWER_PLAY" :
                       rearEvent == RearEvent::DIAGNOSTIC ? "RGB_DIAGNOSTIC" : "IDLE";
    JsonArray followed = doc.createNestedArray("followed");
    for (uint8_t i = 0; i < MAX_FOLLOWED_GAMES; ++i)
      if (cfg.followIds[i]) followed.add(cfg.followIds[i]);
    doc["followCapacity"] = MAX_FOLLOWED_GAMES;
    doc["scoreDate"] = scoreboardDate;
    doc["today"] = targetScoreboardDate;
    doc["clockSynced"] = deviceLocalDate().length() == 10;
    doc["scoreboardLoaded"] = scoreboardLoaded;
    doc["featuredIndex"] = featuredIndex;
    doc["carouselSlide"] = carouselSlide;
    doc["carouselTotal"] = carouselPageCount();
    doc["slideStandings"] = showingStandingsPage();
    doc["slideSummary"] = showingSummaryPage();
    doc["liveMode"] = carouselPlan.liveMode;
    doc["liveGameCount"] = carouselPlan.liveCount;
    doc["summaryGameCount"] = carouselPlan.otherCount;
    doc["summaryPageCount"] = carouselPlan.summaryPages;
    doc["summaryPageNumber"] = showingSummaryPage()
        ? currentCarouselPage().index + 1 : 0;
    doc["standingsPage"] = showingStandingsPage()
        ? StandingsCarousel::page(currentCarouselPage().index).title : "";
    doc["standingsReady"] = standingsReady;
    doc["standingsError"] = standingsError;
    doc["standingsHttpCode"] = standingsLastHttpCode;
    doc["standingsTeams"] = standingsLastTeamCount;
    doc["showStandings"] = cfg.showStandings;
    doc["overlayEnabled"] = cfg.overlayEnabled;
    doc["overlayGoals"] = cfg.overlayGoals;
    doc["overlayPowerPlays"] = cfg.overlayPowerPlays;
    doc["overlayStart"] = cfg.overlayStart;
    doc["overlayFinal"] = cfg.overlayFinal;
    doc["overlayFlash"] = cfg.overlayFlash;
    doc["overlayLogos"] = cfg.overlayLogos;
    doc["displayAllLiveEvents"] = cfg.displayAllLiveEvents;
    doc["favoriteTeams"] = cfg.favoriteTeams;
    doc["overlayGoalSeconds"] = cfg.overlayGoalSeconds;
    doc["overlayFinalSeconds"] = cfg.overlayFinalSeconds;
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
    if (doc.overflowed()) return errorResponse("Status response too large", 503);
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
      g["homeName"] = games[i].homeName;
      g["awayName"] = games[i].awayName;
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
      g["powerPlayVerified"] = games[i].ppDataKnown;
      g["emptyNetSide"] = games[i].emptyNetSide;
      g["emptyNetKnown"] = games[i].emptyNetKnown;
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
    DynamicJsonDocument doc(3072);
    if (doc.capacity() < 3072) return errorResponse("Config memory unavailable", 503);
    doc["ssid"] = cfg.ssid;
    doc["mode"] = cfg.mode;
    doc["brightness"] = cfg.brightness;
    doc["seconds"] = cfg.seconds;
    doc["horn"] = cfg.horn;
    doc["wledPP"] = cfg.wledPP;
    doc["wledGameStart"] = cfg.wledGameStart;
    doc["wledFinal"] = cfg.wledFinal;
    doc["wledGoalStyle"] = cfg.wledGoalStyle;
    doc["favoriteSide"] = cfg.favoriteSide;
    doc["rearEnabled"] = cfg.rearLedEnabled;
    doc["rearGoal"] = cfg.rearGoal;
    doc["rearPowerPlay"] = cfg.rearPowerPlay;
    doc["rearBrightness"] = cfg.rearBrightness;
    doc["rearGoalSeconds"] = cfg.rearGoalSeconds;
    doc["screenInverted"] = cfg.invertScreen;
    doc["showStandings"] = cfg.showStandings;
    doc["cycleGames"] = cfg.cycleGames;
    doc["cycleSeconds"] = cfg.cycleSeconds;
    doc["timezone"] = cfg.timezone;
    doc["displayAllLiveEvents"] = cfg.displayAllLiveEvents;
    doc["favoriteTeams"] = cfg.favoriteTeams;
    JsonArray wled = doc.createNestedArray("wled");
    for (uint8_t i = 0; i < MAX_WLED; ++i) {
      JsonObject d = wled.createNestedObject();
      d["ip"] = cfg.wled[i];
      d["enabled"] = cfg.wledEnabled[i];
      d["events"] = cfg.wledEvents[i];
    }
    sendJson(doc);
  });
  server.on("/api/config", HTTP_POST, []() {
    if (!authorized()) return;
    if (server.arg("plain").length() > 2500) return errorResponse("Configuration too large");
    DynamicJsonDocument doc(4096);
    if (doc.capacity() < 4096) return errorResponse("Config memory unavailable", 503);
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
    cfg.wledPP = doc["wledPP"] | cfg.wledPP;
    cfg.wledGameStart = doc["wledGameStart"] | cfg.wledGameStart;
    cfg.wledFinal = doc["wledFinal"] | cfg.wledFinal;
    cfg.wledGoalStyle = constrain((int)(doc["wledGoalStyle"] | cfg.wledGoalStyle), 0, 3);
    cfg.favoriteSide = constrain((int)(doc["favoriteSide"] | cfg.favoriteSide), 0, 1);
    cfg.rearLedEnabled = doc["rearEnabled"] | cfg.rearLedEnabled;
    cfg.rearGoal = doc["rearGoal"] | cfg.rearGoal;
    cfg.rearPowerPlay = doc["rearPowerPlay"] | cfg.rearPowerPlay;
    cfg.rearBrightness = constrain((int)(doc["rearBrightness"] | cfg.rearBrightness), 1, 255);
    cfg.rearGoalSeconds = constrain((int)(doc["rearGoalSeconds"] | cfg.rearGoalSeconds), 2, 12);
    if (!cfg.rearLedEnabled) stopRearLed();
    cfg.overlayEnabled = doc["overlayEnabled"] | cfg.overlayEnabled;
    cfg.overlayGoals = doc["overlayGoals"] | cfg.overlayGoals;
    cfg.overlayPowerPlays = doc["overlayPowerPlays"] | cfg.overlayPowerPlays;
    cfg.overlayStart = doc["overlayStart"] | cfg.overlayStart;
    cfg.overlayFinal = doc["overlayFinal"] | cfg.overlayFinal;
    cfg.overlayFlash = doc["overlayFlash"] | cfg.overlayFlash;
    cfg.overlayLogos = doc["overlayLogos"] | cfg.overlayLogos;
    cfg.displayAllLiveEvents = doc["displayAllLiveEvents"] | cfg.displayAllLiveEvents;
    cfg.favoriteTeams = doc["favoriteTeams"] | cfg.favoriteTeams;
    cfg.overlayGoalSeconds = constrain(
        (int)(doc["overlayGoalSeconds"] | cfg.overlayGoalSeconds), 3, 12);
    cfg.overlayFinalSeconds = constrain(
        (int)(doc["overlayFinalSeconds"] | cfg.overlayFinalSeconds), 3, 12);
    if (!cfg.overlayEnabled) {
      overlayController.clear();
      overlayLastFrame = 0;
      needsDraw = true;
      lastCarouselAdvance = millis();
    }
    cfg.cycleGames = doc["cycleGames"] | cfg.cycleGames;
    const bool newShowStandings = doc["showStandings"] | cfg.showStandings;
    if (newShowStandings != cfg.showStandings) {
      const LiveCarousel::Page old = currentCarouselPage();
      const uint32_t oldId = old.kind == LiveCarousel::Kind::GAME &&
                                 old.index < gameCount ? games[old.index].id :
          old.kind == LiveCarousel::Kind::SUMMARY && old.count &&
            old.rows[0] < gameCount ? games[old.rows[0]].id : 0;
      cfg.showStandings = newShowStandings;
      rebuildCarouselPlan();
      carouselSlide = LiveCarousel::restore(
          carouselPlan, games, old.kind, oldId, old.index, featuredIndex);
      const LiveCarousel::Page restored = currentCarouselPage();
      if (restored.kind == LiveCarousel::Kind::GAME)
        featuredIndex = restored.index;
      lastCarouselAdvance = millis();
      partialLiveUpdate = false;
      needsDraw = true;
    }
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
      const bool enable = arr[i]["enabled"] | cfg.wledEnabled[i];
      const uint8_t mask = constrain((int)(arr[i]["events"] | cfg.wledEvents[i]),
                                      0, CelebrationEngine::ALL_EVENTS);
      if (cfg.wled[i] != ip || cfg.wledEnabled[i] != enable ||
          cfg.wledEvents[i] != mask) {
        if (restoring[i]) {
          restoreAt[i] = millis();
          restoreRetries[i] = 0;
        }
      }
      cfg.wled[i] = ip;
      cfg.wledEnabled[i] = enable;
      cfg.wledEvents[i] = mask;
    }
    if (wledActiveEvent == CelebrationEngine::Event::POWER_PLAY && !cfg.wledPP)
      scheduleWledRestoreAll();
    rebuildCarouselPlan();
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
  server.on("/api/display/favorites", HTTP_POST, []() {
    if (!authorized()) return;
    StaticJsonDocument<128> request;
    if (deserializeJson(request, server.arg("plain")))
      return errorResponse("Invalid favorite teams JSON");
    if (!request.containsKey("mask") || !request["mask"].is<uint32_t>())
      return errorResponse("Favorite teams mask must be an unsigned integer");
    const uint32_t mask = request["mask"].as<uint32_t>();
    if (mask != cfg.favoriteTeams) {
      const LiveCarousel::Page old=currentCarouselPage();
      const uint32_t oldGame=old.kind==LiveCarousel::Kind::GAME &&
          old.index<gameCount ? games[old.index].id : 0;
      cfg.favoriteTeams=mask;
      rebuildCarouselPlan();
      carouselSlide=LiveCarousel::restore(carouselPlan,games,
          old.kind,oldGame,old.index,featuredIndex);
      lastCarouselAdvance=millis();
      partialLiveUpdate=false;
      needsDraw=true;
      saveSettings();
    }
    StaticJsonDocument<96> response;
    response["ok"]=true;
    response["favoriteTeams"]=cfg.favoriteTeams;
    sendJson(response);
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
    if (deserializeJson(doc, server.arg("plain")))
      return errorResponse("Invalid JSON");
    const uint32_t id = doc["id"] | 0U;
    const bool follow = doc["follow"] | false;
    const FollowResult status=setGameFollow(id,follow);
    if (status==FollowResult::INVALID)
      return errorResponse("Choose a game on today's schedule",404);
    if (status==FollowResult::FULL)
      return errorResponse("Daily follow list is full (20 games)",409);
    if (status==FollowResult::ARMED && !follow)
      return errorResponse("Armed game is always followed",409);
    StaticJsonDocument<128> result;
    result["ok"]=true;
    result["followed"]=rearFollows(id);
    result["armed"]=id==selectedGameId;
    sendJson(result);
  });
  // On-device visual test: intentionally does not trigger WLED or rear LED,
  // so it remains usable even before adding external LED controllers.
  server.on("/api/overlay/preview", HTTP_POST, []() {
    if (!authorized()) return;
    StaticJsonDocument<192> request;
    if (deserializeJson(request, server.arg("plain")))
      return errorResponse("Invalid overlay preview JSON");
    const String type = request["kind"] | String("goal");
    const String side = request["side"] | String("home");
    if (side != "home" && side != "away")
      return errorResponse("Invalid team side");
    EventOverlay::Kind kind = EventOverlay::Kind::NONE;
    if (type == "goal") kind = EventOverlay::Kind::GOAL;
    else if (type == "pp") kind = EventOverlay::Kind::POWER_PLAY_START;
    else if (type == "ppend") kind = EventOverlay::Kind::POWER_PLAY_END;
    else if (type == "start") kind = EventOverlay::Kind::GAME_START;
    else if (type == "win") kind = EventOverlay::Kind::FINAL_WIN;
    else if (type == "loss") kind = EventOverlay::Kind::FINAL_LOSS;
    else if (type != "stop") return errorResponse("Invalid overlay event");
    if (kind == EventOverlay::Kind::NONE) {
      overlayController.clear();
      overlayLastFrame = 0;
      needsDraw = true;
      lastCarouselAdvance = millis();
    } else {
      Game *g = selectedGame();
      if (!g && gameCount) g = &games[featuredIndex % gameCount];
      const String team = g ? (side == "home" ? g->home : g->away)
                            : String("UTA");
      if (!overlayAllowed(kind))
        return errorResponse("Enable this event overlay in Display settings");
      const GameEventLog::Type logType = type == "goal"
        ? GameEventLog::Type::PREVIEW_GOAL :
        type == "pp" || type == "ppend"
        ? GameEventLog::Type::PREVIEW_POWER_PLAY :
        type == "start" ? GameEventLog::Type::PREVIEW_START :
        type == "win" ? GameEventLog::Type::PREVIEW_WIN :
                        GameEventLog::Type::PREVIEW_LOSS;
      const uint32_t seq = addPreviewEvent(g, logType, team).sequence;
      queueGameOverlay(g, kind, team, seq);
    }
    StaticJsonDocument<128> response;
    response["ok"] = true;
    response["active"] = overlayController.active();
    response["pending"] = overlayController.pending();
    sendJson(response);
  });
  // UI reads the same palette used for actual observed NHL goal colors.
  server.on("/api/rear/palette", HTTP_GET, []() {
    if (!authorized()) return;
    DynamicJsonDocument doc(4096);
    if (doc.capacity() < 4096) return errorResponse("Palette memory unavailable", 503);
    JsonArray teams = doc.createNestedArray("teams");
    for (uint8_t i = 0; i < TeamPalette::kCount; ++i) {
      JsonObject t = teams.createNestedObject();
      t["abbr"] = TeamPalette::kTeams[i].abbr;
      t["hex"] = rgbHex(TeamPalette::kTeams[i].rgb);
    }
    sendJson(doc);
  });
  server.on("/api/rear/test", HTTP_POST, []() {
    if (!authorized()) return;
    StaticJsonDocument<192> doc;
    if (deserializeJson(doc, server.arg("plain"))) return errorResponse("Invalid JSON");
    const String mode = doc["mode"] | String("goal");
    const String side = doc["side"] | String("home");
    const String channel = doc["channel"] | String("");
    const String requestedTeam = doc["team"] | String("");
    if (mode != "goal" && mode != "powerplay" && mode != "off" && mode != "channel")
      return errorResponse("Invalid effect");
    if (side != "home" && side != "away") return errorResponse("Invalid team side");
    if (mode != "off" && !cfg.rearLedEnabled)
      return errorResponse("Enable the rear LED and save settings first");
    if (mode == "off") {
      stopRearLed();
      GameEventLog::Entry &stopRow = addPreviewEvent(
          nullptr, GameEventLog::Type::REAR_TEST, "");
      stopRow.rear = GameEventLog::Rear::NA;
      StaticJsonDocument<96> response;
      response["ok"] = true;
      response["mode"] = "off";
      sendJson(response);
      return;
    }
    uint32_t color = 0;
    String team;
    Game *g = nullptr;
    if (mode == "channel") {
      if (!TeamPalette::diagnosticRgb(channel.c_str(), color))
        return errorResponse("Choose red, green, or blue for RGB diagnostics");
      team = channel == "red" ? "RED" : channel == "green" ? "GRN" : "BLU";
    } else {
      g = testMatchup();
      if (requestedTeam.length()) {
        if (!TeamPalette::find(requestedTeam.c_str(), color))
          return errorResponse("Unknown NHL team. Choose a team from the palette");
        team = requestedTeam;
      } else {
        if (!g) return errorResponse("No game available; select an NHL team instead", 409);
        team = side == "home" ? g->home : g->away;
        if (!TeamPalette::find(team.c_str(), color))
          return errorResponse("Matchup team is not in the NHL color palette");
      }
      if (mode == "goal" && !cfg.rearGoal)
        return errorResponse("Enable rear goal flashes in CYD LED settings");
    }
    GameEventLog::Entry &testRow = addPreviewEvent(
        g, GameEventLog::Type::REAR_TEST, team);
    testRow.rear = GameEventLog::Rear::ACTIVE;
    if (mode == "channel") {
      rearGoalDuration = 0;
      rearTestPpUntil = 0;
      rearDiagnosticColor = color;
      rearDiagnosticUntil = millis() + 3000UL;
      rearEvent = RearEvent::DIAGNOSTIC;
    } else if (mode == "goal") {
      triggerRearGoal(color);
      queueGameOverlay(requestedTeam.length() ? nullptr : g,
                       EventOverlay::Kind::GOAL, team, testRow.sequence);
    } else {
      rearGoalDuration = 0;
      rearDiagnosticUntil = 0;
      rearTestPpColor = color;
      rearTestPpUntil = millis() + 9000UL;
      rearEvent = RearEvent::IDLE;
      queueGameOverlay(requestedTeam.length() ? nullptr : g,
                       EventOverlay::Kind::POWER_PLAY_START, team, testRow.sequence);
    }
    StaticJsonDocument<160> response;
    response["ok"] = true;
    response["mode"] = mode;
    response["team"] = team;
    response["hex"] = rgbHex(color);
    sendJson(response);
  });
  server.on("/api/wled/preview", HTTP_POST, []() {
    if (!authorized()) return;
    StaticJsonDocument<256> request;
    if (deserializeJson(request, server.arg("plain")))
      return errorResponse("Invalid preview JSON");
    const String type = request["event"] | String("goal");
    const String side = request["side"] | String("home");
    const String requestedTeam = request["team"] | String("");
    const bool combined = request["combined"] | false;
    if (side != "home" && side != "away")
      return errorResponse("Invalid team side");
    CelebrationEngine::Event event = CelebrationEngine::Event::NONE;
    if (type == "goal") event = CelebrationEngine::Event::GOAL;
    else if (type == "pp") event = CelebrationEngine::Event::POWER_PLAY;
    else if (type == "start") event = CelebrationEngine::Event::GAME_START;
    else if (type == "win") event = CelebrationEngine::Event::FINAL_WIN;
    else if (type == "loss") event = CelebrationEngine::Event::FINAL_LOSS;
    else if (type != "stop") return errorResponse("Invalid celebration type");
    Game *previewGame = testMatchup();
    String previewTeam = previewGame
        ? (side == "home" ? previewGame->home : previewGame->away)
        : String("UTA");
    uint32_t color = teamRgb(previewTeam);
    if (event != CelebrationEngine::Event::NONE && requestedTeam.length()) {
      if (!TeamPalette::find(requestedTeam.c_str(), color))
        return errorResponse("Unknown NHL team. Choose a team from the palette");
      previewTeam = requestedTeam;
    } else if (combined && event != CelebrationEngine::Event::NONE && !previewGame) {
      return errorResponse("No game available; choose a team for full preview", 409);
    }
    const GameEventLog::Type previewType =
        type == "goal" ? GameEventLog::Type::PREVIEW_GOAL :
        type == "pp" ? GameEventLog::Type::PREVIEW_POWER_PLAY :
        type == "start" ? GameEventLog::Type::PREVIEW_START :
        type == "win" ? GameEventLog::Type::PREVIEW_WIN :
        type == "loss" ? GameEventLog::Type::PREVIEW_LOSS :
                         GameEventLog::Type::PREVIEW_STOP;
    GameEventLog::Entry &previewRow =
        addPreviewEvent(requestedTeam.length() ? nullptr : previewGame,
                        previewType, previewTeam);
    bool wledStarted = false, rearStarted = false, overlayStarted = false;
    if (event == CelebrationEngine::Event::NONE) {
      wledSuppressPpUntil = millis() + 30000UL;
      scheduleWledRestoreAll();
      tickRestores();
      if (combined) {
        stopRearLed();
        overlayController.clear();
        overlayLastFrame = 0;
        needsDraw = true;
      }
    } else {
      wledSuppressPpUntil = 0;
      wledStarted = startWledEvent(event, color, previewRow.sequence);
      if (wledStarted && event == CelebrationEngine::Event::POWER_PLAY)
        wledPpTestUntil = millis() + 9000UL;
      else
        wledPpTestUntil = 0;
      // Start the rear flash after potentially slow WLED snapshots/POSTs.
      if (combined && cfg.rearLedEnabled) {
        if (event == CelebrationEngine::Event::GOAL && cfg.rearGoal) {
          triggerRearGoal(color);
          rearStarted = true;
        } else if (event == CelebrationEngine::Event::POWER_PLAY && cfg.rearPowerPlay) {
          rearGoalDuration = 0;
          rearDiagnosticUntil = 0;
          rearTestPpColor = color;
          rearTestPpUntil = millis() + 9000UL;
          rearEvent = RearEvent::IDLE;
          rearStarted = true;
        }
        previewRow.rear = rearStarted ? GameEventLog::Rear::ACTIVE
                                    : GameEventLog::Rear::NOT_ENABLED;
      } else if (combined) {
        previewRow.rear = GameEventLog::Rear::NOT_ENABLED;
      }
    }
    const EventOverlay::Kind overlayKind =
        event == CelebrationEngine::Event::GOAL ? EventOverlay::Kind::GOAL :
        event == CelebrationEngine::Event::POWER_PLAY ? EventOverlay::Kind::POWER_PLAY_START :
        event == CelebrationEngine::Event::GAME_START ? EventOverlay::Kind::GAME_START :
        event == CelebrationEngine::Event::FINAL_WIN ? EventOverlay::Kind::FINAL_WIN :
        event == CelebrationEngine::Event::FINAL_LOSS ? EventOverlay::Kind::FINAL_LOSS :
        EventOverlay::Kind::NONE;
    if (overlayKind != EventOverlay::Kind::NONE)
      overlayStarted = queueGameOverlay(requestedTeam.length() ? nullptr : previewGame,
                                       overlayKind, previewTeam, previewRow.sequence);
    StaticJsonDocument<256> response;
    response["event"] = CelebrationEngine::label(wledActiveEvent);
    response["devices"] = wledEligibleCount(event == CelebrationEngine::Event::NONE
        ? CelebrationEngine::Event::GOAL : event);
    response["wledStarted"] = wledStarted;
    response["rearStarted"] = rearStarted;
    response["overlayStarted"] = overlayStarted;
    response["team"] = previewTeam;
    response["hex"] = rgbHex(color);
    response["ok"] = event == CelebrationEngine::Event::NONE ||
        (combined ? (wledStarted || rearStarted || overlayStarted)
                  : wledActiveEvent == event);
    sendJson(response);
  });
  server.on("/api/test", HTTP_POST, []() {
    if (!authorized()) return;
    StaticJsonDocument<160> doc;
    if (deserializeJson(doc, server.arg("plain"))) return errorResponse("Invalid JSON");
    String side = doc["side"] | String("home");
    if (side != "home" && side != "away") return errorResponse("Invalid side");
    Game *g = selectedGame();
    const String team = g ? (side == "home" ? g->home : g->away) : String("UTA");
    const uint32_t seq = addPreviewEvent(
        g, GameEventLog::Type::PREVIEW_GOAL, team).sequence;
    celebrate(teamRgb(team), seq);
    StaticJsonDocument<96> ok; ok["ok"] = true; sendJson(ok);
  });
#ifdef GLOWDECK_WAVESHARE_S3
  server.on("/api/goal-gif", HTTP_GET, []() {
    if (!authorized()) return;
    StaticJsonDocument<192> result;
    result["supported"] = true;
    result["installed"] = goalGif.available();
    result["bytes"] = goalGif.size();
    result["maxBytes"] = GoalGifPlayer::kMaxBytes;
    sendJson(result);
  });
  server.on("/api/goal-gif/upload", HTTP_POST, []() {
    if (!authorized()) return;
    if (!goalUploadFinished || goalUploadFailed) {
      LittleFS.remove("/goal.tmp");
      if (goalUploadError.isEmpty()) return errorResponse("GIF upload incomplete", 400);
      return errorResponse(goalUploadError.c_str(), 400);
    }
    File file = LittleFS.open("/goal.tmp", "r");
    uint8_t header[10] = {};
    const bool valid = file && file.read(header, 10) == 10 &&
                       GoalGifPlayer::validHeader(header);
    file.close();
    if (!valid) {
      LittleFS.remove("/goal.tmp");
      return errorResponse("Upload an animated GIF scaled to exactly 320x240", 400);
    }
    goalGif.stop();
    // Preserve the old GIF if the new upload was interrupted. Existing image
    // is only removed after validation succeeds.
    LittleFS.remove(GoalGifPlayer::kFilename);
    if (!LittleFS.rename("/goal.tmp", GoalGifPlayer::kFilename))
      return errorResponse("Could not install GIF", 500);
    StaticJsonDocument<128> result;
    result["ok"] = true;
    result["bytes"] = goalUploadBytes;
    sendJson(result);
  }, []() {
    HTTPUpload &upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      goalUploadFinished = false;
      goalUploadFailed = !hasDashboardAccess();
      goalUploadError = goalUploadFailed ? "Unauthorized" : "";
      goalUploadBytes = 0;
      if (!goalUploadFailed) {
        if (!goalGif.mounted()) {
          goalUploadFailed = true;
          goalUploadError = "Flash storage unavailable";
        } else {
          LittleFS.remove("/goal.tmp");
          goalUpload = LittleFS.open("/goal.tmp", "w");
          if (!goalUpload) {
            goalUploadFailed = true;
            goalUploadError = "Unable to write flash storage";
          }
        }
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (!goalUploadFailed) {
        if (goalUploadBytes + upload.currentSize > GoalGifPlayer::kMaxBytes ||
            goalUpload.write(upload.buf, upload.currentSize) != upload.currentSize) {
          goalUploadFailed = true;
          goalUploadError = "GIF exceeds 1MB or flash write failed";
        } else goalUploadBytes += upload.currentSize;
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      goalUpload.close();
      goalUploadFinished = !goalUploadFailed && goalUploadBytes >= 16;
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
      goalUpload.close();
      goalUploadFailed = true;
      goalUploadError = "Upload aborted";
      LittleFS.remove("/goal.tmp");
    }
  });
  server.on("/api/goal-gif/remove", HTTP_POST, []() {
    if (!authorized()) return;
    goalGif.stop();
    LittleFS.remove(GoalGifPlayer::kFilename);
    StaticJsonDocument<64> result; result["ok"] = true; sendJson(result);
  });
#endif
  server.on("/api/update", HTTP_POST, []() {
    if (!authorized()) return;
    bool ok = !Update.hasError();
    server.send(ok ? 200 : 500, "text/plain",
                ok ? "Firmware accepted. Rebooting..." : "Firmware update failed");
    if (ok) rebootAt = millis() + 1000;
  }, []() {
    if (!hasDashboardAccess()) return;
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
#ifdef GLOWDECK_WAVESHARE_S3
  Serial.println("\\nGlowDeck GameDay — Waveshare ESP32-S3 3.5-inch (beta)");
#else
  Serial.println("\\nGlowDeck GameDay v0.3.7 — CYD only");
#endif
  Serial.printf("Configured loopTask stack: %u bytes\n", ARDUINO_LOOP_STACK_SIZE);
  Serial.printf("Setup stack watermark: %u, freeHeap: %u\n",
                (unsigned)uxTaskGetStackHighWaterMark(nullptr),
                (unsigned)ESP.getFreeHeap());

#ifdef GLOWDECK_WAVESHARE_S3
  // Waveshare LCD reset is on TCA9554 I/O expander P1, not an ESP GPIO.
  GameDayS3::beginHardware();
#endif
  pinMode(PIN_BL, OUTPUT);
  digitalWrite(PIN_BL, HIGH);
#ifdef GLOWDECK_WAVESHARE_S3
  if (!tft.init()) Serial.println("[S3] Display init or PSRAM allocation failed");
  goalGif.beginStorage();
#else
  tft.init();
#endif
  tft.setRotation(1);
  tft.fillScreen(BG);
  textAt("GlowDeck GameDay", 15, 30, WHITE, 4);
  textAt("Starting local controls...", 15, 68, MUTED);

#ifdef GLOWDECK_WAVESHARE_S3
  touch.begin(); // FT6336 on shared I2C: GPIO8 SDA / GPIO7 SCL.
#else
  touchSPI.begin(25, 39, 32, PIN_TOUCH_CS);
  touch.begin(touchSPI);
  touch.setRotation(1);
#endif

#ifndef GLOWDECK_WAVESHARE_S3
  ledcSetup(AUDIO_CH, 2000, 8);
  ledcAttachPin(PIN_AUDIO, AUDIO_CH);
  ledcWriteTone(AUDIO_CH, 0);
#endif
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
#ifdef GLOWDECK_WAVESHARE_S3
  tft.present();
#endif
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
  tickWledCelebrations();

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
  // Validate on-ice PP from official penalty and PP-goal gamecenter data,
  // never from scoreboard strength alone. At most one filtered request every
  // 20 seconds, including games visible on the CYD even if not armed.
  if (connected && uint32_t(now - lastPenaltyLedgerAttempt) >= 20000UL &&
      rearEvent != RearEvent::GOAL) {
    bool restoringLights = false;
    for (uint8_t i = 0; i < MAX_WLED; ++i)
      if (restoring[i]) restoringLights = true;
    if (!restoringLights) {
      lastPenaltyLedgerAttempt = millis();
      fetchFollowedPowerPlays();
      observePowerPlayEvents();
    }
  }
  if (overlayController.tick(millis())) {
    // An overlay ended or a queued higher-priority event took over.
    // Redraw the exact underlying page without advancing the carousel.
    lastCarouselAdvance = millis();
    overlayLastFrame = 0;
    partialLiveUpdate = false;
    needsDraw = true;
  }
  if (!overlayController.active() && screen == Screen::LIVE &&
      cfg.cycleGames && carouselPageCount() > 1 &&
      uint32_t(now - lastCarouselAdvance) >= uint32_t(cfg.cycleSeconds) * 1000UL) {
    advanceCarousel(1);
  }
  if (needDetailLoad && screen == Screen::STATS) {
    needDetailLoad = false;
    fetchDetailStats(detailGameId);
  }
  // Standings are requested only while their slide is displayed, with shared
  // pregame cache and 3m live / 15m idle refresh. Keep goal-restoration
  // latency higher priority and never allocate a second full NHL scoreboard.
  static uint32_t lastSlideStandingsCheck = 0;
  if (!overlayController.active() && connected &&
      screen == Screen::LIVE && showingStandingsPage() &&
      uint32_t(millis() - lastSlideStandingsCheck) > 5000UL) {
    bool restoringLights = false;
    for (uint8_t i = 0; i < MAX_WLED; ++i)
      if (restoring[i]) restoringLights = true;
    if (!restoringLights && rearEvent != RearEvent::GOAL) {
      lastSlideStandingsCheck = millis();
      loadSeasonStandings();
    }
  }
  // Prefetch stats for the displayed carousel game; the background request
  // happens only once per game/TTL, never once per frame. Defer requests
  // while a WLED restoration is pending to avoid delaying light recovery.
  if (!overlayController.active() && screen == Screen::LIVE &&
      showingGamePage() && gameCount && connected &&
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
#ifdef GLOWDECK_WAVESHARE_S3
  bool displayChanged = false;
#endif
  if (needsDraw) {
    partialLiveUpdate = false;
    draw();
#ifdef GLOWDECK_WAVESHARE_S3
    displayChanged = true;
#endif
  } else if (overlayController.active()) {
    const uint32_t frameNow = millis();
    const uint32_t period = EventOverlay::isFlash(overlayController.current().kind)
                            ? 140UL : 240UL;
#ifdef GLOWDECK_WAVESHARE_S3
    if (goalGif.active()) {
      if (goalGif.due(frameNow)) {
        drawOverlayAnimation(frameNow);
        displayChanged = true;
      }
    } else
#endif
    if (uint32_t(frameNow - overlayLastFrame) >= period) {
      drawOverlayAnimation(frameNow);
#ifdef GLOWDECK_WAVESHARE_S3
      displayChanged = true;
#endif
    }
  } else if (partialLiveUpdate && screen == Screen::LIVE &&
             showingGamePage() && gameCount > 0) {
    partialLiveUpdate = false;
#ifdef GLOWDECK_WAVESHARE_S3
    if (tft.nativeLayout()) {
      const LiveCarousel::Page page=currentCarouselPage();
      drawS3Readout(games[page.kind==LiveCarousel::Kind::GAME
                         ? page.index:featuredIndex % gameCount]);
    } else {
      drawLiveReadout(games[featuredIndex % gameCount]);
    }
    displayChanged = true;
#else
    drawLiveReadout(games[featuredIndex % gameCount]);
#endif
  }
#ifdef GLOWDECK_WAVESHARE_S3
  if (displayChanged) tft.present();
#endif
  // Update one optional pregame conference record after the current frame
  // is painted. This keeps the first season/standings view responsive.
  if (!overlayController.active()) tickPregameConferenceRecords();
  if (rebootAt && int32_t(now - rebootAt) >= 0) ESP.restart();
  delay(5);
}
