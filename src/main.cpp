#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <Update.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WiFiClientSecure.h>
#ifdef GLOWDECK_WAVESHARE_S3
// Preserve the already hardware-tested USB-CDC Improv Wi-Fi implementation.
#include <ImprovWiFiLibrary.h>
#else
// Original CYD uses a USB-UART bridge; use the corrected RPC frame parser.
#include "GameDayImprovSerial.h"
#endif
#include <qrcode.h>
#ifdef GLOWDECK_WAVESHARE_S3
#include "GameDayS3Hardware.h"
#include "S3GoalAudio.h"
#include "S3CaseLedPolicy.h"
#include "S3CaseLedDriver.h"
#include "GoalGifPlayer.h"
#include "S3NativeLayout.h"
#include "S3ArenaBackground.h"
#include "S3BroadcastGameState.h"
#include "S3BroadcastDataPolicy.h"
#include "S3BroadcastFramePolicy.h"
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
#include "CarouselHoldPolicy.h"
#include "GameDayIcons.h"
#include "NhlFetchPolicy.h"
#include "CelebrationEngine.h"
#include "NhlShootoutPolicy.h"
#include "WledDiscoveryPolicy.h"
#include "WledZonePolicy.h"
#include "WledScenePolicy.h"
#include "WledDdpPolicy.h"
#include "WledRestoreFetchPolicy.h"
#include "HueBridge.h"
#include "TeamGradient.h"
#include "GameEventLog.h"
#include "PowerPlayLogPolicy.h"
#include "NhlPowerPlayEvidence.h"
#include "NhlPenaltyLedger.h"
#include "NhlPowerPlayTracker2.h"
#include "NhlPowerPlayPresentation.h"
#include "NhlLiveTicker.h"
#include "NhlNewsService.h"
#include "NhlNewsPhoto.h"
#include "NhlOnIce.h"
#include "EventOverlay.h"
#include "DisplayEventPolicy.h"
#include "GameFollowPolicy.h"
#include "MultiGameArming.h"
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
constexpr uint8_t MAX_GAMES = GameFollowPolicy::kMaxDailyGames, MAX_WLED = WledDiscoveryPolicy::kMaxDevices;
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
  uint32_t ppSession = 0; // Stable within game across feed gaps/intermission.
  NhlPowerPlayTracker2::Phase ppPhase=NhlPowerPlayTracker2::Phase::IDLE;
  uint8_t emptyNetSide = 0; // 0 none, 1 home goalie pulled, 2 away, 3 both
  bool emptyNetKnown = false;
  uint32_t emptyNetUpdatedAt = 0;
};
struct Settings {
  String ssid, wifiPassword, adminPassword;
  String wled[MAX_WLED];
  bool wledEnabled[MAX_WLED] = {};
  // Masked opt-in for every connected controller: goal / PP / start / final.
  uint8_t wledEvents[MAX_WLED] = {31,31,31,31,31,31,31,31};
  String wledName[MAX_WLED];
  bool wledZoneEnabled[MAX_WLED] = {}; // OFF for migrated 8A/legacy slots
  uint32_t wledZoneMap[MAX_WLED] = {}; // 2-bit per existing WLED segment ID
  WledScenePolicy::Scene wledScenes[MAX_WLED][WledScenePolicy::kSceneCount];
  bool wledDdpEnabled[MAX_WLED]={}; // explicit exclusive-controller opt-in
  uint8_t wledDdpFps[MAX_WLED]={}; // loaded 10–20 fps, default 15
  bool wledPP = true, wledGameStart = true, wledFinal = true;
  uint8_t wledGoalStyle = 0; // 0 strobe+chase, 1 strobe, 2 chase, 3 solid
  uint8_t favoriteSide = 0; // home 0 / away 1; for start and final outcomes
  uint8_t mode = 3; // 0=off, 1=home, 2=away, 3=both
  uint8_t brightness = 190;
  uint8_t seconds = 8;
  bool horn = false;
#ifdef GLOWDECK_WAVESHARE_S3
  uint8_t s3Volume = S3HornPolicy::kVolumePercent;
  uint8_t s3Sound = 0; // 0 = built-in, 1..3 = persistent WAV slots
  uint8_t s3PpSound = 0; // 0 = no PP audio, 1..3 = independent WAV slot
  uint8_t s3StartSound = 3; // Default game-start WAV slot; 0 disables
  uint8_t s3FinishSound = 4; // Default game-finish WAV slot; 0 disables
  bool caseLedsEnabled = false; // Explicit opt-in AFTER physical wiring
  bool caseGoal=true,casePp=true,caseStart=true,caseFinal=true;
  uint8_t casePixels=S3CaseLedPolicy::kDefaultPixels;
  uint8_t caseBrightness=S3CaseLedPolicy::kDefaultBrightness;
  uint8_t caseGoalSeconds=6;
#endif
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

HueBridge::Engine hue; // Independent Hue bridge, never a WLED slot.
uint32_t hueSeenRevision=0;
uint32_t hueTestUntil=0;
MultiGameArming::Queue huePriorityQueue;
Preferences prefs;
// USB Improv Serial: ESP Web Tools provisions home Wi-Fi without joining the setup AP.
// BLE is not enabled: the same USB serial protocol serves CYD and S3.
#ifdef GLOWDECK_WAVESHARE_S3
ImprovWiFi improvSerial(&Serial);
#else
GameDayImprovSerial improvSerial(Serial);
#endif
uint32_t wifiOfflineSince=0;
bool hotspotSetupShown=false;
#ifdef GLOWDECK_WAVESHARE_S3
GameDayS3Display tft;
GameDayS3Touch touch;
S3GoalAudio s3Audio;
S3SoundStorage soundStorage;
S3CaseLedDriver caseLedDriver;
S3CaseLedPolicy::Controller caseLedFx;
#else
TFT_eSPI tft;
SPIClass touchSPI(VSPI);
XPT2046_Touchscreen touch(PIN_TOUCH_CS, PIN_TOUCH_IRQ);
#endif
WebServer server(80);
// Captive portal replies run on the password-protected setup hotspot only.
// No new Wi-Fi credential store, background task or listening HTTP port.
DNSServer captiveDns;
bool captiveDnsReady=false;
Game games[MAX_GAMES];
struct GoalWatch {
  uint32_t id = 0;
  int home = 0, away = 0;
  bool baseline = false;
};
GoalWatch goalWatch[MAX_GAMES];
NhlShootoutPolicy::Watch shootoutWatches[MAX_GAMES];
uint32_t lastShootoutPoll=0;
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
NhlPowerPlayTracker2::Watch ppSessions[MAX_GAMES];
struct PowerPlaySnapshot {
  uint32_t id = 0, updatedAt = 0;
  uint8_t side = 0;
  bool known = false;
};
PowerPlaySnapshot ppCache[MAX_GAMES];
NhlPenaltyLedger::Book penaltyLedgers[MAX_GAMES];
uint32_t lastPenaltyLedgerAttempt = 0;
// Player-level goal/box details are cached ONLY for the visible game.
// The existing filtered gamecenter landing request supplies this data.
NhlLiveTicker::Feed liveTickerFeed, liveTickerWork;
String liveTickerText;
uint32_t liveTickerTextGameId=0, liveTickerRefreshAt=0, liveTickerScrollAt=0;
uint8_t liveTickerShownGoalCount=0;
uint16_t liveTickerScroll=0; // UTF-8 starting byte (not a glyph frame step)
uint8_t liveTickerPixelOffset=0; // fractional glyph scroll, in LCD pixels
uint16_t liveTickerTextWidthPx=0;
bool liveTickerDirty=false;
// Phase 9: RAM-only bounded editorial cache; never attached to celebrations.
NhlNews::Catalog newsCache;
uint32_t newsLastAttempt=0, newsTickerAt=0;
// Continuous league-headline ticker: independent of whichever card is visible.
String newsTickerText;
uint16_t newsTickerChar=0; // UTF-8 character start byte
uint8_t newsTickerPixel=0; // Sub-character pixel offset
NhlNewsPhoto::Cache newsPhotos;
uint32_t newsPhotoLastCheck=0;
uint8_t newsSlide=0, newsTeamCursor=0;
bool newsFetchTeamNext=false, newsRefreshRequested=false, newsTickerDirty=false;
bool newsTryEspnNext=false; // Fallback is scheduled separately to avoid two TLS requests in one loop.
String newsLastError="Awaiting first headline fetch";
enum class RearEvent : uint8_t { IDLE, GOAL, POWER_PLAY, DIAGNOSTIC, SHOOTOUT };
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
CarouselHoldPolicy::State carouselHold; // Only rotation stops, never live polling.
uint32_t overlayReturnHeldGameId=0; // Remember the displayed game across overlays.
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
MultiGameArming::Armed armedGames;
MultiGameArming::Queue wledPriorityQueue;
uint32_t wledActiveGameId=0;
uint32_t wledDispatchGameId=0; // Event-specific WLED home/away colors.
uint32_t wledDeviceGameOwner[MAX_WLED]={};
uint32_t wledPpStoppedIds[MultiGameArming::kMaxArmed]={};
uint32_t wledPpStoppedSessions[MultiGameArming::kMaxArmed]={};
uint32_t wledQueuedDrops=0;
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
uint16_t wledTouchedSegments[MAX_WLED] = {};
uint16_t wledCurrentSceneSegments[MAX_WLED] = {};
uint16_t wledRestoredSceneSegments[MAX_WLED] = {};
uint32_t lastWledSceneExpiryCheck=0;
bool wledRestoreMapped[MAX_WLED] = {};
struct WledDdpSession {
  bool active=false,manual=false;
  uint8_t sequence=0,fps=WledDdpPolicy::kDefaultFps;
  uint32_t started=0,lastFrame=0,lastGoodFrame=0,frames=0,packets=0,durationMs=0;
  CelebrationEngine::Event event=CelebrationEngine::Event::NONE;
  WledDdpPolicy::Layout layout;
  WledDdpPolicy::Style styles[WledDdpPolicy::kMaxRanges];
  String status="Disabled"; // visible reason when the JSON fallback wins
};
WledDdpSession wledDdp[MAX_WLED];
WiFiUDP wledDdpUdp;
uint8_t wledDdpPacket[WledDdpPolicy::kPacketBytes];
uint8_t wledEventSide=WledZonePolicy::kUnknownSide;
uint8_t queuedWledSide=WledZonePolicy::kUnknownSide;
CelebrationEngine::Event wledActiveEvent = CelebrationEngine::Event::NONE;
uint32_t wledEventStart = 0, wledEventColor = 0, wledEventDuration = 0;
uint8_t wledPhase = 0;
String wledDeviceResult[MAX_WLED];
// On-demand WLED discovery. Cached probes are deliberately bounded; no
// automatic background network sweep or controller activation.
struct WledScanEntry {
  String ip,name,version;
  uint16_t ledCount=0;
  uint8_t maxSegments=0;
  bool online=false,live=false,rgbw=false;
};
WledScanEntry wledScan[WledDiscoveryPolicy::kMaxCandidates];
String wledScanCandidates[WledDiscoveryPolicy::kMaxCandidates];
uint8_t wledScanTotal=0,wledScanNext=0,wledScanFound=0;
bool wledScanning=false,wledScanPendingMdns=false,wledScanFinished=false;
uint32_t wledScanStarted=0,wledLastScanStarted=0;
String wledScanError;
String lastArmedGameState;
uint32_t wledPpTestUntil = 0, wledSuppressPpUntil = 0;
uint32_t wledPpManuallyStoppedGameId = 0;
CelebrationEngine::Event queuedWledEvent = CelebrationEngine::Event::NONE;
uint32_t queuedWledColor = 0;
bool restoring[MAX_WLED] = {false, false, false, false};
uint32_t restoreAt[MAX_WLED] = {0, 0, 0, 0};
uint8_t restoreRetries[MAX_WLED] = {0, 0, 0, 0};
bool hornPlaying = false;
uint32_t hornLogSequence = 0; // Most recent accepted audio-start event, if any
uint32_t hornStart = 0;
uint16_t hornFrequency = 0;

enum class Screen : uint8_t { LIVE, GAMES, SETUP, CALIBRATE, STATS };
EventOverlay::Controller overlayController;
#ifdef GLOWDECK_WAVESHARE_S3
S3ArenaBackground s3Arena;
#if defined(GLOWDECK_S3_PHASE6_LIVE) && GLOWDECK_S3_PHASE6_LIVE
uint32_t s3BroadcastRenderedGameId=0;
uint8_t s3BroadcastRenderedStatus=0;
S3BroadcastFrame::Snapshot s3BroadcastLastFrame;
uint32_t s3PerfReportedAt=0, s3PerfFullFrames=0, s3PerfPartialFrames=0;
uint32_t s3PerfSkippedFrames=0, s3PerfTransferBytes=0, s3PerfMaxTransferMs=0;
#endif
GoalGifPlayer goalGif;
uint32_t gifOverlayStart = 0;
uint32_t gifOverlayFailedStart = 0;
Screen s3ReturnFromSetup = Screen::LIVE;
uint8_t s3TouchCheckMask = 0;
int16_t s3TouchCheckX = -1, s3TouchCheckY = -1;
File goalUpload;
bool goalUploadFailed = false, goalUploadFinished = false;
size_t goalUploadBytes = 0;
String goalUploadError;
File soundUpload;
bool soundUploadFailed=false, soundUploadFinished=false;
uint32_t soundUploadBytes=0;
uint8_t soundUploadSlot=0;
String soundUploadError, soundUploadTitle;
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
  String hitsAway, hitsHome, blocksAway, blocksHome;
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
  // Keep pinned game full-screen even if it leaves the live-first plan at FINAL.
  const uint8_t held=carouselHold.index(games,gameCount);
  if(held!=255){
    LiveCarousel::Page p;
    p.kind=LiveCarousel::Kind::GAME; p.index=held;
    return p;
  }
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
  uint8_t from=carouselSlide;
  if(carouselHold.paused){
    const uint8_t located=LiveCarousel::locateGame(
        carouselPlan,games,carouselHold.gameId);
    if(located!=255)from=located;
  }
  carouselSlide=LiveCarousel::advance(from,carouselPlan,direction);
  const LiveCarousel::Page page=carouselPlan.at(carouselSlide);
  carouselHold.navigatedTo(page.kind==LiveCarousel::Kind::GAME &&
      page.index<gameCount ? games[page.index].id : 0);
  if(page.kind==LiveCarousel::Kind::GAME)featuredIndex=page.index;
  lastCarouselAdvance=millis();
  partialLiveUpdate=false;
  needsDraw=true;
}
void toggleCarouselHold(){
  const LiveCarousel::Page page=currentCarouselPage();
  if(page.kind!=LiveCarousel::Kind::GAME || page.index>=gameCount)return;
  const uint32_t id=games[page.index].id;
  carouselHold.toggle(id);
  if(!carouselHold.paused){
    overlayReturnHeldGameId=0; // User deliberately resumed rotation.
    const uint8_t found=LiveCarousel::locateGame(carouselPlan,games,id);
    if(found!=255)carouselSlide=found;
  }
  lastCarouselAdvance=millis();
  partialLiveUpdate=false;
  needsDraw=true;
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
bool isArmedGame(uint32_t gameId) { return armedGames.contains(gameId); }
uint8_t armPriority(uint32_t gameId) {
  const int8_t rank=armedGames.rank(gameId);
  return rank<0?0:uint8_t(rank+1);
}
bool rearFollows(uint32_t gameId) {
  if (!gameId) return false;
  if (isArmedGame(gameId)) return true;
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
// Arm/disarm and priority rank are independent of whether the user is
// viewing that game. Persist only these five IDs, preserving legacy "game".
void scheduleWledRestoreAll();
void persistArmedGames(){
  const uint32_t primary=armedGames.primary();
  if(selectedGameId!=primary){
    // The next observed poll is a baseline for the new #1 game.
    haveBaseline=false;
    lastArmedGameState="";
    armedPpLogSeq=0;
  }
  selectedGameId=primary;
  prefs.putUInt("game",selectedGameId);
  for(uint8_t i=0;i<MultiGameArming::kMaxArmed;++i)
    prefs.putUInt(("armed"+String(i)).c_str(),armedGames.ids[i]);
  needsDraw=true;
}
bool editArmedGame(uint32_t id,bool enable){
  if(!id||!isScheduledGame(id))return false;
  if(enable){
    if(!armedGames.add(id))return false;
  }else if(armedGames.remove(id)){
    wledPriorityQueue.removeGame(id);
    if(wledActiveGameId==id){
      scheduleWledRestoreAll();
      wledPriorityQueue.removeGame(id);
    }
    for(uint8_t i=0;i<MultiGameArming::kMaxArmed;++i)
      if(wledPpStoppedIds[i]==id){
        wledPpStoppedIds[i]=0;wledPpStoppedSessions[i]=0;
      }
  }
  persistArmedGames();
  return true;
}
bool moveArmedGame(uint32_t id,int direction){
  if(!armedGames.move(id,direction))return false;
  persistArmedGames();
  return true;
}
// One follow policy serves CYD game carousel, detail view, game list and web.
// All 20 daily games may be followed; only the armed game controls WLED.
using FollowResult = GameFollowPolicy::Result;
FollowResult setGameFollow(uint32_t id, bool follow) {
  if(isArmedGame(id))return FollowResult::ARMED;
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
    if (isArmedGame(g.id) &&
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

void triggerRearShootout(uint32_t rgb,bool scored){
  if(!cfg.rearLedEnabled||!cfg.rearGoal)return;
  rearDiagnosticUntil=0;rearTestPpUntil=0;
  rearGoalColor=rgb;rearGoalAt=millis();
  rearGoalDuration=scored?4200UL:2300UL;
  rearEvent=RearEvent::SHOOTOUT;
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
  const uint8_t explicitLabel=NhlPowerPlayEvidence::confirmedSide(
      hasPP(home),hasPP(away));
  if(explicitLabel)return explicitLabel;
  // NHL sometimes omits PP text while still reporting 5v4/4v3. This is
  // ONLY a candidate: the independent official penalty ledger must agree.
  // Never interpret six attackers or a pulled goalie as a power play.
  return NhlPowerPlayEvidence::manpowerCandidate(
      away["strength"]|-1,home["strength"]|-1,
      away["goaliePulled"]|false,home["goaliePulled"]|false);
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
// All PP outputs now consume the same per-game tracker, keyed by NHL game ID.
// Fresh official penalties can sustain an already-verified PP when the NHL
// situation label temporarily disappears during a stoppage/TV timeout.
// A NEW PP still requires two independent corroborating sources.
NhlPowerPlayTracker2::Watch *powerPlaySession(uint32_t id,bool create=true) {
  if(!id)return nullptr;
  for(auto &session:ppSessions)if(session.gameId==id)return &session;
  if(!create)return nullptr;
  for(auto &session:ppSessions)if(!session.gameId){
    session.reset(id);return &session;
  }
  return nullptr;
}
void syncPowerPlayOutput(Game &g,uint32_t now) {
  NhlPowerPlayTracker2::Watch *session=powerPlaySession(g.id,false);
  const uint8_t previous=g.ppSide;
  const uint8_t resolved=session?
      session->outputSide(now,g.intermission):0;
  g.ppSide=resolved;
  g.ppPhase=session?session->phase:NhlPowerPlayTracker2::Phase::IDLE;
  g.ppSession=session?session->session:0;
  // Other outputs already check ppDataKnown/ppUpdatedAt; keep their existing
  // safety checks while all LEDs and display share one resolved session.
  g.ppDataKnown=resolved!=0 ||
      (session && session->phase==NhlPowerPlayTracker2::Phase::IDLE &&
       session->reason==NhlPowerPlayTracker2::Reason::VERIFIED);
  if(g.ppDataKnown)g.ppUpdatedAt=now;
  else g.ppUpdatedAt=0;
  if(previous!=resolved && screen==Screen::LIVE &&
     currentCarouselPage().kind==LiveCarousel::Kind::GAME &&
     currentCarouselPage().index<gameCount &&
     games[currentCarouselPage().index].id==g.id &&
     !overlayController.active())partialLiveUpdate=true;
}
void validatePowerPlay(Game &g) {
  const uint32_t now=millis();
  NhlPowerPlayTracker2::Watch *session=powerPlaySession(g.id);
  if(!session){g.ppSide=0;g.ppDataKnown=false;return;}
  NhlPowerPlayTracker2::Sample input;
  input.gameId=g.id;
  input.nowMs=now;
  input.live=g.state=="LIVE"||g.state=="CRIT";
  input.final=CelebrationEngine::isFinal(g.state.c_str());
  input.intermission=g.intermission;
  input.labelKnown=g.ppSituationKnown;
  input.labelSide=g.ppCandidate;
  NhlPenaltyLedger::Book *ledger=penaltyLedger(g.id);
  input.ledgerFresh=ledger && ledger->loaded &&
      uint32_t(now-ledger->checkedAt)<=75000UL;
  input.clockKnown=NhlPenaltyLedger::Book::fromRemaining(
      g.period,g.clock.c_str(),input.gameAt,
      g.period==4 && g.periodType=="OT" &&
      ((g.id/10000UL)%100UL)==2UL);
  if(input.ledgerFresh && input.clockKnown){
    const NhlPenaltyLedger::Book::Evidence evidence=
        ledger->evidenceAt(input.gameAt);
    input.ledgerSide=evidence.side;
    input.penaltyKey=evidence.key;
    input.penaltyAt=evidence.penaltyAt;
    input.penaltyEndAt=evidence.endAt;
    if(session->side && input.ledgerSide==0 &&
       session->anchorKey){
      input.verifiedPpGoalEnd=ledger->hasPpGoalSince(
          session->side,session->anchorPenaltyAt,input.gameAt);
    }
  }
  const NhlPowerPlayTracker2::Phase oldPhase=session->phase;
  const NhlPowerPlayTracker2::Reason oldReason=session->reason;
  const uint8_t oldSide=session->side;
  const NhlPowerPlayTracker2::Delta change=session->observe(input);
  syncPowerPlayOutput(g,now);
  if(change.stateChanged || oldReason!=session->reason ||
     oldSide!=session->side){
    Serial.printf("[PP2] id=%lu session=%lu state=%s reason=%s side=%u "
                  "label=%u ledger=%u fresh=%u clock=%u at=%u ends=%u "
                  "out=%u start=%u end=%u resume=%u\n",
        (unsigned long)g.id,(unsigned long)session->session,
        NhlPowerPlayTracker2::phaseName(session->phase),
        NhlPowerPlayTracker2::reasonName(session->reason),
        unsigned(session->side),unsigned(input.labelSide),
        unsigned(input.ledgerSide),unsigned(input.ledgerFresh),
        unsigned(input.clockKnown),unsigned(input.gameAt),
        unsigned(session->expectedEndAt),unsigned(g.ppSide),
        unsigned(change.started),unsigned(change.ended),
        unsigned(change.resumed));
  }
}
void tickPowerPlaySessions(uint32_t now) {
  static uint32_t lastTick=0;
  if(uint32_t(now-lastTick)<1000UL)return;
  lastTick=now;
  for(uint8_t i=0;i<gameCount;++i){
    Game &g=games[i];
    NhlPowerPlayTracker2::Watch *session=powerPlaySession(g.id,false);
    if(session && session->tick(now,g.intermission)){
      Serial.printf("[PP2] id=%lu session=%lu state=UNKNOWN "
                    "reason=trusted-evidence-timeout\n",
          (unsigned long)g.id,(unsigned long)session->session);
    }
    syncPowerPlayOutput(g,now);
  }
}

uint8_t activePowerPlaySide(uint32_t now, uint32_t &color) {
  // Armed game ranks take precedence; separately starred games remain
  // eligible for local LEDs when none of the armed games has a valid PP.
  for(uint8_t pass=0;pass<2;++pass){
    const uint8_t n=pass==0?MultiGameArming::kMaxArmed:gameCount;
    for(uint8_t i=0;i<n;++i){
      const uint32_t id=pass==0?armedGames.ids[i]:games[i].id;
      const Game *candidate=nullptr;
      for(uint8_t j=0;j<gameCount;++j)
        if(id&&games[j].id==id){candidate=&games[j];break;}
      if(!candidate)continue;
      const Game &g=*candidate;
      if(pass==1 && (!rearFollows(id)||isArmedGame(id)))continue;
      if(!CelebrationEngine::isLive(g.state.c_str())||g.intermission||
         !g.ppDataKnown||!g.ppSide ||
         uint32_t(now-g.ppUpdatedAt)>80000UL)continue;
      color=teamRgb(g.ppSide==1?g.home:g.away);
      return g.ppSide;
    }
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
    rearEvent=rearEvent==RearEvent::SHOOTOUT?
      RearEvent::SHOOTOUT:RearEvent::GOAL;
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

#ifdef GLOWDECK_WAVESHARE_S3
void tickCaseLeds() {
  // Hardware stays electrically idle until the user enables external
  // strips after wiring. The original CYD rear LED is separate.
  if(!cfg.caseLedsEnabled) {
    if(caseLedDriver.enabled())caseLedDriver.enable(false);
    caseLedFx.stop();
    return;
  }
  if(!caseLedDriver.enabled())caseLedDriver.enable(true);
  const uint32_t now=millis();
  // Pick an ACTUAL active PP from followed/armed games. A non-PP game
  // appearing first in the scoreboard cannot silence a different PP.
  const Game *candidate=nullptr;
  for(uint8_t pass=0;pass<2;++pass){
    for(uint8_t i=0;i<gameCount;++i){
      const Game &g=games[i];
      if(!rearFollows(g.id) ||
         (g.state!="LIVE"&&g.state!="CRIT") ||
         g.intermission || !g.ppDataKnown || !g.ppSide ||
         uint32_t(now-g.ppUpdatedAt)>80000UL)continue;
      if(pass==0 && !isArmedGame(g.id))continue;
      if(pass==1 && isArmedGame(g.id))continue;
      if(!candidate || (isArmedGame(g.id) &&
           armPriority(g.id)<armPriority(candidate->id)))candidate=&g;
    }
    if(candidate)break;
  }
  uint8_t pp=0;
  if(cfg.casePp && candidate && !candidate->intermission &&
     candidate->ppDataKnown &&
     uint32_t(now-candidate->ppUpdatedAt)<=80000UL)
    pp=candidate->ppSide;
  caseLedFx.setPowerPlay(S3CaseLedPolicy::Side::HOME,pp==1,
                         candidate?teamRgb(candidate->home):0);
  caseLedFx.setPowerPlay(S3CaseLedPolicy::Side::AWAY,pp==2,
                         candidate?teamRgb(candidate->away):0);
  // The two channels are streamed at <=20Hz. Last goal wins on its
  // own side only; any opposite-side goal or live PP remains intact.
  caseLedDriver.tick(now,caseLedFx,cfg.casePixels,cfg.caseBrightness);
}
#endif

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
void celebrate(uint32_t color,uint32_t eventSequence=0,
               uint8_t scorerSide=WledZonePolicy::kUnknownSide,uint32_t gameId=0);
bool startWledEvent(CelebrationEngine::Event event,uint32_t color,
                    uint32_t eventSequence=0,
                    uint8_t scorerSide=WledZonePolicy::kUnknownSide,
                    uint32_t gameId=0);
bool queueArmedWledEvent(CelebrationEngine::Event event,uint32_t color,
                    uint32_t seq,uint8_t side,uint32_t gameId);

// Display event overlays share the NHL observation that produced the
// WLED/rear RGB celebration. They never create a second score trigger.
bool overlayAllowed(EventOverlay::Kind kind) {
  if (!cfg.overlayEnabled || screen == Screen::CALIBRATE) return false;
  switch(kind) {
    case EventOverlay::Kind::GOAL: return cfg.overlayGoals;
    case EventOverlay::Kind::SHOOTOUT_START:
    case EventOverlay::Kind::SHOOTOUT_GOAL:
    case EventOverlay::Kind::SHOOTOUT_SAVE:
    case EventOverlay::Kind::SHOOTOUT_MISS:return true;
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
    // An event overlay temporarily owns the display, not its carousel lock.
    // Keep the original NHL game ID even through multiple queued events.
    if(carouselHold.paused && !overlayReturnHeldGameId)
      overlayReturnHeldGameId=carouselHold.gameId;
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

// A shootout does not increment normal goal statistics. Track transitions and
// official play-by-play separately, keyed by game ID instead of carousel index.
NhlShootoutPolicy::Watch &shootoutWatch(uint32_t id){
  for(auto &w:shootoutWatches)if(w.gameId==id)return w;
  for(auto &w:shootoutWatches)if(!w.gameId){w.reset(id);return w;}
  return shootoutWatches[0]; // Game count is bounded by MAX_GAMES.
}
void observeShootoutStarts(){
  for(uint8_t i=0;i<gameCount;++i){
    const Game &g=games[i];
    if(!g.id)continue;
    NhlShootoutPolicy::Watch &w=shootoutWatch(g.id);
    if(!w.observe(g.id,CelebrationEngine::isLive(g.state.c_str()),
                  g.periodType.c_str()))continue;
    const bool followed=rearFollows(g.id);
    const String &team=cfg.favoriteSide==0?g.home:g.away;
    GameEventLog::Entry &row=addGameEvent(
      g,GameEventLog::Type::SHOOTOUT_START,team,0);
    row.rear=!followed?GameEventLog::Rear::NOT_SELECTED:
      cfg.rearLedEnabled&&cfg.rearGoal?GameEventLog::Rear::ACTIVE:
      GameEventLog::Rear::NOT_ENABLED;
    if(isArmedGame(g.id))
      queueArmedWledEvent(CelebrationEngine::Event::SHOOTOUT_START,
        teamRgb(team),row.sequence,WledZonePolicy::kUnknownSide,g.id);
    if(followed){
      triggerRearShootout(teamRgb(team),false);
#ifdef GLOWDECK_WAVESHARE_S3
      if(cfg.caseLedsEnabled && cfg.caseStart){
        caseLedFx.trigger(S3CaseLedPolicy::Side::HOME,
          S3CaseLedPolicy::Event::SHOOTOUT,teamRgb(g.home),millis());
        caseLedFx.trigger(S3CaseLedPolicy::Side::AWAY,
          S3CaseLedPolicy::Event::SHOOTOUT,teamRgb(g.away),millis());
      }
#endif
    }
    if(followed||cfg.displayAllLiveEvents)
      queueGameOverlay(&g,EventOverlay::Kind::SHOOTOUT_START,
                       team,row.sequence,true);
    Serial.printf("[SHOOTOUT] game=%lu entered shootout\n",
                  (unsigned long)g.id);
  }
}
void signalShootoutAttempt(const Game &g,uint8_t shooter,
                           NhlShootoutPolicy::Attempt result,
                           uint32_t sortOrder){
  const bool goal=result==NhlShootoutPolicy::Attempt::GOAL;
  const bool save=result==NhlShootoutPolicy::Attempt::SAVE;
  // Give a save to the defending goalie, NOT the shooter.
  const uint8_t signalSide=save?(shooter==1?2:1):shooter;
  const String &team=signalSide==1?g.home:g.away;
  const bool followed=rearFollows(g.id);
  const uint32_t color=teamRgb(team);
  const CelebrationEngine::Event evt=goal?
    CelebrationEngine::Event::SHOOTOUT_GOAL:save?
    CelebrationEngine::Event::SHOOTOUT_SAVE:
    CelebrationEngine::Event::SHOOTOUT_MISS;
  const EventOverlay::Kind overlay=goal?
    EventOverlay::Kind::SHOOTOUT_GOAL:save?
    EventOverlay::Kind::SHOOTOUT_SAVE:
    EventOverlay::Kind::SHOOTOUT_MISS;
  const GameEventLog::Type logType=goal?
    GameEventLog::Type::SHOOTOUT_GOAL:save?
    GameEventLog::Type::SHOOTOUT_SAVE:
    GameEventLog::Type::SHOOTOUT_MISS;
  GameEventLog::Entry &row=addGameEvent(g,logType,team,0);
  row.rear=!followed?GameEventLog::Rear::NOT_SELECTED:
    cfg.rearLedEnabled&&cfg.rearGoal?GameEventLog::Rear::ACTIVE:
    GameEventLog::Rear::NOT_ENABLED;
  // No startHorn()/celebrate() for shootout shots, including SO goals.
  if(isArmedGame(g.id))
    queueArmedWledEvent(evt,color,row.sequence,signalSide==1?
      WledZonePolicy::kHomeSide:WledZonePolicy::kAwaySide,g.id);
  if(followed){
    triggerRearShootout(color,goal);
#ifdef GLOWDECK_WAVESHARE_S3
    if(cfg.caseLedsEnabled&&cfg.caseGoal)
      caseLedFx.trigger(signalSide==1?S3CaseLedPolicy::Side::HOME:
        S3CaseLedPolicy::Side::AWAY,S3CaseLedPolicy::Event::SHOOTOUT,
        color,millis(),goal?4200UL:2300UL);
#endif
  }
  if(followed||cfg.displayAllLiveEvents)
    queueGameOverlay(&g,overlay,team,row.sequence,true);
  Serial.printf("[SHOOTOUT] game=%lu eventSort=%lu shooter=%s "
                "signal=%s result=%s log=%lu\n",
    (unsigned long)g.id,(unsigned long)sortOrder,
    shooter==1?g.home.c_str():g.away.c_str(),team.c_str(),
    CelebrationEngine::label(evt),(unsigned long)row.sequence);
}
// Only fetch NHL PBP during a live SO. Filter the response to minimize heap
// use and dedupe by official sortOrder. Never invent a shot from score deltas.
void fetchShootoutPlays(){
  Game *target=nullptr;
  for(uint8_t i=0;i<gameCount;++i){
    Game &g=games[i];
    if(!g.id||g.periodType!="SO"||
       !CelebrationEngine::isLive(g.state.c_str()))continue;
    if(!target||(isArmedGame(g.id)&&!isArmedGame(target->id))||
       (rearFollows(g.id)&&!rearFollows(target->id)))target=&g;
  }
  if(!target)return;
  NhlShootoutPolicy::Watch &watch=shootoutWatch(target->id);
  WiFiClientSecure client;client.setInsecure();
  HTTPClient http;
  http.setConnectTimeout(2500);http.setTimeout(4500);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.useHTTP10(true);
  const String url="https://api-web.nhle.com/v1/gamecenter/"+
    String(target->id)+"/play-by-play";
  if(!http.begin(client,url))return;
  http.addHeader("Accept-Encoding","identity");
  if(http.GET()!=HTTP_CODE_OK){http.end();return;}
  DynamicJsonDocument filter(1024);
  filter["homeTeam"]["id"]=true;
  filter["awayTeam"]["id"]=true;
  JsonObject item=filter["plays"][0].to<JsonObject>();
  item["sortOrder"]=true;
  item["typeCode"]=true;
  item["periodDescriptor"]["periodType"]=true;
  item["details"]["eventOwnerTeamId"]=true;
  // Never allocate the unfiltered PBP feed.
  DynamicJsonDocument doc(65536);
  if(filter.overflowed()||!doc.capacity()){http.end();return;}
  Stream &body=http.getStream();body.setTimeout(4500);
  const DeserializationError err=deserializeJson(
    doc,body,DeserializationOption::Filter(filter));
  http.end();
  if(err||doc.overflowed()){
    Serial.printf("[SHOOTOUT] NHL PBP parse failed game=%lu %s\n",
      (unsigned long)target->id,err.c_str());return;
  }
  const uint32_t homeId=doc["homeTeam"]["id"]|0U;
  const uint32_t awayId=doc["awayTeam"]["id"]|0U;
  if(!homeId||!awayId||homeId==awayId)return;
  const JsonArrayConst plays=doc["plays"].as<JsonArrayConst>();
  if(plays.isNull())return;
  // First poll after booting into an in-progress shootout seeds the cursor.
  // An observed OT→SO transition may announce already-finished first shots.
  const bool dispatch=watch.playsKnown||watch.transitionSeen;
  uint32_t newest=watch.lastSort;
  for(JsonVariantConst shot:plays){
    const uint32_t sort=shot["sortOrder"]|0U;
    if(sort>newest)newest=sort;
    if(!sort||sort<=watch.lastSort)continue;
    const auto result=NhlShootoutPolicy::classify(
      shot["periodDescriptor"]["periodType"]|"",
      uint16_t(shot["typeCode"]|0));
    if(result==NhlShootoutPolicy::Attempt::NONE)continue;
    const uint32_t owner=shot["details"]["eventOwnerTeamId"]|0U;
    const uint8_t side=owner==homeId?1:owner==awayId?2:0;
    if(!side)continue;
    watch.record(side,result);
    if(dispatch)signalShootoutAttempt(*target,side,result,sort);
  }
  watch.lastSort=newest;
  watch.playsKnown=true;
  watch.transitionSeen=false;
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
    if (watch.baseline && live && g.periodType!="SO") {
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
        const bool armedForGoal = isArmedGame(g.id) &&
            (side == 0 ? (cfg.mode == 1 || cfg.mode == 3)
                       : (cfg.mode == 2 || cfg.mode == 3));
        if (armedForGoal) celebrate(teamRgb(scorer),entry.sequence,
                  side==0?WledZonePolicy::kHomeSide:WledZonePolicy::kAwaySide,
                  g.id);
        else entry.audio=GameEventLog::Audio::NOT_ARMED;
        // Start the internal LED flash AFTER all WLED network requests so
        // multi-device snapshots don't consume its 2–12s animation window.
        if (followed && cfg.rearLedEnabled && cfg.rearGoal) {
          triggerRearGoal(teamRgb(scorer));
          entry.rear = GameEventLog::Rear::ACTIVE;
        }
#ifdef GLOWDECK_WAVESHARE_S3
        // External case LEDs: followed/armed NHL GOALS only.
        // All-live DISPLAY alerts never control physical case strips.
        if(cfg.caseLedsEnabled && cfg.caseGoal && followed)
          caseLedFx.trigger(side==0?S3CaseLedPolicy::Side::HOME:
                                      S3CaseLedPolicy::Side::AWAY,
              S3CaseLedPolicy::Event::GOAL,teamRgb(scorer),millis(),
              uint32_t(cfg.caseGoalSeconds)*1000UL);
#endif
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

// The PP2 session tracker is the SINGLE event authority. Starting/ending
// does not depend on a separate side-only debouncer or LED polling interval.
// Each session emits one start and one end despite TV breaks, intermissions,
// fetch reordering and a WLED preview interrupt.
void scheduleWledRestoreAll();
void startPowerPlaySound(uint32_t eventSequence);
void observePowerPlayEvents() {
  for (uint8_t i=0;i<gameCount;++i) {
    const Game &g=games[i];
    NhlPowerPlayTracker2::Watch *session=powerPlaySession(g.id,false);
    if(!session)continue;
    const NhlPowerPlayTracker2::Delta change=session->takePending();
    const bool followed=rearFollows(g.id);
    if((!followed&&!cfg.displayAllLiveEvents) ||
       (g.state!="LIVE"&&g.state!="CRIT"))continue;
    if(change.ended){
      // change.ended is the team that HAD the power play. The opponent
      // served the penalty and is the one BACK AT FULL STRENGTH.
      // Use that club consistently for the official logo, team colors,
      // display overlay, Activity history and serial diagnostic.
      const uint8_t shortSide=
          NhlPowerPlayPresentation::shortHandedSide(change.ended);
      const String &team=shortSide==1?g.home:g.away;
      const String &ppTeam=change.ended==1?g.home:g.away;
      const uint32_t seq=addGameEvent(
          g,GameEventLog::Type::POWER_PLAY_END,team,0).sequence;
      queueGameOverlay(&g,EventOverlay::Kind::POWER_PLAY_END,team,seq,true);
      if(g.id==selectedGameId)armedPpLogSeq=0;
      Serial.printf("[PP2 EVENT] game=%lu session=%lu END "
                    "backAtFullStrength=%s previousPP=%s\n",
          (unsigned long)g.id,(unsigned long)session->session,
          team.c_str(),ppTeam.c_str());
    }
    if(change.started){
      const String &team=change.started==1?g.home:g.away;
      GameEventLog::Entry &entry=addGameEvent(
          g,GameEventLog::Type::POWER_PLAY_START,team,0);
      entry.rear=!followed?GameEventLog::Rear::NOT_SELECTED:
          (!cfg.rearLedEnabled||!cfg.rearPowerPlay)?
          GameEventLog::Rear::NOT_ENABLED:GameEventLog::Rear::SCHEDULED;
      if(g.id==selectedGameId)armedPpLogSeq=entry.sequence;
      queueGameOverlay(&g,EventOverlay::Kind::POWER_PLAY_START,
                       team,entry.sequence,true);
      if(followed)startPowerPlaySound(entry.sequence);
      else entry.audio=GameEventLog::Audio::NOT_ARMED;
      Serial.printf("[PP2 EVENT] game=%lu session=%lu START %s\n",
          (unsigned long)g.id,(unsigned long)session->session,team.c_str());
    }
    if(change.resumed) {
      // Resume is not a second NHL start: no horn, overlay, or log duplication.
      Serial.printf("[PP2 EVENT] game=%lu session=%lu RESUMED\n",
          (unsigned long)g.id,(unsigned long)session->session);
    }
  }
}

Game *gameById(uint32_t id){
  if(!id)return nullptr;
  for(uint8_t i=0;i<gameCount;++i)if(games[i].id==id)return &games[i];
  return nullptr;
}
Game *selectedGame(){return gameById(selectedGameId);}
Game *wledSceneGame(){return gameById(wledDispatchGameId);}
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
    carouselHold.reset();
    overlayReturnHeldGameId=0;
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
    for(auto &entry:shootoutWatches)entry=NhlShootoutPolicy::Watch{};
    lastShootoutPoll=0;
    for (auto &entry : stateWatch) entry = StateWatch();
    hasBackgroundOverlay = false;
    for (auto &entry : ppSessions) entry.reset();
    for(auto &id:wledPpStoppedIds)id=0;
    for(auto &key:wledPpStoppedSessions)key=0;
    armedPpLogSeq = 0;
    for (auto &entry : ppCache) entry = PowerPlaySnapshot();
    for (auto &entry : penaltyLedgers) entry.reset();
    lastPenaltyLedgerAttempt = 0;
    liveTickerFeed.reset();
    liveTickerWork.reset();
    liveTickerText = "";
    liveTickerTextGameId = 0;
    liveTickerScroll = 0;
    liveTickerPixelOffset=0;
    liveTickerTextWidthPx=0;
    liveTickerDirty = false;
    liveTickerRefreshAt = 0;
    liveTickerShownGoalCount = 0;
    for (uint8_t i = 0; i < MAX_FOLLOWED_GAMES; ++i) cfg.followIds[i] = 0;
    armedGames.clear();
    wledPriorityQueue.clear();
    for(auto &id:wledPpStoppedIds)id=0;
    wledActiveGameId=0;
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
  if(rearEvent==RearEvent::GOAL || rearEvent==RearEvent::SHOOTOUT)return;
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
        // Read all relevant team totals from the same right-rail JSON,
        // independently of the five visible STATS-page rows. A SHOTS row
        // can consume one of those five, hiding BLOCKS from the detail page.
        // Ticker-only totals must not be silently dropped for that reason.
        for (uint8_t idx=0;idx<gameCount;++idx) {
          if(games[idx].id!=id)continue;
          CarouselStats &cached=statsFor(idx);
          cached.hitsAway="";cached.hitsHome="";
          cached.blocksAway="";cached.blocksHome="";
          for (JsonObject row : values) {
            const String category=row["category"].as<String>();
            if(category.equalsIgnoreCase("hits")) {
              cached.hitsAway=row["awayValue"].as<String>();
              cached.hitsHome=row["homeValue"].as<String>();
            } else if(category.equalsIgnoreCase("blockedShotAttempts")) {
              cached.blocksAway=row["awayValue"].as<String>();
              cached.blocksHome=row["homeValue"].as<String>();
            }
          }
          break;
        }
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
  // Migrate legacy all-event masks once; otherwise users cannot opt out of SO.
  const bool migrateSoMasks=!prefs.getBool("soMasksV1",false);
  cfg.ssid = prefs.getString("ssid", "");
  cfg.wifiPassword = prefs.getString("wpass", "");
  cfg.adminPassword = prefs.getString("admin", apPassword);
  hue.config.ip=prefs.getString("hueIp","");
  hue.config.key=prefs.getString("hueKey","");
  hue.config.enabled=prefs.getBool("hueOn",false);
  hue.config.events=prefs.getUChar("hueMask",CelebrationEngine::ALL_EVENTS)&CelebrationEngine::ALL_EVENTS;
  if(migrateSoMasks && hue.config.events==15){
    hue.config.events=CelebrationEngine::ALL_EVENTS;
    prefs.putUChar("hueMask",hue.config.events);
  }
  hue.config.brightness=constrain((int)prefs.getUChar("hueBri",75),5,100);
  for(uint8_t i=0;i<HuePolicy::kMaxLights;++i){
    String id=prefs.getString(("hueL"+String(i)).c_str(),"");
    hue.config.lights[i]=HuePolicy::validResourceId(id.c_str())?id:"";
  }
  for (uint8_t i = 0; i < MAX_WLED; ++i) {
    cfg.wled[i] = prefs.getString(("ip" + String(i)).c_str(), "");
    cfg.wledName[i] = prefs.getString(("wname" + String(i)).c_str(), "");
    cfg.wledZoneEnabled[i] = prefs.getBool(("zoneOn"+String(i)).c_str(),false);
    cfg.wledZoneMap[i] = prefs.getUInt(("zoneMap"+String(i)).c_str(),0);
    cfg.wledDdpEnabled[i]=prefs.getBool(("ddpOn"+String(i)).c_str(),false);
    const int ddpRate=prefs.getUChar(("ddpFps"+String(i)).c_str(),
                                         WledDdpPolicy::kDefaultFps);
    cfg.wledDdpFps[i]=WledDdpPolicy::validFps(ddpRate)?
      uint8_t(ddpRate):WledDdpPolicy::kDefaultFps;
    // 8C scene templates live in one CRC-validated NVS blob per slot.
    // Missing/corrupt blobs keep all 15 entries in INHERIT mode.
    const String key="scene"+String(i);
    if(prefs.getBytesLength(key.c_str())==WledScenePolicy::kBlobBytes){
      uint8_t raw[WledScenePolicy::kBlobBytes]={};
      if(prefs.getBytes(key.c_str(),raw,sizeof(raw))==sizeof(raw) &&
         !WledScenePolicy::decode(raw,sizeof(raw),cfg.wledScenes[i]))
        Serial.printf("[WLED Scene] invalid scene blob slot=%u; defaults retained\n",
                     unsigned(i));
    }
    cfg.wledEnabled[i] = prefs.getBool(("enable" + String(i)).c_str(), false);
    cfg.wledEvents[i] = prefs.getUChar(("event" + String(i)).c_str(),
                                         CelebrationEngine::ALL_EVENTS) &
                        CelebrationEngine::ALL_EVENTS;
    if(migrateSoMasks && cfg.wledEvents[i]==15){
      cfg.wledEvents[i]=CelebrationEngine::ALL_EVENTS;
      prefs.putUChar(("event"+String(i)).c_str(),cfg.wledEvents[i]);
    }
  }
  if(migrateSoMasks)prefs.putBool("soMasksV1",true);
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
  cfg.s3Volume=constrain((int)prefs.getUChar("s3Vol",
                              S3HornPolicy::kVolumePercent),
                          (int)S3HornPolicy::kMinVolumePercent,
                          (int)S3HornPolicy::kMaxVolumePercent);
  cfg.s3Sound=constrain((int)prefs.getUChar("s3Sound",0),
                        0,(int)S3WavPolicy::kSlots);
  cfg.s3PpSound=constrain((int)prefs.getUChar("s3PpSound",0),
                          0,(int)S3WavPolicy::kSlots);
  cfg.s3StartSound=constrain((int)prefs.getUChar("s3StartSound",3),
                             0,(int)S3WavPolicy::kSlots);
  cfg.s3FinishSound=constrain((int)prefs.getUChar("s3FinishSound",4),
                              0,(int)S3WavPolicy::kSlots);
  cfg.caseLedsEnabled=prefs.getBool("caseOn",false);
  cfg.caseGoal=prefs.getBool("caseGoal",true);
  cfg.casePp=prefs.getBool("casePP",true);
  cfg.caseStart=prefs.getBool("caseStart",true);
  cfg.caseFinal=prefs.getBool("caseFinal",true);
  cfg.casePixels=constrain((int)prefs.getUChar("casePixels",
      S3CaseLedPolicy::kDefaultPixels),1,(int)S3CaseLedPolicy::kMaxPixels);
  cfg.caseBrightness=constrain((int)prefs.getUChar("caseBri",
      S3CaseLedPolicy::kDefaultBrightness),1,(int)S3CaseLedPolicy::kMaxBrightness);
  cfg.caseGoalSeconds=constrain((int)prefs.getUChar("caseSec",6),2,12);
#endif
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
  // Migrate legacy single-game arming without resetting other settings.
  if(selectedGameId)armedGames.add(selectedGameId);
  for(uint8_t i=0;i<MultiGameArming::kMaxArmed;++i){
    const uint32_t id=prefs.getUInt(("armed"+String(i)).c_str(),0);
    if(id)armedGames.add(id);
  }
  selectedGameId=armedGames.primary();
  rebuildCarouselPlan(); // Standings-only fallback until the first NHL fetch.
}

void saveSettings() {
  prefs.putString("ssid", cfg.ssid);
  prefs.putString("wpass", cfg.wifiPassword);
  prefs.putString("admin", cfg.adminPassword);
  prefs.putString("hueIp",hue.config.ip);
  prefs.putString("hueKey",hue.config.key);
  prefs.putBool("hueOn",hue.config.enabled);
  prefs.putUChar("hueMask",hue.config.events);
  prefs.putUChar("hueBri",hue.config.brightness);
  for(uint8_t i=0;i<HuePolicy::kMaxLights;++i)
    prefs.putString(("hueL"+String(i)).c_str(),hue.config.lights[i]);
  for (uint8_t i = 0; i < MAX_WLED; ++i) {
    prefs.putString(("ip" + String(i)).c_str(), cfg.wled[i]);
    prefs.putString(("wname" + String(i)).c_str(), cfg.wledName[i]);
    // Mapping keys are written separately by the 8B segment editor.
    // The original Save settings must never replace those assignments.
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
  for(uint8_t i=0;i<MultiGameArming::kMaxArmed;++i)
    prefs.putUInt(("armed"+String(i)).c_str(),armedGames.ids[i]);
  prefs.putUInt("game",armedGames.primary());
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

// iOS/Android/Windows request a known HTTP page to detect captive Wi-Fi.
// Only AP-interface visitors get a redirect, NEVER LAN/admin clients.
// 302 plus a nonempty response body prompts the OS's captive sign-in window.
// The OS decides whether/when to open it: QR itself cannot launch a browser.
void sendCaptiveSetupRedirect() {
  if (!requestFromSetupAp()) {
    if (!authorized()) return;
    server.send(404, "text/plain", "Not found");
    return;
  }
  // Route straight to the *existing* Wi-Fi settings form on both boards.
  // The query parameter is a fallback if iOS drops the URL fragment.
  server.sendHeader("Location", "http://192.168.4.1/?setup=wifi#settings");
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
  server.send(302, "text/plain",
              "GlowDeck GameDay: configure Wi-Fi at http://192.168.4.1/?setup=wifi");
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
// Read-only status for S3 Phase 6C presentation; its renderer must not
// perform Wi-Fi calls, allocations or other network state transitions.
bool setupWifiOnline() { return WiFi.status() == WL_CONNECTED; }
void beginStation() {
  if (cfg.ssid.isEmpty()) return;
  WiFi.begin(cfg.ssid.c_str(), cfg.wifiPassword.c_str());
  reconnectAt = millis();
}

// Encode the WPA2 hotspot itself (not a URL) so iOS/Android camera scanners
// offer to join the network. QR data is deterministic and only displayed on
// physical screens the user can see. Four-module white quiet zone is required.
void drawHotspotJoinQr(int16_t left,int16_t top,uint8_t scale){
  const String payload="WIFI:T:WPA;S:"+apSsid+";P:"+apPassword+";;";
  // Version 3 / low ECC has enough byte-mode capacity for this fixed payload.
  // Keep a generous fixed scratch allocation on the stack; no dynamic sprite.
  uint8_t modules[256]={};
  QRCode qr;
  if(qrcode_initText(&qr,modules,3,ECC_LOW,payload.c_str())<0){
    Serial.println("[Setup QR] Unable to encode hotspot");
    return;
  }
  const int16_t side=(qr.size+8)*scale;
  tft.fillRect(left,top,side,side,WHITE);
  for(uint8_t y=0;y<qr.size;++y)
    for(uint8_t x=0;x<qr.size;++x)
      if(qrcode_getModule(&qr,x,y))
        tft.fillRect(left+(x+4)*scale,top+(y+4)*scale,scale,scale,0x0000);
}

// Provision only through a physically attached USB serial port. Never overwrite
// saved Wi-Fi/password until the new network is actually connected. A failed
// credential attempt restores the previous station connection when possible.
bool connectImprovWifi(const char *ssid,const char *password){
  if(!ssid || !password || !*ssid || strlen(ssid)>32 || strlen(password)>63)
    return false;
  if(wledActiveEvent!=CelebrationEngine::Event::NONE)
    return false; // No network handoff during an active light celebration.
  for(uint8_t i=0;i<MAX_WLED;++i)
    if(restoring[i] || wledDdp[i].active) return false;
  const String oldSsid=cfg.ssid,oldPassword=cfg.wifiPassword;
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid,password);
  for(uint8_t i=0;i<48;++i){ // bounded 12 seconds for DHCP + association
    if(WiFi.status()==WL_CONNECTED && WiFi.SSID()==String(ssid)){
      // The URL placeholder must be reset for each successful reconnect.
#ifdef GLOWDECK_WAVESHARE_S3
      improvSerial.setDeviceInfo(ImprovTypes::CF_ESP32_S3,
        "GlowDeck GameDay","setup-1","GlowDeck GameDay","http://{LOCAL_IPV4}/");
#else
      improvSerial.setDeviceInfo(GameDayImprovSerial::CHIP_ESP32,
        "GlowDeck GameDay","setup-1","GlowDeck GameDay","http://{LOCAL_IPV4}/");
#endif
      return true;
    }
    delay(250);
  }
  if(oldSsid.length())WiFi.begin(oldSsid.c_str(),oldPassword.c_str());
  else WiFi.disconnect(false);
  return false;
}
void savedImprovWifi(const char *ssid,const char *password){
  if(!ssid || !password)return;
  cfg.ssid=ssid;
  cfg.wifiPassword=password;
  // Only write the two existing wifi keys: don't overwrite Hue, WLED, audio,
  // arming or admin configuration in the rest of Preferences.
  prefs.putString("ssid",cfg.ssid);
  prefs.putString("wpass",cfg.wifiPassword);
  Serial.printf("[Improv] Home Wi-Fi provisioned: %s (%s)\n",
                cfg.ssid.c_str(),WiFi.localIP().toString().c_str());
  // Request the normal connected transition and refresh the setup display.
  wifiWasConnected=false;
  mdnsAttempted=false;
  // Normal first-run behavior: leave QR setup when USB provisioning succeeds.
  if(screen==Screen::SETUP)screen=Screen::LIVE;
  hotspotSetupShown=false;
  needsDraw=true;
}
void setupImprovSerial(){
#ifdef GLOWDECK_WAVESHARE_S3
  const auto family=ImprovTypes::CF_ESP32_S3;
#else
  const auto family=GameDayImprovSerial::CHIP_ESP32;
#endif
  improvSerial.setDeviceInfo(family,"GlowDeck GameDay","setup-1",
                            "GlowDeck GameDay","http://{LOCAL_IPV4}/");
  improvSerial.setCustomConnectWiFi(connectImprovWifi);
  improvSerial.onImprovConnected(savedImprovWifi);
#ifdef GLOWDECK_WAVESHARE_S3
  improvSerial.onImprovError([](ImprovTypes::Error err){
#else
  improvSerial.onImprovError([](uint8_t err){
#endif
    Serial.printf("[Improv] Provisioning error: %u\n",unsigned(err));
  });
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

bool validWledLocalIp(const IPAddress &ip){
  const IPAddress here=WiFi.localIP(),mask=WiFi.subnetMask();
  return WiFi.status()==WL_CONNECTED &&
    WledDiscoveryPolicy::sameSubnet(uint32_t(ip),uint32_t(here),
                                    uint32_t(mask)) &&
    ip[0]!=0 && ip[0]!=127 && ip[0]!=224 && ip[0]!=255;
}
bool probeWledInfo(const String &ip,WledScanEntry &out,
                   uint16_t timeoutMs=WledDiscoveryPolicy::kManualProbeTimeoutMs){
  IPAddress parsed;
  if(!parsed.fromString(ip) || !validWledLocalIp(parsed))return false;
  WiFiClient client;
  HTTPClient http;
  if(!http.begin(client,"http://"+ip+"/json/info"))return false;
  http.setConnectTimeout(timeoutMs);
  http.setTimeout(timeoutMs);
  const int code=http.GET();
  if(code!=HTTP_CODE_OK || http.getSize()>4000){
    http.end();return false;
  }
  const String body=http.getString();
  http.end();
  if(body.isEmpty()||body.length()>4000)return false;
  DynamicJsonDocument parsedJson(3072);
  if(deserializeJson(parsedJson,body))return false;
  const String brand=parsedJson["brand"]|String("");
  const String version=parsedJson["ver"]|String("");
  const uint32_t leds=parsedJson["leds"]["count"]|0UL;
  if(!WledDiscoveryPolicy::validInfo(brand.c_str(),version.c_str(),leds))
    return false;
  out.ip=ip;
  out.online=true;
  out.version=version.substring(0,24);
  out.name=String(parsedJson["name"]|String("WLED"));
  out.name.trim();
  if(out.name.isEmpty())out.name="WLED";
  if(out.name.length()>32)out.name=out.name.substring(0,32);
  out.ledCount=uint16_t(leds);
  out.live=parsedJson["live"]|false;
  out.rgbw=parsedJson["leds"]["rgbw"]|false;
  out.maxSegments=uint8_t(constrain((int)(parsedJson["leds"]["maxseg"]|1),1,32));
  return true;
}
void addWledScanCandidate(const String &ip){
  if(wledScanTotal>=WledDiscoveryPolicy::kMaxCandidates)return;
  IPAddress addr;
  if(!addr.fromString(ip)||!validWledLocalIp(addr))return;
  for(uint8_t i=0;i<wledScanTotal;++i)
    if(wledScanCandidates[i]==ip)return;
  wledScanCandidates[wledScanTotal++]=ip;
}
void tickWledDiscovery(){
  if(!wledScanning || WiFi.status()!=WL_CONNECTED)return;
  // Avoid colliding with live WLED effects, restoration or an NHL poll.
  if(wledActiveEvent!=CelebrationEngine::Event::NONE)return;
  for(uint8_t i=0;i<MAX_WLED;++i)
    if(restoring[i])return;
  if(wledScanPendingMdns){
    wledScanPendingMdns=false;
    // Existing controller IPs remain visible even when mDNS is unavailable.
    for(uint8_t i=0;i<MAX_WLED;++i)
      if(!cfg.wled[i].isEmpty())addWledScanCandidate(cfg.wled[i]);
    if(mdnsStarted){
      // WLED advertises _wled._tcp. ESPmDNS queryService performs one
      // bounded on-demand DNS-SD round; never do this on the main ticker.
      const int found=MDNS.queryService("wled","tcp");
      for(int i=0;i<found &&
          wledScanTotal<WledDiscoveryPolicy::kMaxCandidates;++i)
        addWledScanCandidate(MDNS.IP(i).toString());
    }else wledScanError="mDNS not connected; checking configured IPs";
  }
  if(wledScanNext<wledScanTotal){
    const uint8_t n=wledScanNext++;
    WledScanEntry info;
    info.ip=wledScanCandidates[n];
    if(!probeWledInfo(info.ip,info,WledDiscoveryPolicy::kScanProbeTimeoutMs)){
      info.name="Offline or not WLED";
      info.online=false;
    }else ++wledScanFound;
    wledScan[n]=info;
  }
  if(wledScanNext>=wledScanTotal){
    wledScanning=false;
    wledScanFinished=true;
    Serial.printf("[WLED] discovery complete: %u verified / %u candidates\n",
                  unsigned(wledScanFound),unsigned(wledScanTotal));
  }
}

// A successful WLED acknowledgement and a goal GIF never prove that sound
// was requested or that the DAC accepted it. Log this independent outcome
// on the *same* observed goal event, without creating a new NHL trigger.
void startHorn(uint32_t eventSequence=0) {
  GameEventLog::Entry *entry=gameEventHistory.find(eventSequence);
  if(entry) {
    entry->audio=GameEventLog::Audio::HORN_DISABLED;
    entry->audioSource=0;
    entry->audioFallback=false;
  }
  if (!cfg.horn) {
    Serial.println("[Goal Horn] skipped: live horn disabled");
    return;
  }
#ifdef GLOWDECK_WAVESHARE_S3
  const uint8_t source=s3Audio.selectedSource();
  if(entry)entry->audioSource=source;
  if(!s3Audio.ready()){
    if(entry)entry->audio=GameEventLog::Audio::UNAVAILABLE;
    Serial.printf("[Goal Horn] unavailable: %s\n",s3Audio.error());
    return;
  }
  // Phase 7A/7B: real goals use the persisted WAV choice. The preview is
  // independent and is deliberately allowed even when cfg.horn is OFF.
  if(!s3Audio.play()){
    const bool busy=s3Audio.busyOrCoolingDown();
    if(entry)entry->audio=busy?GameEventLog::Audio::BUSY:
                                    GameEventLog::Audio::UNAVAILABLE;
    Serial.printf("[Goal Horn] play refused: %s (selected slot=%u)\n",
                  busy?"busy, cooldown or uploading":s3Audio.error(),
                  unsigned(source));
    return;
  }
  if(entry){
    entry->audio=GameEventLog::Audio::STARTED;
    entry->audioFallback=source!=0 &&
                          strcmp(s3Audio.clipName(),"custom-wav")!=0;
    hornLogSequence=eventSequence;
  }
  Serial.printf("[Goal Horn] started: %s selected=%u event=%lu\n",
                s3Audio.clipName(),unsigned(source),
                (unsigned long)eventSequence);
#else
  hornPlaying = true;
  hornStart = millis();
  hornFrequency = 0;
  if(entry){
    entry->audio=GameEventLog::Audio::STARTED;
    hornLogSequence=eventSequence;
  }
#endif
}
// One-shot rink announcement at a verified power-play START only. Separate
// NVS sound choice from the goal horn; source 0 disables PP sound. The
// existing cooldown means overlapping audio is reported, not mixed.
void startGameStartSound(uint32_t eventSequence) {
  GameEventLog::Entry *entry=gameEventHistory.find(eventSequence);
  if(entry){entry->audio=GameEventLog::Audio::HORN_DISABLED;
            entry->audioFallback=false;}
#ifdef GLOWDECK_WAVESHARE_S3
  if(entry)entry->audioSource=cfg.s3StartSound;
  const uint8_t source=cfg.s3StartSound;
  if(!source)return;
  if(!s3Audio.ready() || !soundStorage.available(source)){
    if(entry)entry->audio=GameEventLog::Audio::UNAVAILABLE;
    Serial.printf("[Game Start Audio] slot %u unavailable\n",unsigned(source));
    return;
  }
  if(!s3Audio.playSource(source)){
    if(entry)entry->audio=s3Audio.busyOrCoolingDown()?
      GameEventLog::Audio::BUSY:GameEventLog::Audio::UNAVAILABLE;
    Serial.printf("[Game Start Audio] slot %u playback refused\n",unsigned(source));
    return;
  }
  if(entry){entry->audio=GameEventLog::Audio::STARTED;
            hornLogSequence=eventSequence;}
  Serial.printf("[Game Start Audio] slot %u started event=%lu\n",
                unsigned(source),(unsigned long)eventSequence);
#else
  (void)eventSequence;
#endif
}
void startGameFinishSound(uint32_t eventSequence) {
  GameEventLog::Entry *entry=gameEventHistory.find(eventSequence);
  if(entry){entry->audio=GameEventLog::Audio::HORN_DISABLED;
            entry->audioFallback=false;entry->audioSource=0;}
#ifdef GLOWDECK_WAVESHARE_S3
  const uint8_t source=cfg.s3FinishSound;
  if(entry)entry->audioSource=source;
  if(!source)return;
  if(!s3Audio.ready() || !soundStorage.available(source)){
    if(entry)entry->audio=GameEventLog::Audio::UNAVAILABLE;
    Serial.printf("[Game Finish Audio] slot %u unavailable\n",unsigned(source));
    return;
  }
  if(!s3Audio.playSource(source)){
    if(entry)entry->audio=s3Audio.busyOrCoolingDown()?
      GameEventLog::Audio::BUSY:GameEventLog::Audio::UNAVAILABLE;
    Serial.printf("[Game Finish Audio] slot %u playback refused\n",unsigned(source));
    return;
  }
  if(entry){entry->audio=GameEventLog::Audio::STARTED;
            hornLogSequence=eventSequence;}
  Serial.printf("[Game Finish Audio] slot %u started event=%lu\n",
                unsigned(source),(unsigned long)eventSequence);
#else
  (void)eventSequence;
#endif
}
void startPowerPlaySound(uint32_t eventSequence) {
  GameEventLog::Entry *entry=gameEventHistory.find(eventSequence);
  if(entry){
    entry->audio=GameEventLog::Audio::HORN_DISABLED;
    entry->audioFallback=false;
    entry->audioSource=0;
  }
#ifdef GLOWDECK_WAVESHARE_S3
  const uint8_t source=cfg.s3PpSound;
  if(entry)entry->audioSource=source;
  if(!source){
    Serial.println("[PP Audio] skipped: no power-play sound selected");
    return;
  }
  if(!s3Audio.ready() || !soundStorage.available(source)){
    if(entry)entry->audio=GameEventLog::Audio::UNAVAILABLE;
    Serial.printf("[PP Audio] unavailable: source=%u codec=%s\n",
                  unsigned(source),s3Audio.error());
    return;
  }
  if(!s3Audio.playSource(source)){
    const bool busy=s3Audio.busyOrCoolingDown();
    if(entry)entry->audio=busy?GameEventLog::Audio::BUSY:
                                    GameEventLog::Audio::UNAVAILABLE;
    Serial.printf("[PP Audio] refused: source=%u %s\n",
                  unsigned(source),busy?"busy/cooldown":"speaker failure");
    return;
  }
  if(entry){
    entry->audio=GameEventLog::Audio::STARTED;
    hornLogSequence=eventSequence; // Existing completion monitor tracks both clips
  }
  Serial.printf("[PP Audio] started: source=%u event=%lu\n",
                unsigned(source),(unsigned long)eventSequence);
#else
  // CYD has a simple buzzer, not a PCM speaker; do not synthesize PP audio.
  (void)eventSequence;
#endif
}
void tickHorn() {
#ifdef GLOWDECK_WAVESHARE_S3
  s3Audio.tick();
  if(hornLogSequence && !s3Audio.playing()){
    GameEventLog::Entry *entry=gameEventHistory.find(hornLogSequence);
    if(entry && entry->audio==GameEventLog::Audio::STARTED){
      entry->audio=s3Audio.lastStreamOk()?
        GameEventLog::Audio::COMPLETED:GameEventLog::Audio::STREAM_FAILED;
      entry->audioFallback=entry->audioFallback || s3Audio.usedFallback();
      entry->audioFrames=s3Audio.writtenFrames();
      entry->audioExpectedFrames=s3Audio.expectedFrames();
      entry->audioDurationMs=s3Audio.lastDurationMs();
      entry->audioPeak=s3Audio.peakSample();
      entry->audioRetries=s3Audio.writeRetries();
      entry->audioReadOffset=s3Audio.readFailureOffset();
      entry->audioI2sCode=s3Audio.lastI2sCode();
      entry->audioVolume=s3Audio.volume();
      entry->audioAmpVerified=s3Audio.ampVerifiedAtStart();
      entry->audioFailure=static_cast<GameEventLog::AudioFailure>(
          static_cast<uint8_t>(s3Audio.lastFailure()));
    }
    hornLogSequence=0;
  }
  return;
#else
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
  if (elapsed >= 900) {
    hornPlaying = false;
    GameEventLog::Entry *entry=gameEventHistory.find(hornLogSequence);
    if(entry && entry->audio==GameEventLog::Audio::STARTED)
      entry->audio=GameEventLog::Audio::COMPLETED;
    hornLogSequence=0;
  }
#endif
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

// 8C scenes are only evaluated AFTER the existing NHL arm/role policy.
const WledScenePolicy::Scene &wledScene(uint8_t slot,
    CelebrationEngine::Event event,WledZonePolicy::Role role){
  static const WledScenePolicy::Scene fallback;
  const int index=WledScenePolicy::index(uint8_t(event),uint8_t(role));
  return slot<MAX_WLED && index>=0?
    cfg.wledScenes[slot][index]:fallback;
}
uint32_t wledSceneDuration(uint8_t slot,
    CelebrationEngine::Event event,WledZonePolicy::Role role){
  return WledScenePolicy::durationMs(wledScene(slot,event,role),
    uint8_t(event),CelebrationEngine::duration(event,cfg.seconds));
}
uint32_t wledSceneMaxDuration(uint8_t slot,
     CelebrationEngine::Event event,uint16_t mask){
  const uint32_t legacy=CelebrationEngine::duration(event,cfg.seconds);
  if(!cfg.wledZoneEnabled[slot])return legacy;
  uint32_t longest=0;
  for(uint8_t id=0;id<WledZonePolicy::kMaxSegments;++id){
    if(!(mask&(uint16_t(1U)<<id)))continue;
    const WledZonePolicy::Role role=
      WledZonePolicy::role(cfg.wledZoneMap[slot],id);
    const uint32_t duration=wledSceneDuration(slot,event,role);
    if(duration>longest)longest=duration;
  }
  return longest;
}

// Phase 8D: DDP is exclusively available on fully assigned, dedicated
// RGB controllers. A mixed-target goal / PP or a WLED used elsewhere MUST
// stay on the working native JSON segment path. Never use DDP as a broad
// replacement for WLED's existing per-segment JSON semantics.
uint8_t activeDdpCount(){
  uint8_t active=0;
  for(uint8_t i=0;i<MAX_WLED;++i)if(wledDdp[i].active)++active;
  return active;
}
// Shared, read-only WLED geometry parser. Quick Setup uses this to check
// strip coverage without changing a controller's existing WLED segments.
bool readWledDdpLayout(const String &snapshot,uint16_t pixels,
                       uint32_t zoneMap,uint8_t overrideRole,
                       WledDdpPolicy::Layout &layout,String &reason){
  DynamicJsonDocument filter(384);
  filter["on"]=true;
  JsonObject shape=filter["seg"][0].to<JsonObject>();
  shape["id"]=true;shape["start"]=true;shape["stop"]=true;shape["len"]=true;
  DynamicJsonDocument state(3072);
  if(deserializeJson(state,snapshot,DeserializationOption::Filter(filter))){
    reason="Unable to read WLED LED segments";return false;
  }
  if(!(state["on"]|false)){reason="Turn this WLED device on first";return false;}
  JsonArray segments=state["seg"].as<JsonArray>();
  if(segments.isNull()||segments.size()==0||
     segments.size()>WledDdpPolicy::kMaxRanges){
    reason="WLED needs 1–16 existing LED segments";return false;
  }
  layout=WledDdpPolicy::Layout();
  layout.pixels=pixels;
  for(uint8_t j=0;j<segments.size();++j){
    JsonObject seg=segments[j];
    const int id=seg["id"]|(int)j;
    const int first=seg["start"]|0;
    int last=seg["stop"]|0;
    if(last<=first)last=first+(seg["len"]|0);
    if(!WledZonePolicy::validSegment(id)||first<0||last<0||
       first>65535||last>65535){
      reason="WLED segment has an invalid LED range";return false;
    }
    WledDdpPolicy::Range &range=layout.ranges[layout.count++];
    range.start=uint16_t(first);range.stop=uint16_t(last);
    range.id=uint8_t(id);
    range.role=overrideRole?overrideRole:
      WledZonePolicy::role(zoneMap,uint8_t(id));
  }
  return true;
}
bool prepareWledDdp(uint8_t slot,CelebrationEngine::Event event,
                    uint32_t color,uint8_t scorerSide,
                    const String &snapshot,WledDdpSession &plan){
  if(slot>=MAX_WLED || !cfg.wledDdpEnabled[slot] ||
     !cfg.wledZoneEnabled[slot] || snapshot.isEmpty() ||
     event==CelebrationEngine::Event::POWER_PLAY ||
     event>=CelebrationEngine::Event::SHOOTOUT_START ||
     event==CelebrationEngine::Event::NONE){
    if(slot<MAX_WLED)wledDdp[slot].status="Native JSON (DDP not enabled / PP)";
    return false;
  }
  if(activeDdpCount()>=WledDdpPolicy::kMaxConcurrent){
    wledDdp[slot].status="Native JSON (DDP concurrent stream limit)";
    return false;
  }
  WledScanEntry info;
  if(!probeWledInfo(cfg.wled[slot],info,WledDiscoveryPolicy::kScanProbeTimeoutMs)){
    wledDdp[slot].status="Native JSON (WLED info unavailable)";
    return false;
  }
  if(info.live||info.rgbw||info.ledCount>WledDdpPolicy::kMaxPixels){
    wledDdp[slot].status=info.live?
      "Native JSON (WLED already in realtime mode)":
      info.rgbw?"Native JSON (RGBW unsupported)":
                 "Native JSON (over "+String(WledDdpPolicy::kMaxPixels)+" LEDs)";
    return false;
  }
  WledDdpPolicy::Layout layout;
  String layoutReason;
  if(!readWledDdpLayout(snapshot,info.ledCount,cfg.wledZoneMap[slot],0,
                        layout,layoutReason)){
    wledDdp[slot].status="Native JSON ("+layoutReason+")";
    return false;
  }
  const WledDdpPolicy::Fail safety=
      WledDdpPolicy::validate(layout,uint8_t(event),scorerSide);
  if(safety!=WledDdpPolicy::OK){
    wledDdp[slot].status=WledDdpPolicy::reason(safety);
    return false;
  }
  // DDP can only restore a whole controller. Different zone durations
  // need native JSON's individually targeted restoration instead.
  const Game *game=wledSceneGame();
  const uint32_t home=game?teamRgb(game->home):color;
  const uint32_t away=game?teamRgb(game->away):color;
  uint32_t total=0;
  WledDdpSession next;
  next.layout=layout;
  next.event=event;next.fps=cfg.wledDdpFps[slot];
  next.status="DDP ready";
  for(uint8_t j=0;j<layout.count;++j){
    const WledDdpPolicy::Range &range=layout.ranges[j];
    const WledZonePolicy::Role role=
       WledZonePolicy::Role(range.role);
    const uint32_t time=wledSceneDuration(slot,event,role);
    if(!total)total=time;
    else if(time!=total){
      wledDdp[slot].status="Native JSON (different zone durations)";
      return false;
    }
    const WledScenePolicy::Scene &scene=wledScene(slot,event,role);
    const CelebrationEngine::Effect baseline=
        CelebrationEngine::effect(event,0,cfg.wledGoalStyle);
    const CelebrationEngine::Effect chosen=
        WledScenePolicy::effect(scene,baseline);
    const uint32_t standard=WledZonePolicy::zoneColor(role,uint8_t(event),
                                        color,home,away);
    const uint32_t team=role==WledZonePolicy::HOME?home:
       role==WledZonePolicy::AWAY?away:color;
    WledDdpPolicy::Style &style=next.styles[j];
    style.color=WledScenePolicy::rgb(scene,standard,team);
    style.brightness=WledScenePolicy::brightness(scene,cfg.brightness);
    style.fx=chosen.fx;style.speed=chosen.speed;
    style.intensity=chosen.intensity;
  }
  if(total==0||total>WledDdpPolicy::kMaxLiveMs){
    wledDdp[slot].status="Native JSON (unbounded duration)";
    return false;
  }
  next.durationMs=total;
  plan=next;
  return true;
}
bool sendWledDdpFrame(uint8_t slot,uint32_t now){
  if(slot>=MAX_WLED || !wledDdp[slot].active ||
     WiFi.status()!=WL_CONNECTED)return false;
  WledDdpSession &session=wledDdp[slot];
  const uint32_t elapsed=uint32_t(now-session.started);
  // Respect the Phase 8C Strobe→Chase legacy goal style while retaining
  // an explicitly configured scene's native FX selection.
  WledDdpPolicy::Style rendered[WledDdpPolicy::kMaxRanges];
  for(uint8_t j=0;j<session.layout.count;++j){
    rendered[j]=session.styles[j];
    const WledZonePolicy::Role role=WledZonePolicy::Role(
         session.layout.ranges[j].role);
    if(session.event==CelebrationEngine::Event::GOAL &&
       wledScene(slot,session.event,role).fx==WledScenePolicy::kInherit &&
       cfg.wledGoalStyle==0 &&
       elapsed>=session.durationMs/3UL)
      rendered[j].fx=28;
  }
  IPAddress target;
  if(!target.fromString(cfg.wled[slot]) || !validWledLocalIp(target))
    return false;
  // One complete logical frame: up to 480 RGB pixels per UDP datagram.
  // DDP byte offsets are per chunk; only the final packet sets PUSH.
  // Keep memory bounded to a single 1450-byte packet buffer.
  for(uint16_t first=0;first<session.layout.pixels;
      first=uint16_t(first+WledDdpPolicy::kChunkPixels)){
    size_t bytes=0;
    if(!WledDdpPolicy::frameChunk(session.layout,rendered,elapsed,
            session.sequence++,first,wledDdpPacket,sizeof(wledDdpPacket),
            bytes))return false;
    if(!wledDdpUdp.beginPacket(target,WledDdpPolicy::kPort))return false;
    const bool written=wledDdpUdp.write(wledDdpPacket,bytes)==bytes;
    const bool sent=wledDdpUdp.endPacket()==1;
    if(!written||!sent)return false;
    ++session.packets;
  }
  session.lastFrame=now;session.lastGoodFrame=now;++session.frames;
  return true;
}
bool releaseWledDdp(uint8_t slot){
  if(slot>=MAX_WLED)return false;
  if(!wledDdp[slot].active)return true;
  // DDP has taken the controller into WLED realtime mode: explicitly
  // release it BEFORE restoring native segment FX, not just stop UDP.
  const String ip=savedWledIp[slot].isEmpty()?cfg.wled[slot]:
                   savedWledIp[slot];
  if(!postWled(ip,"{\"live\":false,\"udpn\":{\"nn\":true}}"))
    return false;
  wledDdp[slot].active=false;
  wledDdp[slot].manual=false;
  wledDdp[slot].status="DDP released; restoring previous WLED state";
  return true;
}
void tickWledDdpStreams(){
  const uint32_t now=millis();
  for(uint8_t i=0;i<MAX_WLED;++i){
    WledDdpSession &session=wledDdp[i];
    if(!session.active)continue;
    if(!restoring[i] || WiFi.status()!=WL_CONNECTED){
      if(restoring[i])restoreAt[i]=now;
      session.status="DDP interrupted; restore requested";
      continue;
    }
    if(restoreAt[i]!=UINT32_MAX &&
       int32_t(now-restoreAt[i])>=0)continue;
    const uint32_t elapsed=uint32_t(now-session.started);
    if(elapsed>=WledDdpPolicy::kMaxLiveMs ||
       (session.manual && elapsed>=WledDdpPolicy::kPreviewMs)){
      restoreAt[i]=now;
      session.status="DDP duration reached; restoring";
      continue;
    }
    const uint32_t interval=1000UL/session.fps;
    if(session.frames && uint32_t(now-session.lastFrame)<interval)continue;
    if(!sendWledDdpFrame(i,now)){
      restoreAt[i]=now;
      session.status="DDP UDP send failed; restoring";
      wledDeviceResult[i]="DDP transport failure";
    }
  }
}
// Native JSON effects always target WLED's EXISTING segment IDs.
// When mapping is enabled there is intentionally NO legacy segment-0 fallback.
// No start/stop/len/sel fields are written: this never reshapes WLED segments.
String wledEffectBody(const String &snapshot,
                      CelebrationEngine::Event event,uint32_t color,
                      uint8_t phase,uint8_t slot,uint8_t scorerSide,
                      uint16_t &touched,uint16_t skipSegments=0) {
  touched=0;
  DynamicJsonDocument filter(384);
  filter["on"]=true;
  JsonObject templateSegment=filter["seg"][0].to<JsonObject>();
  templateSegment["id"]=true;templateSegment["start"]=true;
  templateSegment["stop"]=true;templateSegment["len"]=true;
  DynamicJsonDocument state(3072);
  if(deserializeJson(state,snapshot,DeserializationOption::Filter(filter)))
    return "";
  const bool mapped=cfg.wledZoneEnabled[slot];
  // With zone mapping, do not wake a whole previously-off controller:
  // setting global ON would unintentionally illuminate unrelated segments.
  if(mapped && !(state["on"]|false))return "";
  DynamicJsonDocument command(4096);
  if(!mapped){
    command["on"]=true; // EXACT old behavior for migrated controllers
    command["bri"]=cfg.brightness;
  }
  command["transition"]=0;
  command["udpn"]["nn"]=true;
  const CelebrationEngine::Effect effect=
    CelebrationEngine::effect(event,phase,cfg.wledGoalStyle);
  JsonArray segments=command.createNestedArray("seg");
  JsonArray current=state["seg"].as<JsonArray>();
  uint8_t emitted=0;
  const Game *game=wledSceneGame();
  const uint32_t homeRgb=game?teamRgb(game->home):color;
  const uint32_t awayRgb=game?teamRgb(game->away):color;
  for(uint8_t j=0;j<current.size() &&
      j<CelebrationEngine::kMaxSegments;++j){
    JsonObject seg=current[j];
    const int id=seg["id"]|(int)j;
    const int first=seg["start"]|0;
    const int stop=seg["stop"]|0;
    const int len=seg["len"]|0;
    if(!WledZonePolicy::validSegment(id) ||
       (stop<=first && len<=0))continue;
    if(!WledZonePolicy::target(mapped,cfg.wledZoneMap[slot],uint8_t(id),
         uint8_t(event),scorerSide))continue;
    if(mapped && (skipSegments&(uint16_t(1U)<<id)))continue;
    const WledZonePolicy::Role zone=
      WledZonePolicy::role(cfg.wledZoneMap[slot],uint8_t(id));
    const WledScenePolicy::Scene &scene=wledScene(slot,event,zone);
    const CelebrationEngine::Effect chosen=mapped?
        WledScenePolicy::effect(scene,effect):effect;
    const uint32_t defaultRgb=mapped?
        WledZonePolicy::zoneColor(zone,uint8_t(event),color,homeRgb,awayRgb):
        color;
    const uint32_t zoneTeamRgb=zone==WledZonePolicy::HOME?homeRgb:
       zone==WledZonePolicy::AWAY?awayRgb:color;
    const uint32_t rgb=mapped?
        WledScenePolicy::rgb(scene,defaultRgb,zoneTeamRgb):color;
    JsonObject out=segments.createNestedObject();
    out["id"]=id;out["on"]=true;
    if(mapped)out["bri"]=cfg.brightness; // original 8B default
    if(mapped && scene.fx!=WledScenePolicy::kInherit)
      out["bri"]=scene.brightness;
    out["fx"]=chosen.fx;out["sx"]=chosen.speed;out["ix"]=chosen.intensity;
    out["pal"]=0;
    JsonArray cols=out.createNestedArray("col");
    JsonArray primary=cols.createNestedArray();
    primary.add((rgb>>16)&255);primary.add((rgb>>8)&255);primary.add(rgb&255);
    JsonArray secondary=cols.createNestedArray();
    secondary.add(0);secondary.add(0);secondary.add(0);
    touched|=uint16_t(1U<<id);
    ++emitted;
  }
  if(!emitted){
    if(mapped)return "";
    // Older WLED versions may omit bounds. Preserve the v0.3.1
    // default seg0 behavior only for unmapped legacy controllers.
    JsonObject out=segments.createNestedObject();
    out["id"]=0;out["on"]=true;
    out["fx"]=effect.fx;out["sx"]=effect.speed;out["ix"]=effect.intensity;
    out["pal"]=0;
    JsonArray cols=out.createNestedArray("col");
    JsonArray primary=cols.createNestedArray();
    primary.add((color>>16)&255);primary.add((color>>8)&255);primary.add(color&255);
    touched=1U;
  }
  if(command.overflowed()){touched=0;return "";}
  String body;serializeJson(command,body);return body;
}

// Restore ONLY the segment IDs that this GameDay session actually changed.
// Unlike the original whole-state restore, unrelated segments keep running
// undisturbed. Preserve original fx, palette, colors, speed and brightness.
// This is intentionally NOT a WLED segment layout editor.
String wledMappedRestoreBody(const String &snapshot,uint16_t touched){
  if(!touched)return "";
  DynamicJsonDocument filter(768);
  JsonObject segFilter=filter["seg"][0].to<JsonObject>();
  for(const char *field:{"id","on","bri","fx","sx","ix","pal",
                         "col","rev","mi","sel"})
    segFilter[field]=true;
  DynamicJsonDocument source(7000);
  if(deserializeJson(source,snapshot,DeserializationOption::Filter(filter)))
    return "";
  DynamicJsonDocument command(7000);
  command["transition"]=0;command["udpn"]["nn"]=true;
  JsonArray targets=command.createNestedArray("seg");
  JsonArray originals=source["seg"].as<JsonArray>();
  uint8_t restored=0;
  for(uint8_t j=0;j<originals.size() &&
       j<WledZonePolicy::kMaxSegments;++j){
    JsonObject seg=originals[j];
    const int id=seg["id"]|(int)j;
    if(!WledZonePolicy::validSegment(id) ||
       !(touched&(uint16_t(1U)<<id)))continue;
    JsonObject out=targets.createNestedObject();out["id"]=id;
    for(const char *field:{"on","bri","fx","sx","ix","pal",
                           "col","rev","mi","sel"})
      if(!seg[field].isNull())out[field]=seg[field];
    ++restored;
  }
  if(!restored || command.overflowed())return "";
  String body;serializeJson(command,body);return body;
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
  wledActiveGameId=0;
  wledEventDuration = 0;
  wledPpTestUntil = 0;
  queuedWledEvent = CelebrationEngine::Event::NONE;
  queuedWledLogSeq = 0;
}

// WLED has two different lease states: an ongoing effect that deliberately
// keeps its original snapshot (restoreAt==UINT32_MAX), and an actual pending
// restore with a finite due time. An ongoing PP must never block NHL polls.
bool wledRestoreWouldConflict(uint32_t now,uint32_t fetchBudgetMs) {
  for(uint8_t i=0;i<MAX_WLED;++i) {
    if(WledRestoreFetchPolicy::dueSoon(
        restoring[i],restoreAt[i],now,fetchBudgetMs))return true;
  }
  return false;
}
void tickRestores() {
  const uint32_t now = millis();
  for (uint8_t i = 0; i < MAX_WLED; ++i) {
    // UINT32_MAX marks a WLED effect that must stay active until the shared
    // event manager ends it (not a timestamp to compare using subtraction).
    if (!restoring[i] || restoreAt[i] == UINT32_MAX ||
        int32_t(now - restoreAt[i]) < 0) continue;
    const bool alreadyRestored=wledRestoreMapped[i] &&
                             wledTouchedSegments[i]==0;
    const String body=alreadyRestored?String(""):
        wledRestoreMapped[i]
          ? wledMappedRestoreBody(savedState[i],wledTouchedSegments[i])
          : wledRestoreBody(savedState[i]);
    const bool ddpReleased=releaseWledDdp(i);
    const bool restored=ddpReleased && (alreadyRestored || (!body.isEmpty() &&
        postWled(savedWledIp[i].length() ? savedWledIp[i] :
                 cfg.wled[i],body)));
    if (restored) {
      wledDeviceResult[i] = "Restored";
      markWledEvent(wledEventOwner[i], i, GameEventLog::Wled::RESTORED);
      wledEventOwner[i] = 0;
      wledDeviceGameOwner[i]=0;
      restoring[i] = false;
      savedState[i] = "";
      savedWledIp[i] = "";
      restoreRetries[i] = 0;
      wledTouchedSegments[i]=0;wledRestoreMapped[i]=false;
      wledCurrentSceneSegments[i]=0;wledRestoredSceneSegments[i]=0;
    } else if (++restoreRetries[i] >= 3) {
      wledDeviceResult[i] = "Restore failed";
      if(wledDdp[i].active){
        wledDdp[i].active=false; // stop sending; WLED realtime eventually times out
        wledDdp[i].status="DDP release/restore failed; check WLED";
      }
      markWledEvent(wledEventOwner[i], i, GameEventLog::Wled::RESTORE_FAILED);
      wledEventOwner[i] = 0;
      wledDeviceGameOwner[i]=0;
      Serial.printf("[WLED] controller %u restore failed after 3 tries\n", i + 1);
      restoring[i] = false;
      savedState[i] = "";
      savedWledIp[i] = "";
      wledTouchedSegments[i]=0;wledRestoreMapped[i]=false;
      wledCurrentSceneSegments[i]=0;wledRestoredSceneSegments[i]=0;
    } else {
      restoreAt[i] = millis() + 1500UL;
      wledDeviceResult[i] = "Retrying restore";
      markWledEvent(wledEventOwner[i], i, GameEventLog::Wled::RESTORE_RETRYING);
    }
  }
}

// A WLED controller has one physical output. Before giving it to a
// DIFFERENT game/event, release DDP and restore every pixel/segment changed
// by the previous lease. Keep the ORIGINAL pre-GameDay snapshot for the final
// restore, rather than snapshotting a different game's temporary effects.
bool cleanWledLeaseForNextEvent(uint8_t slot){
  if(slot>=MAX_WLED || !restoring[slot])return true;
  if(!releaseWledDdp(slot)){
    restoreAt[slot]=millis();
    wledDeviceResult[slot]="DDP release failed; restore pending";
    return false;
  }
  const bool unchanged=wledRestoreMapped[slot]&&wledTouchedSegments[slot]==0;
  const String body=unchanged?String(""):
      wledRestoreMapped[slot]
        ? wledMappedRestoreBody(savedState[slot],wledTouchedSegments[slot])
        : wledRestoreBody(savedState[slot]);
  if(!unchanged && (body.isEmpty() ||
      !postWled(savedWledIp[slot].length()?savedWledIp[slot]:cfg.wled[slot],body))){
    restoreAt[slot]=millis();
    wledDeviceResult[slot]="Previous game restore failed; retry scheduled";
    return false;
  }
  markWledEvent(wledEventOwner[slot],slot,GameEventLog::Wled::SUPERSEDED);
  wledEventOwner[slot]=0;
  wledDeviceGameOwner[slot]=0;
  wledTouchedSegments[slot]=0;
  wledCurrentSceneSegments[slot]=0;
  wledRestoredSceneSegments[slot]=0;
  wledRestoreMapped[slot]=cfg.wledZoneEnabled[slot];
  restoreAt[slot]=UINT32_MAX;
  return true;
}

bool startWledEvent(CelebrationEngine::Event event,uint32_t color,
                    uint32_t eventSequence,uint8_t scorerSide,uint32_t gameId) {
  if(gameId&&!isArmedGame(gameId))return false;
  wledDispatchGameId=gameId?gameId:selectedGameId;
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
      // Eight WLED snapshots can exhaust CYD heap; protect the
      // scoreboard/audio/NHL tasks rather than risking an OOM reboot.
      if(ESP.getFreeHeap()<45000UL){
        wledDeviceResult[i]="Memory budget protected";
        markWledEvent(eventSequence,i,GameEventLog::Wled::SNAPSHOT_FAILED);
        continue;
      }
      savedState[i] = getWledState(cfg.wled[i]);
      savedWledIp[i] = cfg.wled[i];
      wledRestoreMapped[i]=cfg.wledZoneEnabled[i];
      wledTouchedSegments[i]=0;
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
  uint32_t longestSceneMs=0;
  for (uint8_t i = 0; i < MAX_WLED; ++i) {
    if (!prepared[i]) continue;
    if(!cleanWledLeaseForNextEvent(i)){
      markWledEvent(eventSequence,i,GameEventLog::Wled::COMMAND_FAILED);
      continue; // Never paint over an un-restored stream or foreign scene.
    }
    wledEventOwner[i] = eventSequence;
    // DDP realtime must relinquish ownership before a queued NHL event
    // attempts native JSON or a new stream on this same controller.
    if(wledDdp[i].active && !releaseWledDdp(i)){
      restoreAt[i]=millis();
      wledDeviceResult[i]="DDP release pending; effect skipped";
      markWledEvent(eventSequence,i,GameEventLog::Wled::COMMAND_FAILED);
      continue;
    }
    wledCurrentSceneSegments[i]=0;
    wledRestoredSceneSegments[i]=0;
    // Attempt DDP only for verified, fully dedicated controllers. Any
    // range conflict, mixed Home/Away goal, invalid count, RGBW controller
    // or concurrent-stream limit falls back to the original 8C JSON path.
    if(cfg.wledDdpEnabled[i] && cfg.wledZoneEnabled[i]){
      WledDdpSession candidate;
      if(prepareWledDdp(i,event,color,scorerSide,savedState[i],candidate)){
        wledDdp[i]=candidate;
        wledDdp[i].active=true;
        wledDdp[i].started=millis();
        wledDdp[i].status="DDP streaming";
        if(sendWledDdpFrame(i,millis())){
          uint16_t mask=0;
          for(uint8_t j=0;j<candidate.layout.count;++j)
            mask|=uint16_t(1U)<<candidate.layout.ranges[j].id;
          wledTouchedSegments[i]|=mask;
          wledCurrentSceneSegments[i]=mask;
          wledRestoreMapped[i]=true;
          restoring[i]=true;restoreRetries[i]=0;
          restoreAt[i]=UINT32_MAX;
          if(candidate.durationMs>longestSceneMs)
            longestSceneMs=candidate.durationMs;
          wledDeviceResult[i]="DDP streaming";
          wledDeviceGameOwner[i]=gameId;
          markWledEvent(eventSequence,i,GameEventLog::Wled::ACCEPTED,true);
          ++successful;
          continue;
        }
        wledDdp[i].active=false;
        wledDdp[i].status="DDP UDP unavailable; native JSON fallback";
        // Best-effort release if an attempted UDP send reached WLED.
        postWled(cfg.wled[i],"{\"live\":false,\"udpn\":{\"nn\":true}}");
      }
    }
    uint16_t changedMask=0;
    const String body=wledEffectBody(savedState[i],event,color,0,
                                     i,scorerSide,changedMask);
    if(body.isEmpty() && cfg.wledZoneEnabled[i]){
      // An unassigned zone or powered-off controller must not be
      // implicitly reactivated or silently fall back to segment 0.
      wledDeviceResult[i]="No matching zone / WLED off";
      markWledEvent(eventSequence,i,GameEventLog::Wled::NOT_SELECTED);
      if(restoring[i])restoreAt[i]=millis();
      else{savedState[i]="";savedWledIp[i]="";}
      continue;
    }
    wledTouchedSegments[i]|=changedMask;
    if (!body.isEmpty() && postWled(cfg.wled[i], body)) {
      wledCurrentSceneSegments[i]=changedMask;
      const uint32_t duration=wledSceneMaxDuration(i,event,changedMask);
      if(duration>longestSceneMs)longestSceneMs=duration;
      restoring[i] = true;
      restoreRetries[i] = 0;
      // All devices share a single clock; PP has no duration until it ends.
      restoreAt[i] = UINT32_MAX;
      wledDeviceResult[i] = "Playing";
      wledDeviceGameOwner[i]=gameId;
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
    wledActiveGameId=0;
    wledEventDuration = 0;
    return false;
  }
  wledActiveEvent = event;
  wledActiveGameId=gameId;
  wledEventColor = color;
  wledEventSide=scorerSide;
  wledEventStart = commonStart;
  wledEventDuration = longestSceneMs; // longest participating scene sets shared WLED end
  wledPhase = 0;
  Serial.printf("[WLED] %s on %u/%u selected controllers, heap=%u\n",
                CelebrationEngine::label(event), successful,
                wledEligibleCount(event), (unsigned)ESP.getFreeHeap());
  return true;
}

// PP ownership is game-ID based. The highest user-ranked eligible game
// controls shared WLED outputs; stopping one PP never ends another's pulse.
bool wledPpSuppressed(uint32_t gameId){
  for(uint8_t i=0;i<MultiGameArming::kMaxArmed;++i){
    if(wledPpStoppedIds[i]!=gameId)continue;
    const NhlPowerPlayTracker2::Watch *session=
        powerPlaySession(gameId,false);
    const Game *g=gameById(gameId);
    // Manual stop is bound to this PP opportunity, not the whole game.
    // A label gap leaves suppression intact; a new PP clears it.
    if(!isArmedGame(gameId)||!g ||
       !CelebrationEngine::isLive(g->state.c_str()) ||
       !session || !session->side ||
       session->session!=wledPpStoppedSessions[i]){
      wledPpStoppedIds[i]=0;
      wledPpStoppedSessions[i]=0;
      return false;
    }
    return true;
  }
  return false;
}
void suppressArmedPowerPlays(){
  for(uint8_t i=0;i<MultiGameArming::kMaxArmed;++i){
    const uint32_t id=armedGames.ids[i];
    const Game *g=gameById(id);
    const NhlPowerPlayTracker2::Watch *session=
        powerPlaySession(id,false);
    if(id && session && session->side &&
       ((g && CelebrationEngine::isLive(g->state.c_str()) &&
         g->ppDataKnown && g->ppSide) || id==wledActiveGameId &&
         wledActiveEvent==CelebrationEngine::Event::POWER_PLAY)){
      wledPpStoppedIds[i]=id;
      wledPpStoppedSessions[i]=session->session;
    }
  }
}
uint8_t currentWledPowerPlay(uint32_t &color,uint32_t &gameId,
                             uint32_t &sequence) {
  gameId=0;sequence=0;
  if(!cfg.wledPP)return 0;
  for(uint8_t i=0;i<MultiGameArming::kMaxArmed;++i){
    const Game *g=gameById(armedGames.ids[i]);
    if(!g || !CelebrationEngine::isLive(g->state.c_str()) ||
       g->intermission || !g->ppDataKnown || !g->ppSide ||
       uint32_t(millis()-g->ppUpdatedAt)>=80000UL ||
       wledPpSuppressed(g->id))continue;
    color=teamRgb(g->ppSide==1?g->home:g->away);
    gameId=g->id;
    sequence=(g->id==selectedGameId)?armedPpLogSeq:0;
    return g->ppSide;
  }
  return 0;
}


void dispatchHueEvent(CelebrationEngine::Event event,uint32_t color,
                      uint32_t sequence,uint32_t gameId){
  if(gameId&&!isArmedGame(gameId))return;
  if(!hue.ready()||
     !HuePolicy::participates(hue.config.enabled,hue.config.events,event))return;
  if(!hue.idle()&&CelebrationEngine::temporary(hue.event)&&
     MultiGameArming::priority(event)<=MultiGameArming::priority(hue.event)){
    // Match WLED's bounded multi-game goal queue: second simultaneous
    // scored events cannot erase the first game's Hue celebration.
    if(gameId){
      MultiGameArming::Request item;
      item.event=event;item.gameId=gameId;item.color=color;
      item.sequence=sequence;
      if(!huePriorityQueue.add(item,armedGames))return;
      GameEventLog::Entry *queued=gameEventHistory.find(sequence);
      if(queued)queued->hue=GameEventLog::Wled::PENDING;
    }
    return;
  }
  hue.begin(event,color,sequence,gameId);
  GameEventLog::Entry *row=gameEventHistory.find(sequence);
  if(row)row->hue=GameEventLog::Wled::PENDING;
}

// The shared physical WLED channel has one owner at a time. Requests are
// bounded, ordered GOAL > FINAL > GAME_START > PP, then game rank (1..5).
// The second simultaneous goal queues rather than destroying the first.
bool queueArmedWledEvent(CelebrationEngine::Event event,uint32_t color,
                         uint32_t sequence,uint8_t side,uint32_t gameId){
  if(!gameId || !isArmedGame(gameId))return false;
  dispatchHueEvent(event,color,sequence,gameId);
  if(!wledEligibleCount(event))return false;
  MultiGameArming::Request item;
  item.event=event;item.gameId=gameId;item.color=color;
  item.sequence=sequence;item.side=side;
  const bool higher=MultiGameArming::priority(event)>
      MultiGameArming::priority(wledActiveEvent);
  if(wledActiveEvent==CelebrationEngine::Event::NONE || higher ||
     wledActiveEvent==CelebrationEngine::Event::POWER_PLAY) {
    if(startWledEvent(event,color,sequence,side,gameId))return true;
  }
  if(!wledPriorityQueue.add(item,armedGames)){
    ++wledQueuedDrops;return false;
  }
  for(uint8_t i=0;i<MAX_WLED;++i)
    if(wledParticipates(i,event))
      markWledEvent(sequence,i,GameEventLog::Wled::PENDING);
  return true;
}

// Expire individual customized scene zones early without restoring or
// changing unrelated segments. A failed targeted restore is retried at the
// next bounded tick; the normal end-of-event restore remains a fallback.
void tickWledSceneExpirations(uint32_t now,uint32_t elapsed){
  if(uint32_t(now-lastWledSceneExpiryCheck)<400UL)return;
  lastWledSceneExpiryCheck=now;
  for(uint8_t i=0;i<MAX_WLED;++i){
    if(!restoring[i] || wledDdp[i].active || !wledRestoreMapped[i] ||
       !cfg.wledZoneEnabled[i] || !wledCurrentSceneSegments[i])continue;
    uint16_t expired=0;
    for(uint8_t id=0;id<WledZonePolicy::kMaxSegments;++id){
      const uint16_t bit=uint16_t(1U)<<id;
      if(!(wledCurrentSceneSegments[i]&bit) ||
         (wledRestoredSceneSegments[i]&bit))continue;
      const WledZonePolicy::Role zone=
          WledZonePolicy::role(cfg.wledZoneMap[i],id);
      const uint32_t duration=wledSceneDuration(i,wledActiveEvent,zone);
      if(duration>0 && elapsed>=duration)
        expired|=bit;
    }
    if(!expired)continue;
    const String body=wledMappedRestoreBody(savedState[i],expired);
    if(!body.isEmpty() && postWled(cfg.wled[i],body)){
      wledRestoredSceneSegments[i]|=expired;
      wledCurrentSceneSegments[i]&=~expired;
      wledTouchedSegments[i]&=~expired;
      wledDeviceResult[i]="Scene zone finished";
    }else{
      wledDeviceResult[i]="Scene zone restore retrying";
      Serial.printf("[WLED Scene] slot %u early restore retry mask=%04X\n",
                    unsigned(i),unsigned(expired));
    }
  }
}

// Try the highest priority queued event, preserving its original game ID.
// Back off failed WLED network requests instead of blocking the NHL loop.
bool startQueuedWledEvent(uint32_t now){
  static uint32_t lastAttempt=0;
  if(!wledPriorityQueue.count || (lastAttempt &&
     uint32_t(now-lastAttempt)<1500UL))return false;
  lastAttempt=now;
  MultiGameArming::Request item;
  while(wledPriorityQueue.pop(item,armedGames)){
    if(item.gameId && !isArmedGame(item.gameId))continue;
    if(startWledEvent(item.event,item.color,item.sequence,
                      item.side,item.gameId))return true;
    if(!wledPriorityQueue.add(item,armedGames))++wledQueuedDrops;
    break;
  }
  return false;
}
void tickWledCelebrations() {
  const uint32_t now=millis();
  uint32_t ppColor=0,ppGameId=0,ppSeq=0;
  uint8_t ppSide=0;
  const bool testPP=wledPpTestUntil &&
                   int32_t(wledPpTestUntil-now)>0;
  if(testPP) {
    ppColor=wledEventColor;ppSide=wledEventSide;
    ppGameId=0; // Diagnostic, not an armed NHL event.
  }else if(int32_t(wledSuppressPpUntil-now)<=0){
    ppSide=currentWledPowerPlay(ppColor,ppGameId,ppSeq);
  }
  const bool hasPP=ppSide!=0;

  if(wledActiveEvent!=CelebrationEngine::Event::NONE &&
     CelebrationEngine::temporary(wledActiveEvent)){
    const uint32_t elapsed=uint32_t(now-wledEventStart);
    if(elapsed<wledEventDuration){
      tickWledSceneExpirations(now,elapsed);
      const uint8_t nextPhase=CelebrationEngine::phase(
          wledActiveEvent,elapsed,wledEventDuration,cfg.wledGoalStyle);
      if(nextPhase!=wledPhase){
        wledPhase=nextPhase;
        wledDispatchGameId=wledActiveGameId?wledActiveGameId:selectedGameId;
        for(uint8_t i=0;i<MAX_WLED;++i){
          if(!restoring[i] || wledDdp[i].active ||
             !wledParticipates(i,wledActiveEvent))continue;
          uint16_t phaseTouched=0;
          const String body=wledEffectBody(savedState[i],wledActiveEvent,
                 wledEventColor,wledPhase,i,wledEventSide,phaseTouched,
                 wledRestoredSceneSegments[i]);
          wledTouchedSegments[i]|=phaseTouched;
          wledCurrentSceneSegments[i]|=phaseTouched;
          if(!body.isEmpty() && postWled(cfg.wled[i],body))
            wledDeviceResult[i]="Playing";
          else{
            wledDeviceResult[i]="Phase update failed";
            markWledEvent(wledEventOwner[i],i,
                          GameEventLog::Wled::COMMAND_FAILED);
          }
        }
      }
      return;
    }
    // The temporary goal/final/start finished. Dispatch waiting goals before
    // resuming the highest-ranked still-active PP. Original WLED snapshots
    // are only discarded after full controller restoration.
    if(wledPriorityQueue.count){
      if(startQueuedWledEvent(now))return;
      scheduleWledRestoreAll();return;
    }
    if(hasPP && startWledEvent(CelebrationEngine::Event::POWER_PLAY,
                               ppColor,ppSeq,ppSide,ppGameId))return;
    scheduleWledRestoreAll();return;
  }

  if(wledPriorityQueue.count &&
     (wledActiveEvent==CelebrationEngine::Event::NONE ||
      wledActiveEvent==CelebrationEngine::Event::POWER_PLAY)){
    if(startQueuedWledEvent(now))return;
  }
  if(wledActiveEvent==CelebrationEngine::Event::POWER_PLAY){
    if(!hasPP){
      // The owner PP ended or became stale: release DDP and restore.
      scheduleWledRestoreAll();
    }else if(wledActiveGameId!=ppGameId ||
             wledEventColor!=ppColor || wledEventSide!=ppSide){
      // Another ranked armed PP took over; restore old game segments first.
      startWledEvent(CelebrationEngine::Event::POWER_PLAY,
                     ppColor,ppSeq,ppSide,ppGameId);
    }
  }else if(hasPP && wledActiveEvent==CelebrationEngine::Event::NONE &&
           !wledPriorityQueue.count &&
           wledEligibleCount(CelebrationEngine::Event::POWER_PLAY)>0){
    static uint32_t lastPpAttempt=0;
    if(uint32_t(now-lastPpAttempt)>8000UL){
      lastPpAttempt=now;
      startWledEvent(CelebrationEngine::Event::POWER_PLAY,
                     ppColor,ppSeq,ppSide,ppGameId);
    }
  }
}

// Horn remains independent of WLED network success. Only a real observed
// goal from an armed NHL game enters the per-game priority queue.
void celebrate(uint32_t color,uint32_t eventSequence,
               uint8_t scorerSide,uint32_t gameId) {
  if(gameId)queueArmedWledEvent(CelebrationEngine::Event::GOAL,color,
                               eventSequence,scorerSide,gameId);
  else startWledEvent(CelebrationEngine::Event::GOAL,color,eventSequence,
                      scorerSide,0); // Existing local hardware preview.
  startHorn(eventSequence);
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
  uint32_t winningUrgency=0;
  for(uint8_t i=0;i<gameCount;++i) {
    Game &g=games[i];
    if(g.state!="LIVE" && g.state!="CRIT")continue;
    if(g.intermission)continue;
    const bool shown=screen==Screen::LIVE &&
        visible.kind==LiveCarousel::Kind::GAME && visible.index==i;
    if(!rearFollows(g.id)&&!shown&&!cfg.displayAllLiveEvents)continue;
    NhlPenaltyLedger::Book *candidate=penaltyLedger(g.id);
    if(!candidate)continue;
    const uint32_t age=candidate->loaded?
        uint32_t(millis()-candidate->checkedAt):3600000UL;
    if(candidate->loaded&&age<30000UL)continue;
    const NhlPowerPlayTracker2::Watch *session=
        powerPlaySession(g.id,false);
    // Fair scheduling: a continuously visible carousel cannot starve an
    // armed PP of official penalty verification for many minutes. Age is
    // dominant; armed/current-PP sessions break ties and non-followed
    // league-wide games remain last. No additional HTTPS requests.
    const uint32_t urgency=age/1000UL*10UL+
        (isArmedGame(g.id)?45UL:0UL)+
        (session&&session->side?35UL:0UL)+
        (shown?20UL:rearFollows(g.id)?10UL:0UL);
    if(!target||urgency>winningUrgency){
      winningUrgency=urgency;target=&g;book=candidate;
    }
  }
  if(!target||!book)return;
  const bool tickerTargetVisible=screen==Screen::LIVE &&
      visible.kind==LiveCarousel::Kind::GAME &&
      visible.index<gameCount && &games[visible.index]==target;
  if(tickerTargetVisible)liveTickerWork.reset(target->id);
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
  DynamicJsonDocument filter(3072);
  JsonObject summary = filter["summary"].to<JsonObject>();
  JsonObject penaltyPeriod = summary["penalties"][0].to<JsonObject>();
  penaltyPeriod["periodDescriptor"]["number"]=true;
  JsonObject penalty = penaltyPeriod["penalties"][0].to<JsonObject>();
  penalty["timeInPeriod"]=true;
  penalty["type"]=true;
  penalty["duration"]=true;
  penalty["descKey"]=true;
  penalty["teamAbbrev"]["default"]=true;
  // Accept either an NHL player object (firstName/lastName/name), a
  // localized default string, or the optional servedBy name for bench minors.
  penalty["committedByPlayer"]=true;
  penalty["servedBy"]=true;
  JsonObject status=filter["situation"].to<JsonObject>();
  status["homeTeam"]["strength"]=true;
  status["awayTeam"]["strength"]=true;
  status["homeTeam"]["goaliePulled"]=true;
  status["awayTeam"]["goaliePulled"]=true;
  status["homeTeam"]["situationDescriptions"][0]=true;
  status["awayTeam"]["situationDescriptions"][0]=true;
  JsonObject scorePeriod = summary["scoring"][0].to<JsonObject>();
  scorePeriod["periodDescriptor"]["number"]=true;
  JsonObject goal=scorePeriod["goals"][0].to<JsonObject>();
  goal["timeInPeriod"]=true;
  goal["strength"]=true;
  goal["teamAbbrev"]["default"]=true;
  goal["firstName"]["default"]=true;
  goal["lastName"]["default"]=true;
  goal["name"]["default"]=true;
  DynamicJsonDocument doc(24576);
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
  const JsonVariantConst landingSituation=doc["situation"];
  if(!landingSituation.isNull() &&
     (target->state=="LIVE"||target->state=="CRIT") &&
     !target->intermission){
    target->ppCandidate=detectPowerPlay(landingSituation);
    target->ppSituationKnown=true;
  }
  if(penalties.isNull() && scoring.isNull())return;
  NhlPenaltyLedger::Book next;
  next.reset(target->id);
  for (JsonVariantConst period : penalties) {
    const uint8_t p=period["periodDescriptor"]["number"]|0;
    for (JsonVariantConst call : period["penalties"].as<JsonArrayConst>()) {
      const String team=call["teamAbbrev"]["default"].as<String>();
      const uint8_t penalized=team==target->home ? 1 :
          team==target->away ? 2 : 0;
      const char *when=call["timeInPeriod"]|"";
      const char *type=call["type"]|"";
      const uint8_t minutes=call["duration"]|0;
      next.addPenalty(p,when,penalized,type,minutes,
          call["descKey"]|"");
      if(tickerTargetVisible) {
        uint16_t elapsed=0;
        if(NhlPenaltyLedger::Book::parseTime(when,elapsed)) {
          // ArduinoJson can stringify absent optional servedBy as "null".
          // Only accept a JSON string with a meaningful value; prefer the
          // official offending player, using servedBy for bench penalties.
          const auto safeName=[](JsonVariantConst value)->String {
            if(!value.is<const char*>())return String();
            const char *valueText=value.as<const char*>();
            return NhlLiveTicker::validPlayerName(valueText)?
                String(valueText):String();
          };
          const JsonVariantConst committed=call["committedByPlayer"];
          const JsonVariantConst served=call["servedBy"];
          const auto fullName=[&safeName](JsonVariantConst candidate)->String {
            String first=safeName(candidate["firstName"]["default"]);
            String last=safeName(candidate["lastName"]["default"]);
            if(first.length() && last.length())first+=" ";
            first+=last;
            if(first.isEmpty())first=safeName(candidate["name"]["default"]);
            if(first.isEmpty())first=safeName(candidate["default"]);
            if(first.isEmpty())first=safeName(candidate);
            return first;
          };
          String player=(!strcmp(type,"BEN"))?
              fullName(served):fullName(committed);
          if(player.isEmpty())player=fullName(served);
          if(player.isEmpty())player=fullName(committed);
          liveTickerWork.addPenalty(p,elapsed,penalized,team.c_str(),
              player.c_str(),call["descKey"]|"",type,minutes);
        }
      }
    }
  }
  for (JsonVariantConst period : scoring) {
    const uint8_t p=period["periodDescriptor"]["number"]|0;
    for (JsonVariantConst goal : period["goals"].as<JsonArrayConst>()) {
      const char *strength=goal["strength"]|"";
      const String team=goal["teamAbbrev"]["default"].as<String>();
      const uint8_t scoringSide=team==target->home ? 1 :
          team==target->away ? 2 : 0;
      const char *when=goal["timeInPeriod"]|"";
      // Keep verified PP event eligibility exactly as it was before ticker.
      if(NhlLiveTicker::isPowerPlayGoal(strength))
        next.addPpGoal(p,when,scoringSide);
      if(tickerTargetVisible) {
        uint16_t elapsed=0;
        if(NhlPenaltyLedger::Book::parseTime(when,elapsed)) {
          String player=goal["name"]["default"].as<String>();
          if(player.isEmpty()) {
            player=goal["firstName"]["default"].as<String>();
            const String last=goal["lastName"]["default"].as<String>();
            if(player.length() && last.length())player+=" ";
            player+=last;
          }
          liveTickerWork.addGoal(p,elapsed,scoringSide,team.c_str(),
              player.c_str(),when,strength);
        }
      }
    }
  }
  if(tickerTargetVisible) {
    liveTickerWork.applyPowerPlayGoals();
    liveTickerWork.fetchedAt=millis();
    liveTickerFeed=liveTickerWork;
    liveTickerRefreshAt=0;
    liveTickerDirty=true;
    Serial.printf("[TICKER] game=%lu goals=%u penalties=%u\n",
        (unsigned long)target->id,liveTickerFeed.goalCount,
        liveTickerFeed.penaltyCount);
  }
  next.loaded=true;
  next.checkedAt=millis();
  *book=next;
  const uint8_t oldPP=target->ppSide;
  // Never extend the PP2 lighting grace using an old scoreboard clock.
  // A landing summary may remain reachable even while score/{date} has
  // stopped updating; its penalty is not proof that the clock hasn't moved.
  // The last trusted session expires safely after its bounded grace.
  if(nhlLastSuccessAt &&
     uint32_t(millis()-nhlLastSuccessAt)<=90000UL)
    validatePowerPlay(*target);
  else Serial.printf("[PP2] id=%lu landing cached but scoreboard "
                     "age=%lus: withholding stale-clock PP confirmation\n",
      (unsigned long)target->id,
      (unsigned long)(nhlLastSuccessAt?
          uint32_t(millis()-nhlLastSuccessAt)/1000UL:0UL));
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
    // Reset only RAW situation input, never the verified PP session/output.
    // Array positions can reorder; tracker state is independently game-ID
    // keyed. Any stale cached output is replaced by syncPowerPlayOutput().
    if(priorId!=g.id){
      g.ppSide=0;g.ppDataKnown=false;g.ppUpdatedAt=0;
      g.ppSession=0;g.ppPhase=NhlPowerPlayTracker2::Phase::IDLE;
    }
    g.ppCandidate=0;
    g.ppSituationKnown=false;
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
    carouselHold.reset();
    overlayReturnHeldGameId=0;
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
  // A brief missing game in the NHL feed must not implicitly resume cycling.
  carouselHold.onFeedRefresh(!newDay);
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
  bool armsPruned=false;
  for(int i=MultiGameArming::kMaxArmed-1;i>=0;--i){
    const uint32_t id=armedGames.ids[i];
    if(id&&!isScheduledGame(id)){
      armedGames.remove(id);
      wledPriorityQueue.removeGame(id);
      armsPruned=true;
    }
  }
  if(armsPruned){
    if(wledActiveGameId&&!isArmedGame(wledActiveGameId))
      scheduleWledRestoreAll();
    selectedGameId=armedGames.primary();
    for(uint8_t i=0;i<MultiGameArming::kMaxArmed;++i)
      prefs.putUInt(("armed"+String(i)).c_str(),armedGames.ids[i]);
    prefs.putUInt("game",selectedGameId);
    haveBaseline=false;
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
  observeShootoutStarts();
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
        const bool secondaryArmed=isArmedGame(observed.id);
        const String representative = secondaryArmed
          ? (cfg.favoriteSide==0?observed.home:observed.away)
          : ended
            ? (observed.homeScore>observed.awayScore?
               observed.home:observed.away)
            : DisplayEventPolicy::isFavorite(cfg.favoriteTeams,
                                             observed.away.c_str())
              ? observed.away:observed.home;
        const bool homeWon=observed.homeScore>observed.awayScore;
        const bool favoredWon=cfg.favoriteSide==0?homeWon:!homeWon;
        const CelebrationEngine::Event event=started
            ? CelebrationEngine::Event::GAME_START
            : secondaryArmed && !favoredWon
              ? CelebrationEngine::Event::FINAL_LOSS
              : CelebrationEngine::Event::FINAL_WIN;
        const GameEventLog::Type logType=started
            ? GameEventLog::Type::GAME_START
            : event==CelebrationEngine::Event::FINAL_LOSS
              ? GameEventLog::Type::FINAL_LOSS
              : GameEventLog::Type::FINAL_WIN;
        const EventOverlay::Kind kind=started
            ? EventOverlay::Kind::GAME_START
            : event==CelebrationEngine::Event::FINAL_LOSS
              ? EventOverlay::Kind::FINAL_LOSS
              : EventOverlay::Kind::FINAL_WIN;
        GameEventLog::Entry &log=addGameEvent(observed,logType,representative,0);
        log.rear=GameEventLog::Rear::NOT_SELECTED;
        if(secondaryArmed){
          if(started)startGameStartSound(log.sequence);
          else if(ended)startGameFinishSound(log.sequence);
        }
        queueGameOverlay(&observed,kind,representative,log.sequence,true);
        if(secondaryArmed){
#ifdef GLOWDECK_WAVESHARE_S3
          if(cfg.caseLedsEnabled && cfg.caseStart && started) {
            caseLedFx.trigger(S3CaseLedPolicy::Side::HOME,
                S3CaseLedPolicy::Event::GAME_START,
                teamRgb(observed.home),millis());
            caseLedFx.trigger(S3CaseLedPolicy::Side::AWAY,
                S3CaseLedPolicy::Event::GAME_START,
                teamRgb(observed.away),millis());
          }else if(cfg.caseLedsEnabled && cfg.caseFinal && ended &&
                   observed.homeScore!=observed.awayScore){
            const bool winnerHome=observed.homeScore>observed.awayScore;
            caseLedFx.trigger(winnerHome?S3CaseLedPolicy::Side::HOME:
                S3CaseLedPolicy::Side::AWAY,S3CaseLedPolicy::Event::FINAL,
                teamRgb(winnerHome?observed.home:observed.away),millis());
          }
#endif
          if(started?cfg.wledGameStart:cfg.wledFinal)
            queueArmedWledEvent(event,teamRgb(representative),
                                log.sequence,WledZonePolicy::kUnknownSide,
                                observed.id);
          else dispatchHueEvent(event,teamRgb(representative),
                                log.sequence,observed.id);
        }
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
      if(isArmedGame(g->id)){
        if(transition==CelebrationEngine::Event::GAME_START)
          startGameStartSound(logged.sequence);
        else if(transition==CelebrationEngine::Event::FINAL_WIN ||
                transition==CelebrationEngine::Event::FINAL_LOSS)
          startGameFinishSound(logged.sequence);
      }
      const EventOverlay::Kind kind =
        transition == CelebrationEngine::Event::GAME_START
          ? EventOverlay::Kind::GAME_START
          : transition == CelebrationEngine::Event::FINAL_WIN
            ? EventOverlay::Kind::FINAL_WIN
            : EventOverlay::Kind::FINAL_LOSS;
      queueGameOverlay(g, kind, fav, logged.sequence, true);
#ifdef GLOWDECK_WAVESHARE_S3
      if(cfg.caseLedsEnabled &&
         transition==CelebrationEngine::Event::GAME_START && cfg.caseStart){
        caseLedFx.trigger(S3CaseLedPolicy::Side::HOME,
           S3CaseLedPolicy::Event::GAME_START,teamRgb(g->home),millis());
        caseLedFx.trigger(S3CaseLedPolicy::Side::AWAY,
           S3CaseLedPolicy::Event::GAME_START,teamRgb(g->away),millis());
      }else if(cfg.caseLedsEnabled && cfg.caseFinal &&
               transition!=CelebrationEngine::Event::GAME_START &&
               g->homeScore!=g->awayScore){
        const bool winnerHome=g->homeScore>g->awayScore;
        caseLedFx.trigger(winnerHome?S3CaseLedPolicy::Side::HOME:
                                    S3CaseLedPolicy::Side::AWAY,
            S3CaseLedPolicy::Event::FINAL,
            teamRgb(winnerHome?g->home:g->away),millis());
      }
#endif
      const bool enabled = transition == CelebrationEngine::Event::GAME_START
                           ? cfg.wledGameStart : cfg.wledFinal;
      if(enabled)queueArmedWledEvent(transition,teamRgb(fav),
                                     logged.sequence,
                                     WledZonePolicy::kUnknownSide,g->id);
      else dispatchHueEvent(transition,teamRgb(fav),logged.sequence,g->id);
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
  const bool armed=isArmedGame(g.id);
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
      tft.drawString("A"+String(armPriority(g.id)),x+15,y-9,1);
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
  // All page controls are compact vector symbols, outside the live score.
  GameDayIcons::draw(tft,GameDayIcons::Kind::PREVIOUS,184,16,12,WHITE);
  GameDayIcons::draw(tft,carouselHold.paused?GameDayIcons::Kind::PLAY:
      GameDayIcons::Kind::PAUSE,208,16,12,carouselHold.paused?ICE_BLUE:WHITE);
  GameDayIcons::draw(tft,GameDayIcons::Kind::NEXT,232,16,12,WHITE);
  tft.setTextColor(ICE_TEXT, ARENA_BLACK);
  tft.drawString(carouselHold.paused?"HELD":String(carouselSlide+1)+"/"+
                 String(carouselPageCount()),265,16,1);
  tft.fillCircle(291,16,3,WiFi.status()==WL_CONNECTED?ICE_BLUE:ORANGE);
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


// Live ticker is presentation-only: never arms a game, changes PP evidence,
// or starts a WLED/audio event. Content is derived from official NHL summaries
// already fetched for live PP verification plus the cached team-stat rail.
String buildLiveTickerText(const Game &g) {
  String text;
  text.reserve(650);
  const bool haveFeed=liveTickerFeed.gameId==g.id &&
                      liveTickerFeed.fetchedAt!=0;
  uint16_t remaining=0;
  const bool haveClock=g.period>0 &&
      NhlPenaltyLedger::Book::parseTime(g.clock.c_str(),remaining);
  const bool regularOt=g.period==4 && g.periodType=="OT" &&
      ((g.id/10000UL)%100UL)==2UL;
  const uint16_t at=haveClock?
      NhlLiveTicker::elapsedAt(g.period,remaining,regularOt):0;
  const bool boxesFresh=haveFeed && haveClock && !g.intermission &&
      uint32_t(millis()-liveTickerFeed.fetchedAt)<75000UL;
  text+="BOX: ";
  uint8_t active=0;
  if(boxesFresh) {
    for(uint8_t i=0;i<liveTickerFeed.penaltyCount;++i){
      const auto &p=liveTickerFeed.penalties[i];
      if(!NhlLiveTicker::inBox(at,p.at,p.end))continue;
      if(active++)text+=", ";
      text+=p.team;
      text+=" ";
      text+=NhlLiveTicker::validPlayerName(p.player)?
          p.player:"PLAYER TBD";
      text+=" (";
      text+=String(p.minutes);
      text+="m";
      if(p.reason[0]){text+=" ";text+=p.reason;}
      text+=")";
      if(active==4)break; // four names is enough for a 480px marquee
    }
  }
  // "UPDATING" when nobody is penalized was misleading. Only say EMPTY
  // after a fresh official penalty snapshot proves there are no box occupants;
  // "--" means no trustworthy player/clock snapshot currently exists.
  if(boxesFresh && !active)text+="EMPTY";
  else if(!boxesFresh)text+=g.intermission?"INTERMISSION":"--";
  text+="   |   GOALS: ";
  if(haveFeed){
    if(!liveTickerFeed.goalCount)text+="NONE YET";
    else {
      uint8_t shown=0;
      for(int16_t i=int16_t(liveTickerFeed.goalCount)-1;i>=0;--i){
        const auto &goal=liveTickerFeed.goals[i];
        if(shown++)text+="; ";
        text+=goal.team;
        text+=" ";
        text+=NhlLiveTicker::validPlayerName(goal.player)?
            goal.player:"GOAL SCORER TBD";
        text+=" (P";
        text+=String(goal.period);
        text+=" ";
        text+=goal.time;
        if(goal.powerPlay)text+=" PP";
        text+=")";
        if(shown>=8)break; // most recent eight scorers
      }
    }
  } else text+="UPDATING";
  for(uint8_t i=0;i<gameCount;++i){
    if(games[i].id!=g.id || !carouselStats[i].loaded ||
       carouselStats[i].id!=g.id)continue;
    const CarouselStats &stats=carouselStats[i];
    if(stats.hitsAway.length() && stats.hitsHome.length())
      text+="   |   HITS: "+g.away+" "+stats.hitsAway+
            " / "+g.home+" "+stats.hitsHome;
    if(stats.blocksAway.length() && stats.blocksHome.length())
      text+="   |   BLOCKS: "+g.away+" "+stats.blocksAway+
            " / "+g.home+" "+stats.blocksHome;
    if(stats.faceAway.length() && stats.faceHome.length())
      text+="   |   FACEOFF: "+g.away+" "+stats.faceAway+
            " / "+g.home+" "+stats.faceHome;
    if(stats.pimAway.length() && stats.pimHome.length())
      text+="   |   PIM: "+g.away+" "+stats.pimAway+
            " / "+g.home+" "+stats.pimHome;
    break;
  }
  if(text.length()>1000)text.remove(1000);
  return text;
}
void refreshLiveTickerText(const Game &g) {
  const String updated=buildLiveTickerText(g);
  const uint8_t goals=liveTickerFeed.gameId==g.id?liveTickerFeed.goalCount:0;
  const bool newGame=liveTickerTextGameId!=g.id;
  const bool newGoals=goals!=liveTickerShownGoalCount;
  const bool changed=newGame || updated!=liveTickerText;
  if(newGame || newGoals || changed) {
    liveTickerScroll=0;
    liveTickerPixelOffset=0;
  }
  if(changed) {
    liveTickerDirty=true;
    liveTickerTextWidthPx=uint16_t(tft.textWidth(updated,2));
  }
  liveTickerText=updated;
  liveTickerTextGameId=g.id;
  liveTickerShownGoalCount=goals;
}
// Advance on UTF-8 code-point boundaries; never start rendering in the
// middle of an accented NHL player's name.
uint16_t liveTickerNextByte(uint16_t index) {
  const uint16_t len=uint16_t(liveTickerText.length());
  const uint16_t cycle=len+NhlLiveTicker::kSeparatorChars;
  if(index>=cycle-1)return 0;
  if(index>=len)return index+1;
  const uint8_t lead=uint8_t(liveTickerText[index]);
  const uint8_t bytes=(lead&0xF8)==0xF0?4:
                      (lead&0xF0)==0xE0?3:(lead&0xE0)==0xC0?2:1;
  const uint16_t next=index+bytes;
  return next<len?next:len;
}
String liveTickerGlyph(uint16_t index) {
  const uint16_t len=uint16_t(liveTickerText.length());
  if(index>=len)return String(" ");
  const uint16_t next=liveTickerNextByte(index);
  return liveTickerText.substring(index,next);
}
// Draw only enough UTF-8 glyphs for the physical strip. The first glyph is
// shifted by liveTickerPixelOffset, giving smooth sub-character movement.
String liveTickerWindow(uint16_t maxWidth,uint8_t font) {
  if(!liveTickerText.length())return String("NHL LIVE");
  if(liveTickerTextWidthPx<=maxWidth)return liveTickerText;
  String visible;
  visible.reserve(110);
  uint16_t at=liveTickerScroll;
  int16_t width=-int16_t(liveTickerPixelOffset);
  for(uint8_t n=0;n<90 && width<int16_t(maxWidth+16);++n) {
    const String glyph=liveTickerGlyph(at);
    visible+=glyph;
    width+=tft.textWidth(glyph,font);
    at=liveTickerNextByte(at);
  }
  return visible;
}
void drawLiveTickerCYD(const Game &g) {
  if(!gameIsLive(g))return;
  tft.fillRect(10,206,300,25,ARENA_PLATE);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(WHITE,ARENA_PLATE);
  tft.drawString(liveTickerWindow(264,2),
                 26-int16_t(liveTickerPixelOffset),219,2);
  // The first/last partially visible glyphs remain inside the lower plate.
  tft.fillRect(10,206,16,25,ARENA_PLATE);
  tft.fillRect(294,206,16,25,ARENA_PLATE);
  tft.fillRect(10,207,2,23,to565(teamRgb(g.away)));
  tft.fillRect(308,207,2,23,to565(teamRgb(g.home)));
  tft.setTextDatum(TL_DATUM);
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
    const NhlPowerPlayTracker2::Watch *session=
        powerPlaySession(g.id,false);
    if(gameIsLive(g)&&g.intermission&&session&&session->side &&
       session->phase==NhlPowerPlayTracker2::Phase::INTERMISSION){
      const int16_t x=session->side==2?79:179;
      tft.fillRoundRect(x,143,62,18,4,0xC3A0);
      tft.setTextColor(WHITE,0xC3A0);
      tft.drawString("PP NEXT",x+31,152,1);
    }
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
  if(gameIsLive(g)) {
    drawLiveTickerCYD(g);
    return;
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
    case EventOverlay::Kind::SHOOTOUT_START:
      description=String(e.away)+" VS "+String(e.home);break;
    case EventOverlay::Kind::SHOOTOUT_GOAL:
      description=String(e.team)+" SCORES IN THE SHOOTOUT";break;
    case EventOverlay::Kind::SHOOTOUT_SAVE:
      description=String(e.team)+" SHOOTOUT SAVE";break;
    case EventOverlay::Kind::SHOOTOUT_MISS:
      description=String(e.team)+" SHOT MISSED";break;
    case EventOverlay::Kind::POWER_PLAY_START:
      description = String(e.team) + " MAN ADVANTAGE"; break;
    case EventOverlay::Kind::POWER_PLAY_END:
      description = String(e.team) + " BACK AT FULL STRENGTH"; break;
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

// Native S3 celebration renderers are defined after the S3 color palette.
#ifdef GLOWDECK_WAVESHARE_S3
void drawS3EventStatic();
void drawS3EventAnimation(uint32_t now);
void drawS3GifBackdrop(const EventOverlay::Item &e);
void drawS3GoalGifHud(const EventOverlay::Item &e, uint32_t now);
#endif

// Called at most once per ~140ms for goals and 240ms for calmer events.
// Colors animate only the perimeter; no full-screen flashing.
void drawOverlayAnimation(uint32_t now) {
  if (!overlayController.active()) return;
  const EventOverlay::Item &e = overlayController.current();
#ifdef GLOWDECK_WAVESHARE_S3
  if (e.kind == EventOverlay::Kind::GOAL && goalGif.active()) {
    if (goalGif.tick(now)) {
      drawS3GoalGifHud(e, now);
      overlayLastFrame = now;
    } else if (!goalGif.active()) {
      // One corrupt/unsupported GIF must not cause an infinite retry loop.
      gifOverlayFailedStart = overlayController.startedAt();
      needsDraw = true;
    }
    return;
  }
  drawS3EventAnimation(now);
  return;
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

// Bounded labels keep long NHL status strings and pregame records in their
// respective cards. Text overrun can look like another screen bleeding
// through, particularly in the compact 480px comparison/standings layouts.
// Caller sets the desired text datum before invoking this helper.
void drawS3FittedText(const String &value,int16_t x,int16_t y,
                      int16_t maxWidth,uint8_t font,
                      uint16_t fg,uint16_t bg) {
  if(maxWidth<=0)return;
  String label=value;
  if(tft.textWidth(label,font)>maxWidth) {
    const String dots="...";
    while(label.length() && tft.textWidth(label+dots,font)>maxWidth)
      label.remove(label.length()-1);
    if(!label.length())return;
    label+=dots;
  }
  tft.setTextColor(fg,bg);
  tft.drawString(label,x,y,font);
}

void drawS3Nav(const char *label) {
  const S3NativeLayout::Rect prev = S3NativeLayout::kPrev;
  const S3NativeLayout::Rect next = S3NativeLayout::kNext;
  const S3NativeLayout::Rect all = S3NativeLayout::kGames;
  tft.fillRect(0, 271, 480, 49, S3_BG);
  tft.setTextDatum(MC_DATUM);
  tft.fillRoundRect(prev.x,prev.y,prev.w,prev.h,8,S3_NAV);
  tft.fillRoundRect(next.x,next.y,next.w,next.h,8,S3_NAV);
  tft.fillRoundRect(all.x,all.y,all.w,all.h,8,S3_CARD);
  GameDayIcons::draw(tft,GameDayIcons::Kind::PREVIOUS,
      prev.x+prev.w/2,prev.y+prev.h/2,20,WHITE);
  GameDayIcons::draw(tft,GameDayIcons::Kind::NEXT,
      next.x+next.w/2,next.y+next.h/2,20,WHITE);
  if(String(label)=="ALL GAMES" || String(label)=="GAMES")
    GameDayIcons::draw(tft,GameDayIcons::Kind::GAMES,
        all.x+all.w/2,all.y+all.h/2,20,S3_AQUA);
  else {
    tft.setTextColor(S3_AQUA,S3_CARD);
    tft.drawString(label,all.x+all.w/2,all.y+all.h/2,2);
  }
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
#if defined(GLOWDECK_S3_PHASE6_LIVE) && GLOWDECK_S3_PHASE6_LIVE
void drawS3ArenaFitted(const String &value,int16_t x,int16_t y,
                       int16_t maxWidth,uint8_t font,uint16_t color){
  String label=value;
  while(label.length()&&tft.textWidth(label,font)>maxWidth)
    label.remove(label.length()-1);
  if(!label.length())return;
  tft.setTextColor(color); // transparent glyphs over the photograph
  tft.drawString(label,x,y,font);
}
// Phase 6B Arena Broadcast: the asset is a static arena template,
// while logos, numbers, clock, PP and touch targets remain live GameDay data.
// The transparent NHL logo pixels are composited into TFT_eSprite.
// Team-linked status accents stay in the 136px centre of the lower strip.
// They use official team primary colors, not an ambiguous generic PP color.
void drawS3ArenaStatusLine(const Game &g,const String &label,
                           int16_t y,uint8_t sides,uint16_t labelColor){
  if(S3Broadcast::hasAway(sides))
    tft.fillRoundRect(175,y-3,11,5,2,to565(teamRgb(g.away)));
  if(S3Broadcast::hasHome(sides))
    tft.fillRoundRect(294,y-3,11,5,2,to565(teamRgb(g.home)));
  drawS3ArenaFitted(label,240,y,100,2,labelColor);
}

// Phase 6B.2B: one state-aware readout renderer for full and partial frames.
// All dynamic pixels stay inside the two dirty rectangles. They are restored
// from the cached RGB565 image before drawing, so FUT->LIVE->INT->FINAL never
// leaks stale VS, PP/EN, clocks or shots into another broadcast state.
// Track only the data that appears in each of the two existing LCD regions.
// All fields are hashed in place; this is no additional canvas or heap copy.
S3BroadcastFrame::Snapshot captureS3ArenaFrame(const Game &g,uint32_t now){
  S3BroadcastFrame::Snapshot frame;
  frame.valid=true;
  frame.gameId=g.id;
  uint32_t top=S3BroadcastFrame::kFNVOffset;
  S3BroadcastFrame::mixText(top,g.state.c_str());
  S3BroadcastFrame::mixText(top,g.startUTC.c_str());
  S3BroadcastFrame::mixText(top,g.clock.c_str());
  S3BroadcastFrame::mixText(top,g.periodType.c_str());
  S3BroadcastFrame::mixNumber(top,g.period);
  S3BroadcastFrame::mixNumber(top,g.intermission);
  S3BroadcastFrame::mixNumber(top,uint32_t(g.awayScore));
  S3BroadcastFrame::mixNumber(top,uint32_t(g.homeScore));
  frame.top=top;

  uint32_t bottom=S3BroadcastFrame::kFNVOffset;
  const S3Broadcast::Phase phase=S3Broadcast::classify(
      g.state.c_str(),g.intermission);
  const S3Broadcast::SpecialTeams special=S3Broadcast::specialTeams(
      phase,g.ppDataKnown,g.ppSide,g.ppUpdatedAt,
      g.emptyNetKnown,g.emptyNetSide,g.emptyNetUpdatedAt,now);
  S3BroadcastFrame::mixNumber(bottom,uint32_t(phase));
  S3BroadcastFrame::mixNumber(bottom,uint32_t(g.ppPhase));
  S3BroadcastFrame::mixNumber(bottom,uint32_t(g.awayShots));
  S3BroadcastFrame::mixNumber(bottom,uint32_t(g.homeShots));
  S3BroadcastFrame::mixNumber(bottom,special.signature());
  if(phase==S3Broadcast::Phase::UPCOMING){
    // Pregame records can arrive via the existing stats prefetch without
    // a new NHL scoreboard. Only records for this game may influence the key.
    for(uint8_t i=0;i<gameCount;++i){
      if(games[i].id==g.id && carouselStats[i].id==g.id &&
         carouselStats[i].loaded){
        S3BroadcastFrame::mixText(bottom,carouselStats[i].recordAway.c_str());
        S3BroadcastFrame::mixText(bottom,carouselStats[i].recordHome.c_str());
        break;
      }
    }
  }
  frame.bottom=bottom;
  return frame;
}
void drawS3ArenaReadout(const Game &g,uint8_t mask=S3BroadcastFrame::BOTH) {
  if(!s3Arena.ready())return;
  const auto top=S3NativeLayout::kBroadcastReadoutTop;
  const auto bottom=S3NativeLayout::kBroadcastReadoutBottom;
  if(mask&S3BroadcastFrame::TOP)
    s3Arena.restoreRegion(tft,top.x,top.y,top.w,top.h);
  if(mask&S3BroadcastFrame::BOTTOM)
    s3Arena.restoreRegion(tft,bottom.x,bottom.y,bottom.w,bottom.h);

  const S3Broadcast::View view=S3Broadcast::resolve(
      g.state.c_str(),g.intermission,g.awayScore,g.homeScore);
  // Bottom-only refreshes must not inherit TL_DATUM from the last frame.
  // Set deterministic font state for either independent dirty rectangle.
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(1);
  if(mask&S3BroadcastFrame::TOP){
  String headline;
  String badge;
  uint16_t badgeColor=S3_AQUA;
  switch(view.phase) {
    case S3Broadcast::Phase::UPCOMING:
      headline=formattedStart(g.startUTC);
      badge="UPCOMING";
      break;
    case S3Broadcast::Phase::LIVE:
      headline=broadcastTime(g);
      badge="LIVE";
      badgeColor=0xF9C3;
      break;
    case S3Broadcast::Phase::INTERMISSION:
      headline="END OF "+String(S3Broadcast::periodName(g.period,
                                                       g.periodType.c_str()));
      badge="INTERMISSION";
      break;
    case S3Broadcast::Phase::FINAL:
      headline="GAME COMPLETE";
      badge=S3Broadcast::finalStatus(g.periodType.c_str(),
                                    g.state.c_str());
      badgeColor=0xC618;
      break;
    default:
      headline=gameStatus(g);
      badge="GAME STATUS";
      break;
  }

  tft.setTextDatum(MC_DATUM);
  // Pick the font by rendered pixel width; long period/status strings are
  // never silently clipped on the physical 480x320 broadcast panel.
  const uint8_t headlineFont=tft.textWidth(headline,4)>170?2:4;
  drawS3ArenaFitted(headline,240,68,170,headlineFont,WHITE);
  if(view.phase==S3Broadcast::Phase::LIVE)
    tft.fillCircle(171,92,3,0xF124);
  drawS3ArenaFitted(badge,240,92,168,
                    badge.length()>14?1:2,badgeColor);

  if(view.showScore) {
    const bool wide=g.awayScore>9 || g.homeScore>9;
    const int savedScale=wide?1:2;
    tft.setTextColor(WHITE);
    tft.setTextSize(savedScale);
    tft.drawString(String(g.awayScore),204,147,4);
    tft.drawString(String(g.homeScore),276,147,4);
    tft.setTextSize(1);
    tft.setTextColor(S3_MUTED);
    tft.drawString("-",240,149,4);
    // A final winner is a result, never a guess during a live/intermission
    // game. Distinguish SO/OT in the badge and mark the winner discreetly.
    if(view.phase==S3Broadcast::Phase::FINAL && view.winnerSide) {
      const int lineX=view.winnerSide==2?182:260;
      const uint16_t winnerColor=to565(teamRgb(
          view.winnerSide==2?g.away:g.home));
      tft.fillRoundRect(lineX,185,38,3,1,winnerColor);
    }
  } else {
    tft.setTextColor(WHITE);
    tft.drawString(view.phase==S3Broadcast::Phase::UPCOMING?"VS":"NHL",
                   240,147,4);
  }
  } // TOP only: clock, header and score

  if(mask&S3BroadcastFrame::BOTTOM){
  if(view.showSeasonRecord) {
    // Pregame never displays meaningless SOG/PP "--" placeholders. Reuse
    // the existing per-game NHL season-record cache, with no new requests.
    const CarouselStats *cached=nullptr;
    for(uint8_t i=0;i<gameCount;++i) {
      if(games[i].id==g.id && carouselStats[i].id==g.id &&
         carouselStats[i].loaded) {
        cached=&carouselStats[i];
        break;
      }
    }
    tft.setTextColor(S3_MUTED);
    tft.drawString("SEASON W-L-OT",240,229,2);
    const bool hasAway=cached && cached->recordAway.length() &&
                       cached->recordAway!="--";
    const bool hasHome=cached && cached->recordHome.length() &&
                       cached->recordHome!="--";
    if(hasAway || hasHome) {
      drawS3ArenaFitted(hasAway?cached->recordAway:String("N/A"),
                        115,252,95,2,WHITE);
      drawS3ArenaFitted(hasHome?cached->recordHome:String("N/A"),
                        365,252,95,2,WHITE);
    } else {
      tft.setTextColor(S3_MUTED);
      tft.drawString("RECORDS PENDING",240,254,1);
    }
  } else if(view.showShots) {
    // Stronger SOG hierarchy; a missing NHL number stays muted ("--"),
    // never silently becomes a fabricated zero.
    tft.setTextColor(S3_MUTED);
    tft.drawString("SOG",115,229,2);
    tft.drawString("SOG",365,229,2);
    tft.setTextColor(g.awayShots>=0?WHITE:S3_MUTED);
    tft.drawString(g.awayShots>=0?String(g.awayShots):String("--"),
                   115,251,4);
    tft.setTextColor(g.homeShots>=0?WHITE:S3_MUTED);
    tft.drawString(g.homeShots>=0?String(g.homeShots):String("--"),
                   365,251,4);

    if(view.showSpecialTeams) {
      // Presentation only: PP proof still comes from the NHL penalty ledger,
      // goalie-pulled states from the NHL on-ice feed. Respect expiry even
      // if Wi-Fi disconnects and the scoreboard stops polling.
      const S3Broadcast::SpecialTeams status=S3Broadcast::specialTeams(
          view.phase,g.ppDataKnown,g.ppSide,g.ppUpdatedAt,
          g.emptyNetKnown,g.emptyNetSide,g.emptyNetUpdatedAt,millis());
      const String ppTeam=status.ppSide==1?g.home:g.away;
      const String enTeam=status.emptyNetSides==3?String("BOTH"):
          status.emptyNetSides==1?g.home:g.away;
      if(status.ppSide && status.emptyNetSides) {
        drawS3ArenaStatusLine(g,"PP "+ppTeam,237,status.ppSide,0xFDA6);
        drawS3ArenaStatusLine(g,"EN "+enTeam,256,status.emptyNetSides,S3_AQUA);
      } else if(status.ppSide) {
        drawS3ArenaStatusLine(g,"POWER PLAY",237,status.ppSide,0xFDA6);
        drawS3ArenaFitted(ppTeam,240,255,100,2,WHITE);
      } else if(status.emptyNetSides) {
        drawS3ArenaStatusLine(g,"EMPTY NET",237,status.emptyNetSides,S3_AQUA);
        drawS3ArenaFitted(enTeam,240,255,100,2,WHITE);
      }
    } else if(view.phase==S3Broadcast::Phase::INTERMISSION) {
      const NhlPowerPlayTracker2::Watch *session=
          powerPlaySession(g.id,false);
      tft.setTextColor(S3_AQUA);
      if(session&&session->side &&
         session->phase==NhlPowerPlayTracker2::Phase::INTERMISSION){
        drawS3ArenaFitted("PP NEXT: "+
            (session->side==1?g.home:g.away),240,245,128,1,0xFDA6);
      }else tft.drawString("INTERMISSION",240,245,1);
    } else if(view.phase==S3Broadcast::Phase::FINAL) {
      if(view.winnerSide) {
        tft.setTextColor(S3_MUTED);
        tft.drawString("WINNER",240,237,1);
        drawS3ArenaFitted(view.winnerSide==2?g.away:g.home,
                          240,254,129,2,WHITE);
      } else {
        tft.setTextColor(S3_MUTED);
        tft.drawString("GAME COMPLETE",240,245,1);
      }
    }
  } else {
    tft.setTextColor(S3_MUTED);
    tft.drawString("AWAITING NHL UPDATE",240,245,1);
  }
  } // BOTTOM only: season records, SOG and verified PP/EN
  if(mask&S3BroadcastFrame::TOP){
  // Entire main scoreboard now opens Stats with a short tap. Keep this
  // low-contrast hint inside the existing top dirty rectangle.
  tft.setTextColor(S3_MUTED);
  tft.drawString("TAP FOR STATS",240,195,1);
  }
  const S3Broadcast::SpecialTeams visibleStatus=S3Broadcast::specialTeams(
      view.phase,g.ppDataKnown,g.ppSide,g.ppUpdatedAt,
      g.emptyNetKnown,g.emptyNetSide,g.emptyNetUpdatedAt,millis());
  s3BroadcastRenderedGameId=g.id;
  s3BroadcastRenderedStatus=visibleStatus.signature();
  s3BroadcastLastFrame=captureS3ArenaFrame(g,millis());
  tft.setTextDatum(TL_DATUM);
}
// A freestanding follow star for the de-boxed Phase 6B.2A broadcast page.
// Unlike the shared CYD/S3 control this does NOT draw a 44x30 card behind it.
// Preserve follow vs armed semantics, position and existing touch geometry.
void drawS3ArenaFollowStar(const Game &g) {
  // Always restore the caller's text datum. Previously only armed games
  // changed it to TL_DATUM after drawing the small "A" label, shifting
  // the GAMES / page / SETUP footer horizontally on those slides.
  const uint8_t previousDatum=tft.getTextDatum();
  constexpr int16_t x=399, y=24;
  const bool followed=rearFollows(g.id);
  const bool armed=isArmedGame(g.id);
  const uint16_t color=followed ? 0xFE60 : 0xCE79;
  const int8_t px[10]={0,3,10,5,6,0,-6,-5,-10,-3};
  const int8_t py[10]={-11,-3,-3,2,10,6,10,2,-3,-3};
  for(uint8_t i=0;i<10;++i) {
    const uint8_t j=(i+1)%10;
    if(followed)
      tft.fillTriangle(x,y,x+px[i],y+py[i],x+px[j],y+py[j],color);
    tft.drawLine(x+px[i],y+py[i],x+px[j],y+py[j],color);
  }
  if(armed) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(S3_AQUA);
    tft.drawString("A"+String(armPriority(g.id)),x+15,y-9,1);
  }
  tft.setTextDatum(previousDatum);
}
void drawS3ArenaTicker(const Game &g) {
  if(!gameIsLive(g))return;
  // Separate 480x20 dirty strip: no arena, logos, score or control redraw.
  // Y=269..288 remains above the lowest icon pixels (starts at ~291).
  tft.fillRect(0,269,480,20,0x1083);
  tft.drawFastHLine(0,269,480,0x2987);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(WHITE);
  tft.drawString(liveTickerWindow(425,2),
                 28-int16_t(liveTickerPixelOffset),279,2);
  // Clip only the sides of this 20px strip; never touch score or logos.
  tft.fillRect(0,270,28,19,0x1083);
  tft.fillRect(465,270,15,19,0x1083);
  tft.fillRect(0,270,3,18,to565(teamRgb(g.away)));
  tft.fillRect(477,270,3,18,to565(teamRgb(g.home)));
  tft.setTextDatum(TL_DATUM);
}

void drawS3ArenaScoreboard(const Game &g) {
  if(!s3Arena.restore(tft))return;
  const uint16_t away=to565(teamRgb(g.away));
  const uint16_t home=to565(teamRgb(g.home));
  // Larger, native-resolution 96px NHL artwork (previously 80px).
  // Center each logo on the softly shaded arena plinth at (86,144)
  // and (394,144), without encroaching on the center score or labels.
  drawTeamLogo(tft,g.away,38,96,96);
  drawTeamLogo(tft,g.home,346,96,96);
  // Team-primary accents stay crisp, above the artwork's lower edge.
  tft.fillRoundRect(36,189,105,3,1,away);
  tft.fillRoundRect(339,189,105,3,1,home);
  tft.setTextDatum(MC_DATUM);
  drawS3ArenaFitted(g.away,88,204,100,4,WHITE);
  drawS3ArenaFitted(g.home,392,204,100,4,WHITE);
  // Frameless follow/arm indicator, original touch target unchanged.
  drawS3ArenaFollowStar(g);
  // Five native touch icons: games, previous, pause/play, next, setup.
  // No logo, scoreboard or shot-stat region is covered by the controls.
  GameDayIcons::draw(tft,GameDayIcons::Kind::GAMES,37,300,18,S3_MUTED);
  GameDayIcons::draw(tft,GameDayIcons::Kind::PREVIOUS,120,300,20,S3_MUTED);
  GameDayIcons::draw(tft,carouselHold.paused?GameDayIcons::Kind::PLAY:
      GameDayIcons::Kind::PAUSE,240,301,20,
      carouselHold.paused?S3_AQUA:WHITE);
  GameDayIcons::draw(tft,GameDayIcons::Kind::NEXT,360,300,20,S3_MUTED);
  GameDayIcons::draw(tft,GameDayIcons::Kind::SETUP,443,300,20,S3_MUTED);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(carouselHold.paused?S3_AQUA:S3_MUTED);
  tft.drawString(carouselHold.paused?"HELD":String(carouselSlide+1)+"/"+
                 String(carouselPageCount()),240,279,1);
  tft.setTextDatum(TL_DATUM);
  drawS3ArenaReadout(g);
  drawS3ArenaTicker(g);
}
#endif

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
  // 80px official NHL SVG rasters, centered under each team's label.
  // Sprite overload writes to the native framebuffer, not directly to ST7796.
  drawTeamLogo(tft,g.away,57,96,80);
  drawTeamLogo(tft,g.home,343,96,80);
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
  // Small logos on every summary and All Games row, sourced from the
  // same JSON/CDN assets as the main scoreboard (no HTTP in the draw loop).
  drawTeamLogo(tft,g.away,24,r.y+8,30);
  drawTeamLogo(tft,g.home,369,r.y+8,30);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(WHITE,surface);
  tft.drawString(g.away,103,r.y+17,4);
  tft.drawString(g.home,341,r.y+17,4);
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
  if(isArmedGame(g.id))status="A"+String(armPriority(g.id))+" / "+status;
  if(status.length()>34)status=status.substring(0,34);
  tft.drawString(status,213,r.y+38,1);
  const uint8_t rank=armPriority(g.id);
  const uint16_t pill=rank?S3_AQUA:S3_NAV;
  tft.fillRoundRect(301,r.y+29,58,19,4,pill);
  tft.setTextColor(rank?S3_BG:WHITE,pill);
  tft.drawString(rank?"A"+String(rank):
      armedGames.count()>=MultiGameArming::kMaxArmed?"FULL":"ARM",
      330,r.y+38,1);
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
  GameDayIcons::draw(tft,GameDayIcons::Kind::SETUP,
      setup.x+setup.w/2,setup.y+setup.h/2,20,WHITE);
  tft.setTextColor(armed?S3_BG:S3_AQUA,armed?S3_AQUA:S3_CARD);
  tft.drawString(armed?"DISARM":
                 armedGames.count()>=MultiGameArming::kMaxArmed?"MAX 5":"ARM GOALS",
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
  // Fit inside the native 76px-high stats header without being clipped.
  drawTeamLogo(tft,g->away,37,62,60);
  drawTeamLogo(tft,g->home,387,62,60);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(WHITE,S3_CARD);
  drawS3FittedText(g->away,136,86,60,4,WHITE,S3_CARD);
  drawS3FittedText(g->home,344,86,60,4,WHITE,S3_CARD);
  const String score=gameIsFuture(*g)?"VS":
      String(g->awayScore)+" - "+String(g->homeScore);
  drawS3FittedText(score,240,77,125,4,WHITE,S3_CARD);
  tft.setTextColor(S3_AQUA,S3_CARD);
  String state=gameStatus(*g);
  if(state.length()>30)state=state.substring(0,30);
  drawS3FittedText(state,240,113,230,2,S3_AQUA,S3_CARD);
  const bool haveStats=detail.loaded && detail.gameId==g->id && detail.count>0;
  if(haveStats) {
    for(uint8_t row=0;row<detail.count && row<5;++row) {
      const int16_t y=134+row*27;
      const uint16_t plate=(row&1)?S3_DARK:S3_CARD;
      tft.fillRoundRect(12,y,456,25,5,plate);
      tft.setTextColor(WHITE,plate);
      drawS3FittedText(detail.away[row],96,y+11,105,2,WHITE,plate);
      drawS3FittedText(detail.home[row],384,y+11,105,2,WHITE,plate);
      drawS3FittedText(detail.label[row],240,y+10,140,2,S3_MUTED,plate);
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
  drawS3StatsActions(isArmedGame(g->id));
}
void drawS3Standings() {
  const StandingsCarousel::Page page=StandingsCarousel::page(
      currentCarouselPage().index);
  tft.fillScreen(S3_BG);
  drawS3Header(nullptr,"NHL STANDINGS");
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(WHITE,S3_BG);
  drawS3FittedText(page.title,17,65,310,2,WHITE,S3_BG);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(S3_AQUA,S3_BG);
  drawS3FittedText(page.range,464,65,120,2,S3_AQUA,S3_BG);
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
      tft.fillRect(65,y+3,4,13,to565(teamRgb(entry.team)));
      drawTeamLogo(tft,entry.team,75,y+1,17);
      tft.setTextColor(WHITE,plate);
      tft.drawString(String(StandingsCarousel::rank(
          page.group,entry.conferenceSequence,entry.divisionSequence)),
          37,y+10,2);
      tft.drawString(entry.team,130,y+10,2);
      tft.setTextColor(S3_MUTED,plate);
      tft.drawString(String(entry.gamesPlayed),217,y+10,2);
      drawS3FittedText(entry.seasonRecord,331,y+10,107,2,S3_MUTED,plate);
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
// --- Phase 4: native 480x320 celebrations and device surfaces ----------------
void drawS3OverlayDismiss() {
  const S3NativeLayout::Rect r=S3NativeLayout::kOverlayClose;
  tft.fillRoundRect(r.x,r.y,r.w,r.h,8,S3_CARD);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(WHITE,S3_CARD);
  tft.drawString("X",r.x+r.w/2,r.y+r.h/2,4);
  tft.setTextDatum(TL_DATUM);
}
String s3OverlayScore(const EventOverlay::Item &e) {
  if(!e.away[0] || !e.home[0])return String("NHL GAME EVENT");
  return String(e.away)+"  "+String(e.awayScore)+"  -  "+
         String(e.homeScore)+"  "+String(e.home);
}
String s3OverlaySubtitle(const EventOverlay::Item &e) {
  switch(e.kind) {
    case EventOverlay::Kind::GOAL:
      return String(e.team)+" SCORES!";
    case EventOverlay::Kind::SHOOTOUT_START:return "SHOOTOUT BEGINS";
    case EventOverlay::Kind::SHOOTOUT_GOAL:return String(e.team)+" SCORES!";
    case EventOverlay::Kind::SHOOTOUT_SAVE:return String(e.team)+" MAKES THE SAVE";
    case EventOverlay::Kind::SHOOTOUT_MISS:return String(e.team)+" SHOT MISSED";
    case EventOverlay::Kind::POWER_PLAY_START:
      return String(e.team)+" HAS THE ADVANTAGE";
    case EventOverlay::Kind::POWER_PLAY_END:
      return String(e.team)+" BACK AT FULL STRENGTH";
    case EventOverlay::Kind::GAME_START:
      return "THE PUCK HAS DROPPED";
    case EventOverlay::Kind::FINAL_WIN:
      return String(e.team)+" WINS";
    case EventOverlay::Kind::FINAL_LOSS:
      return String(e.team)+" FALLS";
    default: return "";
  }
}
void drawS3EventStatic() {
  if(!overlayController.active())return;
  const EventOverlay::Item &e=overlayController.current();
  const uint32_t rgb=e.color?e.color:0x31837AUL;
  const uint16_t club=TeamGradient::rgb565(rgb);
  tft.fillScreen(S3_BG);
  for(int x=0;x<480;x+=8) {
    const uint16_t sample=uint16_t(uint32_t(x+4)*319UL/479UL);
    const uint16_t c=TeamGradient::rgb565(
         TeamGradient::colorAt(rgb,rgb,sample));
    tft.fillRect(x,44,8,231,c);
  }
  tft.fillRect(0,0,480,44,S3_NAV);
  tft.fillRoundRect(15,54,450,211,13,S3_CARD);
  tft.drawRoundRect(15,54,450,211,13,S3_LINE);
  tft.fillRect(19,64,6,192,club);
  tft.fillRect(455,64,6,192,club);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(S3_MUTED,S3_NAV);
  tft.drawString("GLOWDECK  /  NHL EVENT",15,22,2);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(WHITE,S3_CARD);
  if(cfg.overlayLogos && e.team[0]) {
    drawTeamLogo(tft,String(e.team),45,91,72);
  } else {
    tft.drawString(e.team[0]?String(e.team):String("NHL"),73,120,4);
  }
  String headline=EventOverlay::label(e.kind);
  tft.drawString(headline,280,97,headline.length()>12?2:4);
  tft.setTextColor(0xD6FA,S3_CARD);
  tft.drawString(s3OverlaySubtitle(e),280,134,2);
  tft.drawFastHLine(43,165,394,S3_LINE);
  tft.setTextColor(WHITE,S3_CARD);
  tft.drawString(s3OverlayScore(e),240,197,4);
  String period=e.period?String("P")+String(e.period):String("");
  if(e.clock[0]) period+=(period.length()?"  |  ":"")+String(e.clock);
  tft.setTextColor(S3_AQUA,S3_CARD);
  tft.drawString(period.length()?period:String("LIVE NHL"),240,238,2);
  tft.setTextDatum(TL_DATUM);
  drawS3OverlayDismiss();
}
void drawS3EventAnimation(uint32_t now) {
  if(!overlayController.active())return;
  const EventOverlay::Item &e=overlayController.current();
  const uint32_t elapsed=now-overlayController.startedAt();
  uint8_t brightness=80;
  if(EventOverlay::isFlash(e.kind)) {
    brightness=(cfg.overlayFlash && elapsed<1200UL &&
        (elapsed/180UL)%2UL)?38:95;
  } else if(EventOverlay::isPulse(e.kind)) {
    const uint32_t phase=elapsed%1600UL;
    const uint32_t wave=phase<800UL?phase:1600UL-phase;
    brightness=uint8_t(35UL+wave*65UL/800UL);
  } else {
    const uint32_t phase=elapsed%2200UL;
    const uint32_t wave=phase<1100UL?phase:2200UL-phase;
    brightness=uint8_t(52UL+wave*36UL/1100UL);
  }
  const uint32_t rgb=e.color?e.color:0x31837AUL;
  const uint32_t r=TeamGradient::scale((rgb>>16)&255U,brightness);
  const uint32_t g=TeamGradient::scale((rgb>>8)&255U,brightness);
  const uint32_t b=TeamGradient::scale(rgb&255U,brightness);
  const uint16_t c=TeamGradient::rgb565((r<<16)|(g<<8)|b);
  // Animate only the perimeter and progress; preserve static logo and copy.
  tft.fillRect(0,0,6,320,c);
  tft.fillRect(474,0,6,320,c);
  tft.fillRect(0,0,480,4,c);
  tft.fillRect(0,316,480,4,c);
  const uint32_t remain=overlayController.remaining(now);
  const uint16_t progress=e.duration?
       uint16_t(456UL*remain/e.duration):0;
  tft.fillRoundRect(12,285,456,16,6,S3_NAV);
  if(progress)tft.fillRoundRect(12,285,progress,16,6,c);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(WHITE,S3_BG);
  tft.drawString(String((remain+999)/1000)+"s",240,309,1);
  tft.setTextDatum(TL_DATUM);
  overlayLastFrame=now;
}
void drawS3GifBackdrop(const EventOverlay::Item &e) {
  const uint16_t club=TeamGradient::rgb565(e.color?e.color:0x31837AUL);
  tft.fillScreen(S3_BG);
  tft.fillRect(0,0,480,4,club);
  tft.fillRect(0,316,480,4,club);
  tft.fillRect(0,40,8,240,club);
  tft.fillRect(472,40,8,240,club);
  tft.fillRect(0,4,480,36,S3_NAV);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(WHITE,S3_NAV);
  tft.drawString(String(e.team)+"  GOAL!",16,21,4);
  tft.setTextDatum(TL_DATUM);
  if(cfg.overlayLogos && e.team[0])
    drawTeamLogo(tft,String(e.team),12,129,56);
  // Native 320x240 GIF viewport starts at 80,40. No portion is stretched.
  tft.fillRect(S3NativeLayout::kGifViewport.x,
               S3NativeLayout::kGifViewport.y,
               S3NativeLayout::kGifViewport.w,
               S3NativeLayout::kGifViewport.h,TFT_BLACK);
  tft.fillRect(0,281,480,35,S3_NAV);
  drawS3OverlayDismiss();
}
void drawS3GoalGifHud(const EventOverlay::Item &e,uint32_t now) {
  const uint16_t club=TeamGradient::rgb565(e.color?e.color:0x31837AUL);
  const uint32_t remain=overlayController.remaining(now);
  tft.fillRect(0,281,480,35,S3_NAV);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(WHITE,S3_NAV);
  tft.drawString(s3OverlayScore(e),235,294,2);
  tft.setTextDatum(TL_DATUM);
  tft.fillRect(12,310,456,4,S3_LINE);
  const uint16_t width=e.duration?uint16_t(456UL*remain/e.duration):0;
  if(width)tft.fillRect(12,310,width,4,club);
  const uint32_t elapsed=now-overlayController.startedAt();
  const uint8_t light=((elapsed/180UL)%2UL)?55:100;
  const uint16_t pulse=TeamGradient::rgb565(
      (uint32_t(TeamGradient::scale((e.color>>16)&255U,light))<<16)|
      (uint32_t(TeamGradient::scale((e.color>>8)&255U,light))<<8)|
      uint32_t(TeamGradient::scale(e.color&255U,light)));
  tft.fillRect(0,40,7,240,pulse);
  tft.fillRect(473,40,7,240,pulse);
  overlayLastFrame=now;
}
void drawS3SetupNav() {
  const auto l=S3NativeLayout::kPrev;
  const auto m=S3NativeLayout::kGames;
  const auto r=S3NativeLayout::kNext;
  tft.fillRect(0,271,480,49,S3_BG);
  tft.fillRoundRect(l.x,l.y,l.w,l.h,8,S3_NAV);
  tft.fillRoundRect(m.x,m.y,m.w,m.h,8,S3_CARD);
  tft.fillRoundRect(r.x,r.y,r.w,r.h,8,S3_NAV);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(WHITE,S3_NAV);
  tft.drawString("TODAY",l.x+l.w/2,l.y+l.h/2,2);
  tft.drawString("BACK",r.x+r.w/2,r.y+r.h/2,2);
  tft.setTextColor(S3_AQUA,S3_CARD);
  GameDayIcons::draw(tft,GameDayIcons::Kind::GAMES,
      m.x+m.w/2,m.y+m.h/2,20,S3_AQUA);
  tft.setTextDatum(TL_DATUM);
}
void drawS3Setup() {
  tft.fillScreen(S3_BG);
  drawS3Header(nullptr,"DEVICE SETUP");
  if(WiFi.status()!=WL_CONNECTED){
    tft.fillRoundRect(12,56,456,145,10,S3_CARD);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(S3_AQUA,S3_CARD);
    tft.drawString("CONNECT TO WI-FI",25,78,2);
    tft.setTextColor(WHITE,S3_CARD);
    tft.drawString("Scan QR with your phone",25,106,2);
    tft.drawString(apSsid,25,132,2);
    tft.setTextColor(S3_MUTED,S3_CARD);
    tft.drawString("Then open 192.168.4.1",25,159,2);
    tft.drawString("Or set up Wi-Fi by USB",25,183,2);
    tft.setTextDatum(TL_DATUM);
    drawHotspotJoinQr(337,73,3);
  } else {
  tft.fillRoundRect(12,56,456,145,10,S3_CARD);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(S3_AQUA,S3_CARD);
  tft.drawString("NETWORK",25,74,2);
  tft.setTextColor(WHITE,S3_CARD);
  String ssid=cfg.ssid.isEmpty()?String("Not configured"):cfg.ssid;
  if(ssid.length()>29)ssid=ssid.substring(0,29)+"...";
  tft.drawString("Wi-Fi: "+ssid,25,102,2);
  String ip=localAddress();
  if(ip.length()>32)ip=ip.substring(0,32);
  tft.drawString("IP: "+ip,25,124,2);
  tft.setTextColor(S3_AQUA,S3_CARD);
  tft.drawString("GLOWDECK HOTSPOT",25,151,2);
  tft.setTextColor(WHITE,S3_CARD);
  tft.drawString("AP: "+apSsid,25,174,2);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(S3_MUTED,S3_CARD);
  tft.drawString("KEY: "+apPassword,456,174,2);
  }
  tft.setTextDatum(MC_DATUM);
  const S3NativeLayout::Rect b[3]={
    S3NativeLayout::kSetupTest,
    S3NativeLayout::kSetupTouchCheck,
    S3NativeLayout::kSetupColorMode};
  const char* label[3]={"TEST LIGHTS","TOUCH CHECK","COLORS"};
  for(uint8_t i=0;i<3;++i) {
    tft.fillRoundRect(b[i].x,b[i].y,b[i].w,b[i].h,9,S3_CARD);
    tft.setTextColor(i==2?S3_AQUA:WHITE,S3_CARD);
    tft.drawString(label[i],b[i].x+b[i].w/2,b[i].y+b[i].h/2,2);
  }
  tft.setTextDatum(TL_DATUM);
  drawS3SetupNav();
}
void drawS3TouchCheck() {
  tft.fillScreen(S3_BG);
  drawS3Header(nullptr,"TOUCH CHECK");
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(S3_MUTED,S3_BG);
  tft.drawString("Tap all four targets to check alignment",240,57,2);
  const S3NativeLayout::Rect r[4]={
    S3NativeLayout::kTouchCheckTopLeft,
    S3NativeLayout::kTouchCheckTopRight,
    S3NativeLayout::kTouchCheckBottomLeft,
    S3NativeLayout::kTouchCheckBottomRight};
  for(uint8_t i=0;i<4;++i) {
    const bool detected=s3TouchCheckMask&(1U<<i);
    const uint16_t bg=detected?S3_AQUA:S3_CARD;
    tft.fillRoundRect(r[i].x,r[i].y,r[i].w,r[i].h,12,bg);
    tft.setTextColor(detected?S3_BG:WHITE,bg);
    tft.drawString(detected?"OK":"+",r[i].x+r[i].w/2,
                   r[i].y+r[i].h/2,4);
  }
  tft.setTextColor(s3TouchCheckMask==15?S3_AQUA:S3_MUTED,S3_BG);
  tft.drawString(s3TouchCheckMask==15?
      "All corners registered correctly":
      "Factory FT6336 mapping - no calibration required",240,142,2);
  if(s3TouchCheckX>=0) {
    tft.setTextColor(WHITE,S3_BG);
    tft.drawString("Last touch: "+
       String(s3TouchCheckX)+", "+String(s3TouchCheckY),240,175,2);
  }
  tft.fillRect(0,271,480,49,S3_BG);
  const auto l=S3NativeLayout::kPrev;
  const auto m=S3NativeLayout::kGames;
  const auto n=S3NativeLayout::kNext;
  tft.fillRoundRect(l.x,l.y,l.w,l.h,8,S3_NAV);
  tft.fillRoundRect(m.x,m.y,m.w,m.h,8,S3_CARD);
  tft.fillRoundRect(n.x,n.y,n.w,n.h,8,S3_NAV);
  tft.setTextColor(WHITE,S3_NAV);
  tft.drawString("BACK",l.x+l.w/2,l.y+l.h/2,2);
  GameDayIcons::draw(tft,GameDayIcons::Kind::GAMES,
      n.x+n.w/2,n.y+n.h/2,20,WHITE);
  tft.setTextColor(S3_AQUA,S3_CARD);
  tft.drawString("RESET",m.x+m.w/2,m.y+m.h/2,2);
  tft.setTextDatum(TL_DATUM);
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
#if defined(GLOWDECK_S3_PHASE6C_UI) && GLOWDECK_S3_PHASE6C_UI
#include "S3BroadcastPages.h"
#endif
#endif


uint32_t newsFollowMask() {
  // Game stars apply to today's matchups; favorite teams persist through
  // the season. Both feed personalized news without arming any lights.
  uint32_t mask=cfg.favoriteTeams;
  for(uint8_t i=0;i<gameCount;++i)if(rearFollows(games[i].id)) {
    const int8_t away=DisplayEventPolicy::teamIndex(games[i].away.c_str());
    const int8_t home=DisplayEventPolicy::teamIndex(games[i].home.c_str());
    if(away>=0)mask|=1UL<<away;
    if(home>=0)mask|=1UL<<home;
  }
  return mask;
}
bool newsAnyLive() {
  // An intermission is still a live game: never replace its scoreboard.
  for(uint8_t i=0;i<gameCount;++i)if(gameIsLive(games[i]))return true;
  return false;
}
bool newsViewEnabled() {
  // News is the PRIMARY idle display, not an optional carousel page.
  // An empty/unreachable feed must show a clear diagnostic placeholder rather
  // than silently resuming upcoming games and conference standings.
  return screen==Screen::LIVE && scoreboardLoaded &&
    !newsAnyLive() && !carouselHold.paused;
}
const NhlNews::Story *visibleNewsStory() {
  if(!newsCache.count)return nullptr;
  return newsCache.ranked(newsSlide%newsCache.count,newsFollowMask());
}
void advanceNews(int direction) {
  if(newsCache.count<2)return;
  const int count=newsCache.count;
  newsSlide=uint8_t((int(newsSlide)+count+(direction<0?-1:1))%count);
  // Leave the ticker untouched: it must keep scrolling between articles.
  lastCarouselAdvance=millis();
  needsDraw=true;
}
void rebuildNewsTicker(){
  String text="  NHL HEADLINES  |  ";
  text.reserve(1500);
  const uint32_t mask=newsFollowMask();
  for(uint8_t i=0;i<newsCache.count;++i){
    const NhlNews::Story *article=newsCache.ranked(i,mask);
    if(!article)break;
    text+=article->headline;
    text+="   |   ";
  }
  if(text!=newsTickerText){
    newsTickerText=text;
    if(newsTickerChar>=newsTickerText.length()){
      newsTickerChar=0;
      newsTickerPixel=0;
    }
  }
}
// UTF-8-safe glyph traversal; byte offsets only move at codepoint boundaries.
// The rendered ticker text and scroll position outlive any card transition.
String newsGlyph(uint16_t offset){
  if(newsTickerText.isEmpty())return String(" ");
  const uint16_t n=newsTickerText.length();
  offset%=n;
  const uint8_t ch=(uint8_t)newsTickerText[offset];
  uint8_t len=ch>=0xF0?4:ch>=0xE0?3:ch>=0xC0?2:1;
  if(offset+len>n)len=1;
  return newsTickerText.substring(offset,offset+len);
}
uint16_t newsNextByte(uint16_t offset){
  if(newsTickerText.isEmpty())return 0;
  const uint16_t n=newsTickerText.length();
  const uint8_t ch=(uint8_t)newsTickerText[offset%n];
  const uint8_t len=ch>=0xF0?4:ch>=0xE0?3:ch>=0xC0?2:1;
  return uint16_t((offset+len)%n);
}
String newsTickerWindow(uint16_t width){
  if(newsTickerText.isEmpty())return String("NHL NEWS");
  String out;out.reserve(110);
  uint16_t at=newsTickerChar;
  int16_t filled=-int16_t(newsTickerPixel);
  for(uint8_t i=0;i<100 && filled<int16_t(width+16);++i){
    const String glyph=newsGlyph(at);
    out+=glyph;
    filled+=tft.textWidth(glyph,2);
    at=newsNextByte(at);
  }
  return out;
}
void tickNewsTicker(uint32_t now){
  if(!newsViewEnabled() || !newsCache.count || overlayController.active() ||
     newsTickerText.isEmpty())return;
  if(uint32_t(now-newsTickerAt)<30UL)return;
  newsTickerAt=now;
  newsTickerPixel++;
  const String glyph=newsGlyph(newsTickerChar);
  const uint8_t glyphWidth=uint8_t(max(1,tft.textWidth(glyph,2)));
  if(newsTickerPixel>=glyphWidth){
    newsTickerPixel=0;
    newsTickerChar=newsNextByte(newsTickerChar);
  }
  newsTickerDirty=true;
}
void tickForgeNews(uint32_t now) {
  // Keep score, physical event output, and restore timing higher priority.
  if(WiFi.status()!=WL_CONNECTED || !scoreboardLoaded || newsAnyLive() ||
     overlayController.active() || rearEvent!=RearEvent::IDLE ||
     wledRestoreWouldConflict(now,8500UL) || now<NhlNews::kMinimumBootMs)
    return;
  const uint32_t follows=newsFollowMask();
  // A failed Forge attempt schedules ESPN for the NEXT loop, rather than
  // blocking the controller for two network timeouts during one iteration.
  const uint32_t interval=newsTryEspnNext ? 0UL :
    (newsCache.failures ? NhlNews::kRetryMs :
     (newsFetchTeamNext && follows ? 30000UL : NhlNews::kRefreshMs));
  if(!newsRefreshRequested && newsLastAttempt &&
     uint32_t(now-newsLastAttempt)<interval)return;
  if(ESP.getFreeHeap()<39000UL){
    if(newsLastError!="News paused: low free heap"){
      newsLastError="News paused: low free heap";
      if(newsViewEnabled() && !newsCache.count)needsDraw=true;
    }
    return;
  }
  newsLastAttempt=millis();
  newsRefreshRequested=false;
  uint8_t teamId=0;
  const bool usingEspn=newsTryEspnNext;
  if(!usingEspn && newsFetchTeamNext && follows){
    for(uint8_t n=0;n<TeamPalette::kCount;++n){
      const uint8_t ix=uint8_t((newsTeamCursor+n)%TeamPalette::kCount);
      if((follows&(1UL<<ix))==0)continue;
      teamId=NhlNews::idFromTeam(TeamPalette::kTeams[ix].abbr);
      newsTeamCursor=uint8_t((ix+1)%TeamPalette::kCount);
      break;
    }
  }
  const auto result=usingEspn ?
      NhlNewsService::fetchEspn(newsCache,follows) :
      NhlNewsService::fetch(newsCache,follows,teamId);
  newsCache.lastHttpCode=result.code;
  if(result.ok && result.accepted){
    newsCache.failures=0;
    newsCache.lastSuccessAt=millis();
    newsTryEspnNext=false;
    newsLastError="";
    NhlNews::copy(newsCache.source,sizeof(newsCache.source),
                  usingEspn?"ESPN":"NHL Forge");
    // After fallback, attempt NHL Forge again on the next regular cycle.
    newsFetchTeamNext=usingEspn?false:!newsFetchTeamNext;
    if(newsSlide>=newsCache.count)newsSlide=0;
    rebuildNewsTicker();
    if(newsViewEnabled())needsDraw=true;
    Serial.printf("[NEWS] %s ok team=%u received=%u cached=%u heap=%u\n",
      newsCache.source,teamId,result.accepted,newsCache.count,
      (unsigned)ESP.getFreeHeap());
  }else if(!usingEspn){
    newsTryEspnNext=true;
    newsLastError=String("NHL Forge ")+result.error+
      " (HTTP "+String(result.code)+"); ESPN next";
    Serial.printf("[NEWS] %s\n",newsLastError.c_str());
    if(newsViewEnabled() && !newsCache.count)needsDraw=true;
  }else{
    newsTryEspnNext=false;
    if(newsCache.failures<255)++newsCache.failures;
    newsLastError=String("ESPN ")+result.error+
      " (HTTP "+String(result.code)+"); retry scheduled";
    Serial.printf("[NEWS] %s\n",newsLastError.c_str());
    if(newsViewEnabled() && !newsCache.count)needsDraw=true;
  }
}
// News typography is independent of NHL game panes and only runs on a card
// transition, not on every ticker tick.
void newsWrap(const char *raw,int16_t x,int16_t y,int16_t width,
              uint8_t font,uint8_t maxLines,uint8_t lineHeight,
              uint16_t color,uint16_t background) {
  String input(raw?raw:"");
  input.replace("\n"," ");
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(color,background);
  int at=0;
  for(uint8_t row=0;row<maxLines && at<int(input.length());++row){
    String line;
    while(at<int(input.length()) && input[at]==' ')++at;
    while(at<int(input.length())){
      int end=at;
      while(end<int(input.length()) && input[end]!=' ')++end;
      const String word=input.substring(at,end);
      const String probe=line.length()?line+" "+word:word;
      if(line.length() && tft.textWidth(probe,font)>width)break;
      line=probe;
      at=end;
      while(at<int(input.length()) && input[at]==' ')++at;
      if(tft.textWidth(line,font)>width)break;
    }
    if(!line.length())break;
    // Very long single words are bounded so they cannot overflow a TFT.
    while(line.length()>1 && tft.textWidth(line,font)>width)
      line.remove(line.length()-1);
    tft.drawString(line,x,y+int(row)*lineHeight,font);
  }
}
// JPEG decode writes directly into the original CYD panel or existing S3
// 480x320 sprite. No second framebuffer, PNG inflater or large heap array.
int16_t newsImageX0=0,newsImageY0=0,newsImageX1=0,newsImageY1=0;
bool newsJpegBlock(int16_t x,int16_t y,uint16_t w,uint16_t h,uint16_t *pixels) {
  if(!pixels || x>=newsImageX1 || y>=newsImageY1)return false;
  if(x>=newsImageX0 && y>=newsImageY0 &&
     x+w<=newsImageX1 && y+h<=newsImageY1){
    tft.pushImage(x,y,w,h,pixels);
    return true;
  }
  // Only JPEG MCU blocks touching the right/bottom edge need clipping.
  for(uint16_t row=0;row<h;++row){
    const int16_t yy=y+row;
    if(yy<newsImageY0 || yy>=newsImageY1)continue;
    for(uint16_t col=0;col<w;++col){
      const int16_t xx=x+col;
      if(xx>=newsImageX0 && xx<newsImageX1)
        tft.drawPixel(xx,yy,pixels[row*w+col]);
    }
  }
  return true;
}
void drawNewsTicker() {
  if(!newsCache.count)return;
#ifdef GLOWDECK_WAVESHARE_S3
  constexpr int w=480,y=286,h=24;
#else
  constexpr int w=320,y=215,h=23;
#endif
  tft.fillRect(0,y,w,h,ARENA_PLATE);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(WHITE,ARENA_PLATE);
  tft.drawString(newsTickerWindow(w),-int16_t(newsTickerPixel),y+4,2);
}
void tickNewsPhoto(uint32_t now) {
  if(!newsViewEnabled() || !newsCache.count || !newsPhotos.ready() ||
     overlayController.active() || rearEvent!=RearEvent::IDLE ||
     WiFi.status()!=WL_CONNECTED || wledRestoreWouldConflict(now,5000UL))
    return;
  // Show the text first, then fill the optional JPEG asynchronously in a
  // later main-loop iteration. Never download in draw()/touch callbacks.
  if(uint32_t(now-lastCarouselAdvance)<700UL ||
     uint32_t(now-lastCarouselAdvance)>NhlNews::kCardMs-1700UL ||
     uint32_t(now-newsPhotoLastCheck)<700UL)return;
  const NhlNews::Story *story=visibleNewsStory();
  if(!story || !story->image[0] || newsPhotos.has(story->image,now))
    return;
  newsPhotoLastCheck=now;
  if(newsPhotos.fetch(story->image,now)){
    // Only redraw if still the same visible article; real game state and
    // queued overlays are checked again at the start of the next loop.
    if(newsViewEnabled() && visibleNewsStory()==story)needsDraw=true;
  }
}
void drawNewsCard() {
  const NhlNews::Story *story=visibleNewsStory();
#ifdef GLOWDECK_WAVESHARE_S3
  const int w=480, h=320, margin=26;
  const int titleY=76, summaryY=190, titleLines=3;
#else
  const int w=320, h=240, margin=15;
  const int titleY=53, summaryY=147, titleLines=3;
#endif
  tft.fillScreen(ARENA_BLACK);
  tft.fillRect(0,0,w,5,TEAL);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(WHITE,ARENA_BLACK);
  tft.drawString("GLOWDECK  /  NHL NEWS",margin,18,2);
  if(!story){
    tft.setTextColor(TEAL,ARENA_BLACK);
    tft.drawString("UPDATING HEADLINES",margin,titleY,4);
    const String detail=WiFi.status()!=WL_CONNECTED?
      "Offline: waiting for Wi-Fi" :
      newsLastError.length()?newsLastError:String("Retrieving latest NHL stories");
    newsWrap(detail.c_str(),margin,summaryY,w-2*margin,2,3,22,
             MUTED,ARENA_BLACK);
    tft.setTextColor(MUTED,ARENA_BLACK);
    tft.drawString("No live games  |  GAMES menu available",
      margin,h-48,2);
    return;
  }
  const String category=String(story->team[0]?story->team:"LEAGUE")+
      "  /  "+NhlNews::kindLabel(story->kind);
  tft.setTextColor(TEAL,ARENA_BLACK);
  tft.drawString(category,margin,
#ifdef GLOWDECK_WAVESHARE_S3
    48
#else
    36
#endif
    ,2);
  newsWrap(story->headline,margin,titleY,w-2*margin,4,titleLines,
#ifdef GLOWDECK_WAVESHARE_S3
    30
#else
    28
#endif
    ,WHITE,ARENA_BLACK);
  newsWrap(story->summary,margin,summaryY,w-2*margin,2,
#ifdef GLOWDECK_WAVESHARE_S3
    3,22
#else
    3,19
#endif
    ,MUTED,ARENA_BLACK);
  tft.setTextColor(MUTED,ARENA_BLACK);
#ifdef GLOWDECK_WAVESHARE_S3
  tft.drawString(String(story->source)+"  |  Swipe for stories  |  GAMES >",margin,260,2);
#else
  tft.drawString(String(story->source)+"  |  "+String(newsSlide+1)+"/"+String(newsCache.count),
                 margin,203,2);
  tft.drawString("MENU",w-49,18,2);
#endif
  // Do not rewind headline scrolling on article switch or JPEG redraw.
  drawNewsTicker();
}

void draw() {
  needsDraw = false;
#ifdef GLOWDECK_WAVESHARE_S3
  // Phase 4: every S3 screen uses the native 480x320 canvas.
  // The uploaded GIF itself remains 320x240, centered and unscaled.
  tft.setNativeLayout(true);
#endif
#ifndef GLOWDECK_WAVESHARE_S3
  // CYD offline is an onboarding state, not an empty NHL carousel.
  // Enforce this on every redraw as a fallback if another UI route changed
  // screen=LIVE/GAMES while STA was disconnected.
  if(WiFi.status()!=WL_CONNECTED && screen!=Screen::CALIBRATE &&
     (hotspotSetupShown || cfg.ssid.isEmpty())){
    screen=Screen::SETUP;
    hotspotSetupShown=true;
  }
#endif
  if (overlayController.active()) {
#ifdef GLOWDECK_WAVESHARE_S3
    const EventOverlay::Item &e=overlayController.current();
    if(e.kind==EventOverlay::Kind::GOAL && goalGif.available() &&
       gifOverlayFailedStart!=overlayController.startedAt()) {
      if(!goalGif.active() || gifOverlayStart!=overlayController.startedAt()) {
        goalGif.stop();
        gifOverlayStart=overlayController.startedAt();
#if defined(GLOWDECK_S3_PHASE6C_UI) && GLOWDECK_S3_PHASE6C_UI
        drawS3CGifBackdrop(e);
#else
        drawS3GifBackdrop(e);
#endif
        if(!goalGif.start(tft,S3NativeLayout::kGifViewport.x,
                         S3NativeLayout::kGifViewport.y))
          gifOverlayFailedStart=overlayController.startedAt();
      }
      if(goalGif.active()) {
        drawOverlayAnimation(millis());
        return;
      }
    }
    goalGif.stop();
#if defined(GLOWDECK_S3_PHASE6C_UI) && GLOWDECK_S3_PHASE6C_UI
    drawS3CEventStatic();
#else
    drawS3EventStatic();
#endif
    drawOverlayAnimation(millis());
    return;
#else
    drawOverlayStatic();
    drawOverlayAnimation(millis());
    return;
#endif
  }
#ifdef GLOWDECK_WAVESHARE_S3
  if(goalGif.active())goalGif.stop();
#if defined(GLOWDECK_S3_PHASE6C_UI) && GLOWDECK_S3_PHASE6C_UI
  if(screen==Screen::CALIBRATE) { drawS3CTouchCheck(); return; }
  if(screen==Screen::SETUP) { drawS3CSetup(); return; }
#else
  if(screen==Screen::CALIBRATE) { drawS3TouchCheck(); return; }
  if(screen==Screen::SETUP) { drawS3Setup(); return; }
#endif
#else
  if (screen == Screen::CALIBRATE) { drawCalibration(); return; }
#endif
  if (newsViewEnabled()) { drawNewsCard(); return; }
  if (screen == Screen::LIVE) {
#ifdef GLOWDECK_WAVESHARE_S3
    if (tft.nativeLayout()) {
#if defined(GLOWDECK_S3_PHASE6C_UI) && GLOWDECK_S3_PHASE6C_UI
      if (showingStandingsPage()) drawS3CStandings();
      else if (showingSummaryPage()) drawS3CGameList(false);
      else if (!gameCount) drawS3CEmpty();
#else
      if (showingStandingsPage()) drawS3Standings();
      else if (showingSummaryPage()) drawS3Summary();
      else if (!gameCount) drawS3Empty();
#endif
      else {
        const LiveCarousel::Page page=currentCarouselPage();
        const Game &g=games[page.kind==LiveCarousel::Kind::GAME
                            ? page.index : featuredIndex % gameCount];
#if defined(GLOWDECK_S3_PHASE6_LIVE) && GLOWDECK_S3_PHASE6_LIVE
        if(s3Arena.begin()) drawS3ArenaScoreboard(g);
        else drawS3Scoreboard(g);
#else
        drawS3Scoreboard(g);
#endif
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
#if defined(GLOWDECK_S3_PHASE6C_UI) && GLOWDECK_S3_PHASE6C_UI
    if (screen == Screen::GAMES) drawS3CGameList(true);
    else if (screen == Screen::STATS) drawS3CStats();
#else
    if (screen == Screen::GAMES) drawS3GamePicker();
    else if (screen == Screen::STATS) drawS3Stats();
#endif
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
             g.home+" "+String(g.homeScore),isArmedGame(g.id));
      const uint8_t rank=armPriority(g.id);
      const uint16_t plate=rank?TEAL:PANEL;
      tft.fillRoundRect(214,y+7,46,22,4,plate);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(WHITE,plate);
      tft.drawString(rank?"A"+String(rank):
          armedGames.count()>=MultiGameArming::kMaxArmed?"FULL":"ARM",
          237,y+18,1);
      tft.setTextDatum(TL_DATUM);
      drawFollowStar(g,290,y+18,BG);
    }
    if (gameCount > 3) {
      tft.setTextDatum(TR_DATUM);
      tft.setTextColor(MUTED, BG);
      tft.drawString("< PREV         NEXT >", 309, 40, 2);
      tft.setTextDatum(TL_DATUM);
    }
  } else if (screen == Screen::SETUP) {
    if(WiFi.status()!=WL_CONNECTED){
      textAt("Join hotspot",12,44,WHITE,2);
      textAt("Scan with phone",12,69,MUTED,2);
      textAt(apSsid,12,95,WHITE,2);
      textAt("Then open:",12,119,MUTED,2);
      textAt("192.168.4.1",12,140,WHITE,2);
      drawHotspotJoinQr(198,42,3);
    } else {
      textAt("Web setup", 12, 43, WHITE, 4);
      textAt("Network: " + cfg.ssid, 12, 76, MUTED);
      textAt("IP: " + localAddress(), 12, 97, MUTED);
      textAt("AP: " + apSsid, 12, 118, MUTED);
      textAt("AP password: " + apPassword, 12, 138, WHITE);
    }
    button(8, 161, 98, 29, "TEST");
    button(111, 161, 98, 29, "TOUCH");
    button(214, 161, 98, 29, "INVERT");
  }

  if (screen == Screen::STATS) {
    button(6, 200, 99, 34, "BACK");
    button(111, 200, 99, 34,
           isArmedGame(detailGameId)?"DISARM":
           armedGames.count()>=MultiGameArming::kMaxArmed?"MAX 5":"ARM");
    button(216,200,98,34,"");
    GameDayIcons::draw(tft,GameDayIcons::Kind::SETUP,265,217,20,WHITE);
  } else {
    button(6,200,99,34,"TODAY",screen==Screen::LIVE);
    button(111,200,99,34,"",screen==Screen::GAMES);
    GameDayIcons::draw(tft,GameDayIcons::Kind::GAMES,160,217,20,WHITE);
    button(216,200,98,34,"",screen==Screen::SETUP);
    GameDayIcons::draw(tft,GameDayIcons::Kind::SETUP,265,217,20,WHITE);
  }
}

void selectGame(uint32_t id) {
  // Legacy select promotes to rank 1; other armed games stay armed.
  if(!isScheduledGame(id) || !armedGames.promote(id))return;
  carouselHold.reset();
  overlayReturnHeldGameId=0; // Explicit game selection replaces any old hold.
  swallowLiveGesture = true;
  const bool primaryChanged=selectedGameId!=id;
  persistArmedGames();
  if(primaryChanged){
    armedPpLogSeq=0;
    lastArmedGameState="";
    haveBaseline=false; // Never infer a historical goal from promotion.
  }
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
  if(screen==Screen::SETUP) {
    const S3NativeLayout::Hit hit=S3NativeLayout::setupHitTest(
        gestureStartX,gestureStartY);
    switch(hit.action) {
      case S3NativeLayout::Action::TEST: {
        Game *g=selectedGame();
        celebrate(teamRgb(g?g->home:String("UTA")));
        break;
      }
      case S3NativeLayout::Action::TOUCH_CHECK:
        s3TouchCheckMask=0;
        s3TouchCheckX=s3TouchCheckY=-1;
        screen=Screen::CALIBRATE;
        break;
      case S3NativeLayout::Action::COLOR_MODE:
        cfg.invertScreen=!cfg.invertScreen;
        tft.invertDisplay(cfg.invertScreen);
        prefs.putBool("invert",cfg.invertScreen);
        break;
      case S3NativeLayout::Action::PREVIOUS:
        screen=Screen::LIVE;
        break;
      case S3NativeLayout::Action::GAMES:
        screen=Screen::GAMES;
        break;
      case S3NativeLayout::Action::BACK:
        screen=s3ReturnFromSetup==Screen::CALIBRATE?
            Screen::LIVE:s3ReturnFromSetup;
        break;
      default:break;
    }
    lastCarouselAdvance=millis();
    needsDraw=true;
    return;
  }
  if(screen==Screen::CALIBRATE) {
    const S3NativeLayout::Hit hit=S3NativeLayout::touchCheckHitTest(
        gestureStartX,gestureStartY);
    switch(hit.action) {
      case S3NativeLayout::Action::TOUCH_CHECK:
        s3TouchCheckMask|=uint8_t(1U<<hit.row);
        s3TouchCheckX=gestureEndX;
        s3TouchCheckY=gestureEndY;
        break;
      case S3NativeLayout::Action::RESET_CHECK:
        s3TouchCheckMask=0;
        s3TouchCheckX=s3TouchCheckY=-1;
        break;
      case S3NativeLayout::Action::BACK:
        screen=Screen::SETUP;
        break;
      case S3NativeLayout::Action::GAMES:
        screen=Screen::GAMES;
        break;
      default:
        s3TouchCheckX=gestureEndX;
        s3TouchCheckY=gestureEndY;
        break;
    }
    needsDraw=true;
    return;
  }
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
        if(detailGameId)
          editArmedGame(detailGameId,!isArmedGame(detailGameId));
        break;
      case S3NativeLayout::Action::SETUP:
        s3ReturnFromSetup=Screen::STATS;
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
  if(screen==Screen::LIVE && newsViewEnabled()){
    if(S3NativeLayout::carouselSwipe(dx,dy))advanceNews(dx<0?1:-1);
    else if(gestureStartY<65 && gestureStartX>=385)screen=Screen::GAMES;
    else if(gestureStartY>248 && gestureStartX>=385)screen=Screen::GAMES;
    else advanceNews(1);
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
          s3ReturnFromSetup=Screen::GAMES;
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
        case S3NativeLayout::Action::SUMMARY_ARM:
        case S3NativeLayout::Action::SUMMARY_DETAILS: {
          const uint8_t index=gamePage*4+hit.row;
          if(index<gameCount) {
            const Game &g=games[index];
            if(hit.action==S3NativeLayout::Action::SUMMARY_FOLLOW)
              setGameFollow(g.id,!rearFollows(g.id));
            else if(hit.action==S3NativeLayout::Action::SUMMARY_ARM)
              editArmedGame(g.id,!isArmedGame(g.id));
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
#if defined(GLOWDECK_S3_PHASE6_LIVE) && GLOWDECK_S3_PHASE6_LIVE
  if(screen==Screen::LIVE && showingGamePage() && s3Arena.ready()) {
    // A diagonal drag or imperfect swipe must never accidentally follow a
    // matchup or open Setup/Stats. The regular carousel swipe was handled
    // above; only deliberate short taps reach the hit-map below.
    if(!S3NativeLayout::broadcastTap(dx,dy)) {
      lastCarouselAdvance=millis();
      return;
    }
    const auto hit=S3NativeLayout::broadcastHitTest(gestureStartX,gestureStartY);
    const LiveCarousel::Page page=currentCarouselPage();
    switch(hit.action){
      case S3NativeLayout::Action::FOLLOW:
        if(page.index<gameCount){
          const Game &g=games[page.index];
          setGameFollow(g.id,!rearFollows(g.id));
        }
        break;
      case S3NativeLayout::Action::GAMES:
        screen=Screen::GAMES;
        break;
      case S3NativeLayout::Action::MENU:
        s3ReturnFromSetup=Screen::LIVE;
        screen=Screen::SETUP;
        break;
      case S3NativeLayout::Action::PREVIOUS:
        if(carouselPageCount()>1)advanceCarousel(-1);
        break;
      case S3NativeLayout::Action::NEXT:
        if(carouselPageCount()>1)advanceCarousel(1);
        break;
      case S3NativeLayout::Action::PAUSE:
        toggleCarouselHold();
        break;
      case S3NativeLayout::Action::DETAILS:
        if(page.index<gameCount)showDetail(games[page.index].id);
        break;
      default:break;
    }
    lastCarouselAdvance=millis();
    needsDraw=true;
    return;
  }
#endif
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
    case S3NativeLayout::Action::SUMMARY_ARM:
    case S3NativeLayout::Action::SUMMARY_DETAILS:
      if(summary && hit.row<page.count && page.rows[hit.row]<gameCount) {
        const Game &g=games[page.rows[hit.row]];
        if(hit.action==S3NativeLayout::Action::SUMMARY_FOLLOW)
          setGameFollow(g.id,!rearFollows(g.id));
        else if(hit.action==S3NativeLayout::Action::SUMMARY_ARM)
          editArmedGame(g.id,!isArmedGame(g.id));
        else showDetail(g.id);
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
      swallowLiveGesture = false; // Release completed during a queued overlay.
    } else if (!touchWasDown && millis() - lastTouch > 160UL) {
      touchWasDown = true;
      lastTouch = millis();
      const TS_Point p = touch.getPoint();
#ifdef GLOWDECK_WAVESHARE_S3
      const TS_Point native = touch.getScreenPoint();
      const bool dismiss=S3NativeLayout::overlayCloseHit(
          native.x,native.y);
#else
      const int16_t x = constrain((int)map(p.x, cfg.touchX0, cfg.touchX1, 0, 319), 0, 319);
      const int16_t y = constrain((int)map(p.y, cfg.touchY0, cfg.touchY1, 0, 239), 0, 239);
      const bool dismiss=x>=266 && y<=39;
#endif
      if (dismiss) {
        overlayController.dismiss(millis());
        // Do not reuse this press/release on the underlying carousel.
        swallowLiveGesture=true;
        overlayLastFrame = 0;
        lastCarouselAdvance = millis();
        needsDraw = true;
      }
    }
    return;
  }
#ifdef GLOWDECK_WAVESHARE_S3
  if (screen==Screen::LIVE || screen==Screen::GAMES ||
      screen==Screen::STATS || screen==Screen::SETUP ||
      screen==Screen::CALIBRATE) {
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
      if (newsViewEnabled()) {
        if(abs(dx)>48 && abs(dy)<90)advanceNews(dx<0?1:-1);
        else if(gestureStartY<=45 && gestureStartX>=260)
          screen=Screen::GAMES;
        else advanceNews(1);
      } else if (carouselPageCount() > 1 && abs(dx) > 48 && abs(dy) < 90) {
        advanceCarousel(dx < 0 ? 1 : -1);
      } else if (showingGamePage() && gameCount &&
                 GameFollowPolicy::headerStarTap(
                     gestureStartX,gestureStartY,gestureEndX,gestureEndY)) {
        const Game &g=games[currentCarouselPage().index];
        setGameFollow(g.id,!rearFollows(g.id));
      } else if (gestureStartY <= 32 && gestureStartX >= 177 &&
                 gestureStartX <= 244 && showingGamePage()) {
        if (gestureStartX < 196) {
          if(carouselPageCount()>1)advanceCarousel(-1);
        } else if (gestureStartX < 220) {
          toggleCarouselHold();
        } else if (carouselPageCount()>1) {
          advanceCarousel(1);
        }
      } else if (gestureStartY <= 35 && gestureStartX >= 303) {
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
      else if (x < 214) editArmedGame(detailGameId,!isArmedGame(detailGameId));
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
        else if(x>=214)editArmedGame(id,!isArmedGame(id));
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
  // Captive-portal detector requests. These redirect to the existing web
  // dashboard only for verified clients on the GlowDeck setup AP.
  // Secure/WPA2 join QR, existing home-Wi-Fi Basic auth and routes are intact.
  static const char *const probePaths[] = {
    "/generate_204", "/gen_204", "/hotspot-detect.html",
    "/canonical.html", "/ncsi.txt", "/connecttest.txt",
    "/redirect", "/fwlink", "/success.txt"
  };
  for (const char *path : probePaths)
    server.on(path, HTTP_GET, sendCaptiveSetupRedirect);
  server.on("/", HTTP_GET, []() {
    if (!authorized()) return;
    // Manual opening of 192.168.4.1 on a disconnected GameDay should also
    // lead to Wi-Fi setup, not an empty NHL dashboard. A setup=wifi request
    // is the final destination (no redirect loop). LAN clients and online
    // devices keep the ordinary overview.
    if(requestFromSetupAp() && WiFi.status()!=WL_CONNECTED &&
       !server.hasArg("setup")){
      server.sendHeader("Location", "http://192.168.4.1/?setup=wifi#settings");
      server.sendHeader("Cache-Control", "no-store");
      server.send(302, "text/plain", "Open GameDay Wi-Fi setup");
      return;
    }
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
    DynamicJsonDocument doc(16000);
    if (doc.capacity() < 16000)
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
      row["audio"] = GameEventLog::audioLabel(e->audio);
      row["audioSource"] = e->audioSource;
      row["audioFallback"] = e->audioFallback;
      row["audioFrames"] = e->audioFrames;
      row["audioExpectedFrames"] = e->audioExpectedFrames;
      row["audioDurationMs"] = e->audioDurationMs;
      row["audioPeak"] = e->audioPeak;
      row["audioRetries"] = e->audioRetries;
      row["audioReadOffset"] = e->audioReadOffset;
      row["audioI2sCode"] = e->audioI2sCode;
      row["audioVolume"] = e->audioVolume;
      row["audioAmpVerified"] = e->audioAmpVerified;
      row["audioFailure"] = GameEventLog::audioFailureLabel(e->audioFailure);
      row["speakerVerified"] = false;
      row["hue"] = GameEventLog::wledLabel(e->hue);
      row["hueAccepted"] = e->hueAcknowledged;
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
    doc["hornEnabled"] = cfg.horn;
#ifdef GLOWDECK_WAVESHARE_S3
    doc["audioSupported"] = true;
    doc["audioReady"] = s3Audio.ready();
    doc["audioPlaying"] = s3Audio.playing();
    doc["audioVolume"] = s3Audio.volume();
    doc["audioError"] = s3Audio.error();
    doc["audioHasPlayed"] = s3Audio.hasPlayed();
    doc["audioLastFrames"] = s3Audio.writtenFrames();
    doc["audioLastStreamOk"] = s3Audio.lastStreamOk();
    doc["audioLastDurationMs"] = s3Audio.lastDurationMs();
    doc["audioAmpEnabled"] = s3Audio.ampEnabled();
    doc["audioLastClip"] = s3Audio.clipName();
    doc["audioSelectedSource"] = s3Audio.selectedSource();
    doc["audioPowerPlaySource"] = cfg.s3PpSound;
    doc["audioGameStartSource"] = cfg.s3StartSound;
    doc["audioGameFinishSource"] = cfg.s3FinishSound;
    doc["audioStorageReady"] = soundStorage.mounted();
    doc["caseLedEnabled"]=cfg.caseLedsEnabled;
    doc["caseLedActive"]=caseLedDriver.enabled();
#else
    doc["audioSupported"] = false;
#endif
    doc["wifiConnected"] = WiFi.status() == WL_CONNECTED;
    doc["ip"] = localAddress();
    doc["apSsid"] = apSsid;
    doc["apIp"] = WiFi.softAPIP().toString();
    doc["mdns"] = mdnsStarted ? mdnsName + ".local" : "";
    doc["selectedId"] = selectedGameId;
    doc["armedCapacity"] = MultiGameArming::kMaxArmed;
    doc["armedCount"] = armedGames.count();
    JsonArray armedList=doc.createNestedArray("armedIds");
    for(uint8_t i=0;i<MultiGameArming::kMaxArmed;++i)
      if(armedGames.ids[i])armedList.add(armedGames.ids[i]);
    doc["wledActiveGameId"]=wledActiveGameId;
    doc["wledQueuedEvents"]=wledPriorityQueue.count;
    doc["wledQueueDrops"]=wledQueuedDrops;
    JsonArray owners=doc.createNestedArray("wledOwnerGames");
    for(uint8_t i=0;i<MAX_WLED;++i)owners.add(wledDeviceGameOwner[i]);
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
                       rearEvent == RearEvent::SHOOTOUT ? "SHOOTOUT" :
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
    doc["carouselPaused"] = carouselHold.paused;
    doc["carouselHeldGameId"] = carouselHold.gameId;
    doc["overlayReturnHeldGameId"] = overlayReturnHeldGameId;
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
  server.on("/api/news",HTTP_GET,[](){
    if(!authorized())return;
    const String filterArg=server.arg("filter");
    NhlNews::Filter filter=NhlNews::Filter::ALL;
    if(filterArg=="followed")filter=NhlNews::Filter::FOLLOWED;
    else if(filterArg=="pregame")filter=NhlNews::Filter::PREGAME;
    else if(filterArg=="postgame")filter=NhlNews::Filter::POSTGAME;
    const uint32_t mask=newsFollowMask();
    DynamicJsonDocument doc(10000);
    doc["source"]=newsCache.source;
    doc["cached"]=newsCache.count>0;
    doc["lastError"]=newsLastError;
    doc["fallbackPending"]=newsTryEspnNext;
    doc["lastAttemptMs"]=newsLastAttempt;
    doc["freeHeap"]=ESP.getFreeHeap();
    doc["idleMode"]=newsViewEnabled();
    doc["paused"]=carouselHold.paused;
    doc["ageSeconds"]=newsCache.lastSuccessAt?
       uint32_t(millis()-newsCache.lastSuccessAt)/1000UL:0;
    doc["httpCode"]=newsCache.lastHttpCode;
    doc["failures"]=newsCache.failures;
    doc["live"]=newsAnyLive();
    doc["matches"]=newsCache.matching(mask,filter);
    JsonArray arr=doc.createNestedArray("articles");
    for(uint8_t i=0;i<newsCache.count;++i){
      const NhlNews::Story *story=newsCache.ranked(i,mask,filter);
      if(!story)break;
      JsonObject row=arr.createNestedObject();
      row["slug"]=story->slug;
      row["headline"]=story->headline;
      row["summary"]=story->summary;
      row["published"]=story->published;
      row["team"]=story->team;
      row["kind"]=NhlNews::kindLabel(story->kind);
      row["image"]=story->image;
      row["url"]=story->url;
      row["source"]=story->source;
    }
    sendJson(doc);
  });
  server.on("/api/news/refresh",HTTP_POST,[](){
    if(!authorized())return;
    newsRefreshRequested=true; // No HTTP request in a web handler.
    StaticJsonDocument<128> doc;
    doc["queued"]=true;
    doc["note"]="Refresh runs when live games and lights are idle";
    sendJson(doc,202);
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
      for(const auto &so:shootoutWatches)if(so.gameId==games[i].id){
        g["soHomeAttempts"]=so.homeAttempts;
        g["soAwayAttempts"]=so.awayAttempts;
        g["soHomeGoals"]=so.homeGoals;
        g["soAwayGoals"]=so.awayGoals;
        break;
      }
      g["clock"] = games[i].clock;
      g["intermission"] = games[i].intermission;
      g["powerPlaySide"] = games[i].ppSide;
      g["powerPlayVerified"] = games[i].ppDataKnown;
      g["powerPlaySession"] = games[i].ppSession;
      g["powerPlayPhase"] =
          NhlPowerPlayTracker2::phaseName(games[i].ppPhase);
      g["emptyNetSide"] = games[i].emptyNetSide;
      g["emptyNetKnown"] = games[i].emptyNetKnown;
      g["followed"] = rearFollows(games[i].id);
      g["armed"] = isArmedGame(games[i].id);
      g["armedPriority"] = armPriority(games[i].id);
    }
    sendJson(doc);
  });
  // Read-only Power-Play Tracker 2.0 diagnostics. Shared session and
  // raw official evidence are exposed separately to explain why the badge
  // and LEDs are playing, held, resumed or stopped. No NHL fetch is made.
  server.on("/api/pp-tracker",HTTP_GET,[](){
    if(!authorized())return;
    DynamicJsonDocument doc(6144);
    doc["version"]=2;
    doc["snapshotMs"]=millis();
    JsonArray rows=doc.createNestedArray("games");
    for(uint8_t i=0;i<gameCount;++i){
      const Game &g=games[i];
      if(g.state!="LIVE"&&g.state!="CRIT"&&g.ppSession==0)continue;
      const NhlPowerPlayTracker2::Watch *watch=
          powerPlaySession(g.id,false);
      JsonObject row=rows.createNestedObject();
      row["id"]=g.id;row["matchup"]=g.away+" @ "+g.home;
      row["state"]=g.state;row["intermission"]=g.intermission;
      row["period"]=g.period;row["clock"]=g.clock;
      row["phase"]=watch?NhlPowerPlayTracker2::phaseName(watch->phase):
          "IDLE";
      row["reason"]=watch?NhlPowerPlayTracker2::reasonName(watch->reason):
          "no session";
      row["session"]=watch?watch->session:0;
      row["trackedSide"]=watch?watch->side:0;
      row["indicatorSide"]=g.ppSide;
      row["candidateSide"]=g.ppCandidate;
      row["situationKnown"]=g.ppSituationKnown;
      row["penaltyKey"]=watch?watch->anchorKey:0;
      row["penaltyAt"]=watch?watch->anchorPenaltyAt:0;
      row["penaltyEndAt"]=watch?watch->expectedEndAt:0;
      row["evidenceAgeMs"]=(watch&&watch->everTrusted)?
          int32_t(millis()-watch->lastEvidenceMs):-1;
      const NhlPenaltyLedger::Book *ledger=nullptr;
      for(const auto &entry:penaltyLedgers)
        if(entry.gameId==g.id){ledger=&entry;break;}
      row["ledgerAgeMs"]=ledger&&ledger->loaded?
          int32_t(millis()-ledger->checkedAt):-1;
      uint16_t at=0;
      const bool clockValid=NhlPenaltyLedger::Book::fromRemaining(
          g.period,g.clock.c_str(),at,
          g.period==4&&g.periodType=="OT"&&
          ((g.id/10000UL)%100UL)==2UL);
      row["gameClockAt"]=clockValid?int(at):-1;
      row["ledgerSide"]=ledger&&ledger->loaded&&clockValid?
          ledger->sideAt(at):0;
      row["wledOwner"]=wledActiveGameId==g.id &&
          wledActiveEvent==CelebrationEngine::Event::POWER_PLAY;
      row["armed"]=isArmedGame(g.id);
      row["followed"]=rearFollows(g.id);
    }
    if(doc.overflowed())return errorResponse("PP diagnostic full",503);
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
  server.on("/api/arm",HTTP_POST,[](){
    if(!authorized())return;
    StaticJsonDocument<192> request;
    if(deserializeJson(request,server.arg("plain")) ||
       !request["id"].is<uint32_t>())
      return errorResponse("Specify an NHL game id",400);
    const uint32_t id=request["id"].as<uint32_t>();
    if(!isScheduledGame(id))
      return errorResponse("Game not on today's schedule",404);
    const String action=request["action"]|String("");
    bool success=false;
    if(action=="up"||action=="down"){
      success=moveArmedGame(id,action=="up"?-1:1);
      if(!success)return errorResponse("Game already at end of priority list",409);
    }else if(request["armed"].is<bool>()){
      success=editArmedGame(id,request["armed"].as<bool>());
      if(!success)return errorResponse("Only five games may be armed",409);
    }else return errorResponse("Provide armed=true/false or action=up/down",400);
    StaticJsonDocument<256> reply;
    reply["ok"]=true;reply["id"]=id;
    reply["armed"]=isArmedGame(id);reply["priority"]=armPriority(id);
    reply["count"]=armedGames.count();reply["max"]=MultiGameArming::kMaxArmed;
    sendJson(reply);
  });
  server.on("/api/select", HTTP_POST, []() {
    if (!authorized()) return;
    StaticJsonDocument<128> doc;
    if (deserializeJson(doc, server.arg("plain"))) return errorResponse("Invalid JSON");
    uint32_t id = doc["id"] | 0U;
    for (uint8_t i = 0; i < gameCount; ++i) {
      if (games[i].id == id) {
        if(!isArmedGame(id) && armedGames.count()>=MultiGameArming::kMaxArmed)
          return errorResponse("Five games already armed; disarm one first",409);
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
#ifdef GLOWDECK_WAVESHARE_S3
    doc["s3Volume"] = s3Audio.volume();
    doc["s3VolumeMin"] = S3HornPolicy::kMinVolumePercent;
    doc["s3VolumeMax"] = S3HornPolicy::kMaxVolumePercent;
    doc["s3Sound"] = s3Audio.selectedSource();
    doc["s3PpSound"] = cfg.s3PpSound;
    doc["s3StartSound"] = cfg.s3StartSound;
    doc["s3FinishSound"] = cfg.s3FinishSound;
    doc["caseLedsEnabled"]=cfg.caseLedsEnabled;
    doc["casePixels"]=cfg.casePixels;
    doc["caseBrightness"]=cfg.caseBrightness;
    doc["caseGoal"]=cfg.caseGoal;
    doc["casePp"]=cfg.casePp;
    doc["caseStart"]=cfg.caseStart;
    doc["caseFinal"]=cfg.caseFinal;
    doc["caseGoalSeconds"]=cfg.caseGoalSeconds;
#endif
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
      d["name"] = cfg.wledName[i];
      d["zoneEnabled"] = cfg.wledZoneEnabled[i];
      d["ddpEnabled"] = cfg.wledDdpEnabled[i];
      d["ddpFps"] = cfg.wledDdpFps[i];
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
      String label=arr[i]["name"] | cfg.wledName[i];
      label.trim();
      if(label.length()>32)return errorResponse("WLED name must be at most 32 characters");
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
      if(cfg.wled[i]!=ip && cfg.wledDdpEnabled[i]){
        // Exclusive streaming consent never follows an edited IP address.
        cfg.wledDdpEnabled[i]=false;
        prefs.putBool(("ddpOn"+String(i)).c_str(),false);
        wledDdp[i].status="Disabled: controller IP changed";
      }
      cfg.wled[i] = ip;
      cfg.wledName[i] = label;
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
    result["armed"]=isArmedGame(id);
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
#ifdef GLOWDECK_WAVESHARE_S3
  // S3-only Phase 7A: local horn test, no WLED, score change, animation,
  // NHL event log or NVS writes. Preview works even if cfg.horn is off.
  server.on("/api/audio/preview", HTTP_POST, []() {
    if (!authorized()) return;
    if (!s3Audio.ready())return errorResponse(s3Audio.error(),503);
    StaticJsonDocument<96> request;
    const String body=server.arg("plain");
    if(body.length() && deserializeJson(request,body))
      return errorResponse("Invalid preview request");
    if(request.containsKey("source") && !request["source"].is<int>())
      return errorResponse("Preview source must be an integer");
    const int requested=request["source"] | int(s3Audio.selectedSource());
    if(requested<0 || requested>S3WavPolicy::kSlots)
      return errorResponse("Invalid sound slot");
    const uint8_t source=uint8_t(requested);
    S3WavPolicy::Info meta;
    if(source && !soundStorage.inspect(source,meta))
      return errorResponse("WAV slot is empty or invalid",400);
    // Preview only: never records an NHL event, enqueues an overlay,
    // changes the selected sound or touches WLED devices.
    if(!(request.containsKey("source")?s3Audio.playSource(source):
                                            s3Audio.play()))
      return errorResponse(s3Audio.playing()?"Audio already playing":
                           "Speaker unavailable, uploading, or cooldown",409);
    StaticJsonDocument<160> doc;
    doc["ok"]=true;
    doc["volume"]=s3Audio.volume();
    doc["durationMs"]=source?meta.durationMs:S3HornPolicy::kDurationMs;
    doc["source"]=source;
    doc["sound"]=source?"custom-wav":"onboard-goal-horn";
    sendJson(doc);
  });
  // Independent 1kHz audible diagnostic at the same fixed codec gain.
  // No change to cfg.horn, overlays, events, WLED or follow state.
  server.on("/api/audio/test-tone", HTTP_POST, []() {
    if (!authorized()) return;
    if (!s3Audio.ready())return errorResponse(s3Audio.error(),503);
    if (!s3Audio.testTone())return errorResponse(
        "Speaker busy, cooldown, or PA check failed",409);
    StaticJsonDocument<128> doc;
    doc["ok"]=true;
    doc["sound"]="test-tone-1khz";
    doc["volume"]=s3Audio.volume();
    doc["durationMs"]=S3HornPolicy::kTestDurationMs;
    sendJson(doc);
  });
  // Standalone S3-only gain control. No NHL events, WLED or CYD NVS keys.
  // Apply and verify DAC gain before storing the preference; never
  // rewrite the codec volume during active I2S playback.
  server.on("/api/audio/volume", HTTP_POST, []() {
    if (!authorized()) return;
    if (!s3Audio.ready()) return errorResponse(s3Audio.error(),503);
    StaticJsonDocument<96> request;
    if (deserializeJson(request,server.arg("plain")))
      return errorResponse("Invalid audio volume JSON");
    if (!request.containsKey("percent") || !request["percent"].is<int>())
      return errorResponse("Supply integer percent in audio volume request");
    const int percent=request["percent"].as<int>();
    if (percent<S3HornPolicy::kMinVolumePercent ||
        percent>S3HornPolicy::kMaxVolumePercent)
      return errorResponse("Allowed S3 speaker volume is 30 to 80 percent");
    if (!s3Audio.setVolume(uint8_t(percent)))
      return errorResponse(s3Audio.playing()?"Audio is playing":
                           "ES8311 gain update/readback failed",409);
    cfg.s3Volume=uint8_t(percent);
    prefs.putUChar("s3Vol",cfg.s3Volume);
    StaticJsonDocument<96> response;
    response["ok"]=true;
    response["volume"]=s3Audio.volume();
    sendJson(response);
  });
  // Phase 7B.1/7B.2 — authenticated S3 sound library. Fixed slot names
  // and bounded PCM WAV parsing avoid arbitrary paths or untrusted codecs.
  server.on("/api/audio/library", HTTP_GET, []() {
    if (!authorized()) return;
    DynamicJsonDocument doc(1800);
    doc["supported"]=true;
    doc["storageReady"]=soundStorage.mounted();
    doc["selected"]=s3Audio.selectedSource();
    doc["powerPlaySelected"]=cfg.s3PpSound;
    doc["gameStartSelected"]=cfg.s3StartSound;
    doc["gameFinishSelected"]=cfg.s3FinishSound;
    doc["slots"]=S3WavPolicy::kSlots;
    doc["maxFileBytes"]=S3WavPolicy::kMaxBytes;
    doc["totalBytes"]=soundStorage.totalBytes();
    doc["usedBytes"]=soundStorage.usedBytes();
    JsonArray clips=doc.createNestedArray("clips");
    for(uint8_t slot=1;slot<=S3WavPolicy::kSlots;++slot){
      S3WavPolicy::Info m;
      const bool valid=soundStorage.inspect(slot,m);
      JsonObject clip=clips.createNestedObject();
      clip["slot"]=slot;
      clip["installed"]=valid;
      clip["name"]=prefs.getString(("s3Name"+String(slot)).c_str(),
                                   "Custom sound "+String(slot));
      clip["bytes"]=soundStorage.bytes(slot);
      if(valid){
        clip["durationMs"]=m.durationMs;
        clip["sampleRate"]=m.sampleRate;
        clip["channels"]=m.channels;
        clip["bits"]=m.bits;
      }
    }
    if(doc.overflowed())return errorResponse("Sound library response too large",503);
    sendJson(doc);
  });
  server.on("/api/audio/select", HTTP_POST, []() {
    if (!authorized())return;
    if(s3Audio.playing() || s3Audio.storageBusy())
      return errorResponse("Finish current playback or upload first",409);
    StaticJsonDocument<96> request;
    if(deserializeJson(request,server.arg("plain")))
      return errorResponse("Invalid sound selection JSON");
    if(!request.containsKey("source") || !request["source"].is<int>())
      return errorResponse("Select an integer source 0 through 4");
    const int source=request["source"].as<int>();
    if(source<0 || source>S3WavPolicy::kSlots)
      return errorResponse("Sound source out of range");
    S3WavPolicy::Info m;
    if(source && !soundStorage.inspect(uint8_t(source),m))
      return errorResponse("That custom sound is missing or invalid",400);
    cfg.s3Sound=uint8_t(source);
    s3Audio.setSelectedSource(cfg.s3Sound);
    prefs.putUChar("s3Sound",cfg.s3Sound);
    StaticJsonDocument<96> response;
    response["ok"]=true;
    response["selected"]=cfg.s3Sound;
    sendJson(response);
  });
  // Distinct PP audio assignment. Source 0 disables; it never changes the
  // working goal horn selection or cfg.horn. No NHL eligibility changes.
  server.on("/api/audio/gamefinish-select", HTTP_POST, []() {
    if(!authorized())return;
    if(s3Audio.playing() || s3Audio.storageBusy())
      return errorResponse("Audio busy; retry after playback",409);
    StaticJsonDocument<96> request;
    if(deserializeJson(request,server.arg("plain")))
      return errorResponse("Invalid game-finish sound JSON");
    if(!request.containsKey("source") || !request["source"].is<int>())
      return errorResponse("Source must be an integer 0 through 4");
    const int source=request["source"].as<int>();
    if(source<0 || source>S3WavPolicy::kSlots)
      return errorResponse("Game-finish slot out of range");
    S3WavPolicy::Info meta;
    if(source && !soundStorage.inspect(uint8_t(source),meta))
      return errorResponse("Game-finish WAV missing or invalid",400);
    cfg.s3FinishSound=uint8_t(source);
    prefs.putUChar("s3FinishSound",cfg.s3FinishSound);
    StaticJsonDocument<96> response;
    response["ok"]=true;response["gameFinishSelected"]=cfg.s3FinishSound;
    sendJson(response);
  });
  server.on("/api/audio/gamestart-select", HTTP_POST, []() {
    if(!authorized())return;
    if(s3Audio.playing() || s3Audio.storageBusy())
      return errorResponse("Audio busy; retry after playback",409);
    StaticJsonDocument<96> request;
    if(deserializeJson(request,server.arg("plain")))
      return errorResponse("Invalid game-start sound JSON");
    if(!request.containsKey("source") || !request["source"].is<int>())
      return errorResponse("Source must be an integer 0 through 4");
    const int source=request["source"].as<int>();
    if(source<0 || source>S3WavPolicy::kSlots)
      return errorResponse("Game-start slot out of range");
    S3WavPolicy::Info meta;
    if(source && !soundStorage.inspect(uint8_t(source),meta))
      return errorResponse("Game-start WAV missing or invalid",400);
    cfg.s3StartSound=uint8_t(source);
    prefs.putUChar("s3StartSound",cfg.s3StartSound);
    StaticJsonDocument<96> response;
    response["ok"]=true;response["gameStartSelected"]=cfg.s3StartSound;
    sendJson(response);
  });
  server.on("/api/audio/powerplay-select", HTTP_POST, []() {
    if(!authorized())return;
    if(s3Audio.playing() || s3Audio.storageBusy())
      return errorResponse("Finish playback/upload before changing PP sound",409);
    StaticJsonDocument<96> request;
    if(deserializeJson(request,server.arg("plain")))
      return errorResponse("Invalid PP sound selection JSON");
    if(!request.containsKey("source") || !request["source"].is<int>())
      return errorResponse("Select an integer source 0 through 4");
    const int source=request["source"].as<int>();
    if(source<0 || source>S3WavPolicy::kSlots)
      return errorResponse("PP sound slot out of range");
    S3WavPolicy::Info m;
    if(source && !soundStorage.inspect(uint8_t(source),m))
      return errorResponse("That PP sound is missing or invalid",400);
    cfg.s3PpSound=uint8_t(source);
    prefs.putUChar("s3PpSound",cfg.s3PpSound);
    StaticJsonDocument<96> response;
    response["ok"]=true;
    response["powerPlaySelected"]=cfg.s3PpSound;
    sendJson(response);
  });
  server.on("/api/audio/delete", HTTP_POST, []() {
    if(!authorized())return;
    if(s3Audio.playing() || s3Audio.storageBusy())
      return errorResponse("Finish playback/upload before deleting sound",409);
    StaticJsonDocument<96> request;
    if(deserializeJson(request,server.arg("plain")))
      return errorResponse("Invalid sound deletion JSON");
    if(!request.containsKey("slot") || !request["slot"].is<int>())
      return errorResponse("Provide a sound slot number");
    const int slot=request["slot"].as<int>();
    if(slot<1 || slot>S3WavPolicy::kSlots)
      return errorResponse("Invalid sound slot");
    if(!soundStorage.mounted())return errorResponse("Flash storage unavailable",503);
    if(!soundStorage.remove(uint8_t(slot)))
      return errorResponse("Could not delete WAV file",500);
    prefs.remove(("s3Name"+String(slot)).c_str());
    if(cfg.s3Sound==slot){
      cfg.s3Sound=0; // built-in safety fallback
      s3Audio.setSelectedSource(0);
      prefs.putUChar("s3Sound",0);
    }
    if(cfg.s3FinishSound==slot){
      cfg.s3FinishSound=0;
      prefs.putUChar("s3FinishSound",0);
    }
    if(cfg.s3StartSound==slot){
      cfg.s3StartSound=0;
      prefs.putUChar("s3StartSound",0);
    }
    if(cfg.s3PpSound==slot){
      cfg.s3PpSound=0; // Removing a PP WAV disables only that announcement.
      prefs.putUChar("s3PpSound",0);
    }
    StaticJsonDocument<96> response;
    response["ok"]=true;
    response["selected"]=cfg.s3Sound;
    response["powerPlaySelected"]=cfg.s3PpSound;
    sendJson(response);
  });
  server.on("/api/audio/upload", HTTP_POST, []() {
    if (!authorized()){
      soundUpload.close();
      LittleFS.remove(S3SoundStorage::kTemp);
      s3Audio.setStorageBusy(false);
      return;
    }
    soundUpload.close();
    if(soundUploadFailed || !soundUploadFinished){
      LittleFS.remove(S3SoundStorage::kTemp);
      s3Audio.setStorageBusy(false);
      return errorResponse(soundUploadError.length()?soundUploadError.c_str():
                           "WAV upload missing or incomplete",400);
    }
    File candidate=LittleFS.open(S3SoundStorage::kTemp,"r");
    S3WavPolicy::Info meta;
    const bool valid=candidate && S3WavPolicy::parse(candidate,meta);
    candidate.close();
    if(!valid){
      LittleFS.remove(S3SoundStorage::kTemp);
      s3Audio.setStorageBusy(false);
      return errorResponse("Use PCM 16-bit WAV, mono/stereo, 8–48kHz, max 768KB/10s",400);
    }
    if(!soundStorage.install(soundUploadSlot)){
      LittleFS.remove(S3SoundStorage::kTemp);
      s3Audio.setStorageBusy(false);
      return errorResponse("Unable to install WAV; check available flash space",500);
    }
    prefs.putString(("s3Name"+String(soundUploadSlot)).c_str(),soundUploadTitle);
    s3Audio.setStorageBusy(false);
    StaticJsonDocument<192> response;
    response["ok"]=true;
    response["slot"]=soundUploadSlot;
    response["name"]=soundUploadTitle;
    response["bytes"]=soundUploadBytes;
    response["durationMs"]=meta.durationMs;
    sendJson(response);
  }, []() {
    HTTPUpload &upload=server.upload();
    if(upload.status==UPLOAD_FILE_START){
      soundUpload.close();
      soundUploadFailed=!hasDashboardAccess();
      soundUploadError=soundUploadFailed?"Unauthorized":"";
      soundUploadFinished=false;
      soundUploadBytes=0;
      const String slotArg=server.arg("slot");
      // Require an exact slot digit; never allow modulo-wrapped user IDs.
      soundUploadSlot=slotArg=="1"?1:slotArg=="2"?2:slotArg=="3"?3:slotArg=="4"?4:0;
      soundUploadTitle="";
      const String filename=upload.filename;
      const int dot=filename.lastIndexOf('.');
      const String base=dot>0?filename.substring(0,dot):filename;
      for(unsigned i=0;i<base.length() && soundUploadTitle.length()<24;++i){
        const char c=base[i];
        if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
           (c>='0'&&c<='9')||c==' '||c=='-'||c=='_')
          soundUploadTitle+=c;
      }
      soundUploadTitle.trim();
      if(soundUploadTitle.isEmpty())
        soundUploadTitle="Custom sound "+String(soundUploadSlot);
      if(!soundUploadFailed){
        if(!S3WavPolicy::validSlot(soundUploadSlot)){
          soundUploadError="Select sound slot 1 through 4";
          soundUploadFailed=true;
        }else if(!soundStorage.mounted()){
          soundUploadError="Sound storage not available";
          soundUploadFailed=true;
        }else if(s3Audio.playing() || s3Audio.storageBusy()){
          soundUploadError="Finish playback or other upload first";
          soundUploadFailed=true;
        }else{
          s3Audio.setStorageBusy(true);
          LittleFS.remove(S3SoundStorage::kTemp);
          soundUpload=LittleFS.open(S3SoundStorage::kTemp,"w");
          if(!soundUpload){
            soundUploadError="Cannot open temporary WAV file";
            soundUploadFailed=true;
          }
        }
      }
    }else if(upload.status==UPLOAD_FILE_WRITE){
      if(!soundUploadFailed){
        if(upload.currentSize>S3WavPolicy::kMaxBytes-soundUploadBytes ||
           soundUpload.write(upload.buf,upload.currentSize)!=upload.currentSize){
          soundUploadError="WAV exceeds 768KB or flash write failed";
          soundUploadFailed=true;
        }else soundUploadBytes+=upload.currentSize;
      }
    }else if(upload.status==UPLOAD_FILE_END){
      soundUpload.close();
      soundUploadFinished=!soundUploadFailed && soundUploadBytes>=44;
      if(!soundUploadFinished && soundUploadError.isEmpty())
        soundUploadError="Incomplete WAV";
    }else if(upload.status==UPLOAD_FILE_ABORTED){
      soundUpload.close();
      soundUploadError="WAV upload aborted";
      soundUploadFailed=true;
      soundUploadFinished=false;
      LittleFS.remove(S3SoundStorage::kTemp);
      s3Audio.setStorageBusy(false);
    }
  });
  // GET gives observable I2C codec registers, expander state and actual
  // I2S write results. A READY flag by itself does NOT prove speaker audio.
  server.on("/api/audio/diagnostics", HTTP_GET, []() {
    if (!authorized()) return;
    const S3GoalAudio::Probe p=s3Audio.probe();
    StaticJsonDocument<448> doc;
    doc["supported"]=true;
    doc["ready"]=s3Audio.ready();
    doc["playing"]=s3Audio.playing();
    doc["codecRead"]=p.codecRead;
    doc["codecReset"]=p.codecReset;
    doc["codecMute"]=p.codecMute;
    doc["codecVolume"]=p.codecVolume;
    doc["volumePercent"]=s3Audio.volume();
    doc["codecId1"]=p.codecId1;
    doc["codecId2"]=p.codecId2;
    doc["expanderRead"]=p.expanderRead;
    doc["ampDirection"]=p.ampDirection;
    doc["ampOutput"]=p.ampOutput;
    doc["ampEnabled"]=s3Audio.ampEnabled();
    doc["lastClip"]=s3Audio.clipName();
    doc["hasPlayed"]=s3Audio.hasPlayed();
    doc["framesWritten"]=s3Audio.writtenFrames();
    doc["streamOk"]=s3Audio.lastStreamOk();
    doc["streamDurationMs"]=s3Audio.lastDurationMs();
    doc["expectedFrames"]=s3Audio.expectedFrames();
    doc["peakSample"]=s3Audio.peakSample();
    doc["writeRetries"]=s3Audio.writeRetries();
    doc["readFailureOffset"]=s3Audio.readFailureOffset();
    doc["i2sCode"]=s3Audio.lastI2sCode();
    doc["failureReason"]=S3GoalAudio::failureName(s3Audio.lastFailure());
    doc["fallbackUsed"]=s3Audio.usedFallback();
    doc["ampVerifiedAtStart"]=s3Audio.ampVerifiedAtStart();
    doc["speakerVerified"]=false;
    doc["error"]=s3Audio.error();
    sendJson(doc);
  });
#endif
#ifdef GLOWDECK_WAVESHARE_S3
  // Phase 7C S3 external case strip controls. Never shares CYD RGB pins
  // or any WLED controller and never changes NHL state or the display.
  server.on("/api/case-led", HTTP_GET, []() {
    if(!authorized())return;
    StaticJsonDocument<400> doc;
    doc["supported"]=true;
    doc["enabled"]=cfg.caseLedsEnabled;
    doc["outputActive"]=caseLedDriver.enabled();
    doc["physicalOutputVerified"]=false; // software cannot detect unbuilt strips
    doc["homeSide"]="left";
    doc["awaySide"]="right";
    doc["homeGPIO"]=S3CaseLedDriver::kHomePin;
    doc["awayGPIO"]=S3CaseLedDriver::kAwayPin;
    doc["homeHeaderPin"]=14;
    doc["awayHeaderPin"]=16;
    doc["pixelsPerSide"]=cfg.casePixels;
    doc["brightness"]=cfg.caseBrightness;
    doc["goal"]=cfg.caseGoal;
    doc["powerPlay"]=cfg.casePp;
    doc["gameStart"]=cfg.caseStart;
    doc["final"]=cfg.caseFinal;
    doc["goalSeconds"]=cfg.caseGoalSeconds;
    doc["homeEffect"]=int(caseLedFx.eventAt(
        S3CaseLedPolicy::Side::HOME,millis()));
    doc["awayEffect"]=int(caseLedFx.eventAt(
        S3CaseLedPolicy::Side::AWAY,millis()));
    sendJson(doc);
  });
  server.on("/api/case-led/config", HTTP_POST, []() {
    if(!authorized())return;
    StaticJsonDocument<512> request;
    if(server.arg("plain").length()>512 ||
       deserializeJson(request,server.arg("plain")))
      return errorResponse("Invalid case LED settings JSON");
    const bool enabled=request["enabled"]|cfg.caseLedsEnabled;
    // Require explicit confirmation only on transitions from OFF to ON.
    // No strip/data pin is initialized by default.
    if(enabled && !cfg.caseLedsEnabled &&
       !(request["wiredConfirmed"]|false))
      return errorResponse("Confirm external 5V wiring and common GND before enabling",400);
    const int count=request["pixelsPerSide"]|int(cfg.casePixels);
    const int brightness=request["brightness"]|int(cfg.caseBrightness);
    const int duration=request["goalSeconds"]|int(cfg.caseGoalSeconds);
    if(count<1 || count>S3CaseLedPolicy::kMaxPixels)
      return errorResponse("Select 1–16 LEDs per side",400);
    if(brightness<1 || brightness>S3CaseLedPolicy::kMaxBrightness)
      return errorResponse("Case brightness must be 1–128 (50% cap)",400);
    if(duration<2 || duration>12)
      return errorResponse("Case goal duration must be 2–12 seconds",400);
    cfg.casePixels=uint8_t(count);
    cfg.caseBrightness=uint8_t(brightness);
    cfg.caseGoalSeconds=uint8_t(duration);
    cfg.caseGoal=request["goal"]|cfg.caseGoal;
    cfg.casePp=request["powerPlay"]|cfg.casePp;
    cfg.caseStart=request["gameStart"]|cfg.caseStart;
    cfg.caseFinal=request["final"]|cfg.caseFinal;
    cfg.caseLedsEnabled=enabled;
    if(!enabled)caseLedFx.stop();
    caseLedDriver.enable(enabled);
    prefs.putBool("caseOn",cfg.caseLedsEnabled);
    prefs.putBool("caseGoal",cfg.caseGoal);
    prefs.putBool("casePP",cfg.casePp);
    prefs.putBool("caseStart",cfg.caseStart);
    prefs.putBool("caseFinal",cfg.caseFinal);
    prefs.putUChar("casePixels",cfg.casePixels);
    prefs.putUChar("caseBri",cfg.caseBrightness);
    prefs.putUChar("caseSec",cfg.caseGoalSeconds);
    StaticJsonDocument<96> response;
    response["ok"]=true;
    response["enabled"]=cfg.caseLedsEnabled;
    sendJson(response);
  });
  server.on("/api/case-led/test", HTTP_POST, []() {
    if(!authorized())return;
    if(!cfg.caseLedsEnabled || !caseLedDriver.enabled())
      return errorResponse("Case LEDs disabled; complete wiring first",409);
    StaticJsonDocument<192> request;
    if(deserializeJson(request,server.arg("plain")))
      return errorResponse("Invalid case LED test request");
    const String mode=request["mode"]|String("goal");
    const String side=request["side"]|String("home");
    if(side!="home" && side!="away")
      return errorResponse("Use home or away side",400);
    if(mode!="goal" && mode!="pp" && mode!="start" &&
       mode!="final" && mode!="red" && mode!="green" &&
       mode!="blue" && mode!="off")
      return errorResponse("Invalid case LED diagnostic effect",400);
    const S3CaseLedPolicy::Side position=side=="home"
        ?S3CaseLedPolicy::Side::HOME:S3CaseLedPolicy::Side::AWAY;
    if(mode=="off"){
      caseLedFx.stop();
    }else{
      const Game *preview=testMatchup();
      const String guessed=preview
          ?(side=="home"?preview->home:preview->away)
          :(side=="home"?String("UTA"):String("BOS"));
      String team=request["team"]|String("");
      if(team.isEmpty())team=guessed;
      uint32_t color=0;
      if(mode=="red")color=0xFF0000;
      else if(mode=="green")color=0x00FF00;
      else if(mode=="blue")color=0x0000FF;
      else if(!TeamPalette::find(team.c_str(),color))
        return errorResponse("Unknown NHL team for case LED preview",400);
      const S3CaseLedPolicy::Event event=
          mode=="goal"?S3CaseLedPolicy::Event::GOAL:
          mode=="pp"?S3CaseLedPolicy::Event::TEST_PP:
          mode=="start"?S3CaseLedPolicy::Event::GAME_START:
          mode=="final"?S3CaseLedPolicy::Event::FINAL:
                        S3CaseLedPolicy::Event::TEST;
      caseLedFx.trigger(position,event,color,millis(),
                       mode=="goal"?uint32_t(cfg.caseGoalSeconds)*1000UL:0);
    }
    caseLedDriver.tick(millis(),caseLedFx,cfg.casePixels,cfg.caseBrightness);
    StaticJsonDocument<128> response;
    response["ok"]=true;
    response["mode"]=mode;
    response["side"]=side;
    response["isolated"]=true;
    sendJson(response);
  });
#endif
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

  // Philips Hue: all routes are authenticated; no key ever reaches the web UI.
  server.on("/api/hue/config",HTTP_GET,[](){
    if(!authorized())return;
    StaticJsonDocument<950> doc;
    doc["ip"]=hue.config.ip;
    doc["paired"]=hue.config.key.length()>0;
    doc["enabled"]=hue.config.enabled;
    doc["events"]=hue.config.events;
    doc["brightness"]=hue.config.brightness;
    doc["status"]=HueBridge::label(hue.result);
    doc["active"]=!hue.idle();
    doc["error"]=hue.lastError;
    JsonArray lights=doc.createNestedArray("lights");
    for(const auto &id:hue.config.lights)if(id.length())lights.add(id);
    sendJson(doc);
  });
  server.on("/api/hue/discover",HTTP_GET,[](){
    if(!authorized())return;
    StaticJsonDocument<950> doc;
    JsonArray bridges=doc.createNestedArray("bridges");
    if(WiFi.status()==WL_CONNECTED && mdnsStarted){
      const int n=MDNS.queryService("hue","tcp");
      for(int i=0;i<n && i<6;++i){
        const String ip=MDNS.IP(i).toString();
        if(!HueBridge::Engine::validIp(ip))continue;
        JsonObject item=bridges.createNestedObject();
        item["ip"]=ip;
        item["name"]=MDNS.hostname(i);
      }
    }
    doc["manualSupported"]=true;
    sendJson(doc);
  });
  server.on("/api/hue/pair",HTTP_POST,[](){
    if(!authorized())return;
    if(server.arg("plain").length()>180)return errorResponse("Request too large");
    StaticJsonDocument<192> input;
    if(deserializeJson(input,server.arg("plain")))return errorResponse("Invalid JSON");
    String ip=input["ip"]|String("");ip.trim();
    if(!HueBridge::Engine::validIp(ip))
      return errorResponse("Bridge must be on the same Wi-Fi subnet",400);
    if(!hue.idle())return errorResponse("Stop Hue before re-pairing",409);
    String token;
    if(!hue.pair(ip,token)){
      StaticJsonDocument<260> response;
      response["ok"]=false;
      response["error"]=hue.lastError;
      return sendJson(response,409);
    }
    if(ip!=hue.config.ip){
      for(auto &id:hue.config.lights)id="";
      hue.config.enabled=false;
    }
    hue.config.ip=ip;hue.config.key=token;
    saveSettings();
    StaticJsonDocument<120> result;result["ok"]=true;result["paired"]=true;
    sendJson(result);
  });
  server.on("/api/hue/lights",HTTP_GET,[](){
    if(!authorized())return;
    if(hue.config.key.isEmpty())return errorResponse("Pair Hue Bridge first",409);
    String raw;
    if(!hue.list(raw))return errorResponse(hue.lastError.c_str(),502);
    DynamicJsonDocument parsed(20000);
    if(deserializeJson(parsed,raw))return errorResponse("Invalid Hue light response",502);
    DynamicJsonDocument result(5300);
    JsonArray list=result.createNestedArray("lights");
    JsonArray data=parsed["data"].as<JsonArray>();
    if(data.isNull())return errorResponse("Hue response contains no lights",502);
    for(JsonObject light:data){
      if(list.size()>=32)break;
      String id=light["id"]|String("");
      if(!HuePolicy::validResourceId(id.c_str()))continue;
      JsonObject entry=list.createNestedObject();
      entry["id"]=id;
      entry["name"]=light["metadata"]["name"]|String("Hue light");
      entry["color"]=!light["color"].isNull();
      entry["dimmable"]=!light["dimming"].isNull();
    }
    if(result.overflowed())return errorResponse("Too many Hue lights",503);
    sendJson(result);
  });
  server.on("/api/hue/config",HTTP_POST,[](){
    if(!authorized())return;
    if(!hue.idle())return errorResponse("Stop Hue before editing light settings",409);
    if(server.arg("plain").length()>950)return errorResponse("Hue config too large");
    StaticJsonDocument<1050> input;
    if(deserializeJson(input,server.arg("plain")))return errorResponse("Invalid JSON");
    JsonArray arr=input["lights"].as<JsonArray>();
    if(arr.isNull()||arr.size()>HuePolicy::kMaxLights)
      return errorResponse("Select no more than four lights");
    String ids[HuePolicy::kMaxLights];
    for(uint8_t i=0;i<arr.size();++i){
      ids[i]=arr[i].as<String>();
      if(!HuePolicy::validResourceId(ids[i].c_str()))
        return errorResponse("Invalid Hue light ID");
      for(uint8_t j=0;j<i;++j)
        if(ids[i]==ids[j])return errorResponse("Duplicate Hue light ID");
    }
    const bool enabled=input["enabled"]|false;
    if(enabled&&(!hue.config.key.length()||arr.size()==0))
      return errorResponse("Pair Hue and choose a light before enabling");
    hue.config.enabled=enabled;
    hue.config.events=constrain((int)(input["events"]|15),0,15);
    hue.config.brightness=constrain((int)(input["brightness"]|75),5,100);
    for(uint8_t i=0;i<HuePolicy::kMaxLights;++i)
      hue.config.lights[i]=ids[i];
    saveSettings();
    StaticJsonDocument<80> response;response["ok"]=true;sendJson(response);
  });
  server.on("/api/hue/test",HTTP_POST,[](){
    if(!authorized())return;
    if(server.arg("plain").length()>150)return errorResponse("Request too large");
    StaticJsonDocument<180> input;
    if(deserializeJson(input,server.arg("plain")))return errorResponse("Invalid JSON");
    const String requested=input["event"]|String("goal");
    if(requested=="stop"){
      hueTestUntil=0;
      hue.stop();
      StaticJsonDocument<80> ok;ok["ok"]=true;sendJson(ok);return;
    }
    CelebrationEngine::Event event=requested=="goal"?CelebrationEngine::Event::GOAL:
      requested=="pp"?CelebrationEngine::Event::POWER_PLAY:
      requested=="start"?CelebrationEngine::Event::GAME_START:
      requested=="win"?CelebrationEngine::Event::FINAL_WIN:
      requested=="loss"?CelebrationEngine::Event::FINAL_LOSS:
      CelebrationEngine::Event::NONE;
    if(event==CelebrationEngine::Event::NONE)
      return errorResponse("Unknown Hue test event");
    if(!hue.ready())return errorResponse("Enable and pair selected Hue lights",409);
    const String side=input["side"]|String("home");
    Game *g=testMatchup();
    String team=g?(side=="away"?g->away:g->home):String("UTA");
    GameEventLog::Type type=event==CelebrationEngine::Event::GOAL?
      GameEventLog::Type::PREVIEW_GOAL:event==CelebrationEngine::Event::POWER_PLAY?
      GameEventLog::Type::PREVIEW_POWER_PLAY:event==CelebrationEngine::Event::GAME_START?
      GameEventLog::Type::PREVIEW_START:event==CelebrationEngine::Event::FINAL_WIN?
      GameEventLog::Type::PREVIEW_WIN:GameEventLog::Type::PREVIEW_LOSS;
    GameEventLog::Entry &row=addPreviewEvent(g,type,team);
    hue.begin(event,teamRgb(team),row.sequence,0);
    hueTestUntil=event==CelebrationEngine::Event::POWER_PLAY?
      millis()+9000UL:0;
    row.hue=GameEventLog::Wled::PENDING;
    StaticJsonDocument<100> response;
    response["ok"]=true;response["queued"]=true;
    sendJson(response);
  });
  server.on("/api/wled/discovery", HTTP_GET, []() {
    if(!authorized())return;
    DynamicJsonDocument doc(2900);
    doc["scanning"]=wledScanning;
    doc["complete"]=wledScanFinished;
    doc["candidateCount"]=wledScanTotal;
    doc["checked"]=wledScanNext;
    doc["found"]=wledScanFound;
    doc["maxSaved"]=MAX_WLED;
    doc["error"]=wledScanError;
    JsonArray arr=doc.createNestedArray("devices");
    for(uint8_t i=0;i<wledScanNext &&
        i<WledDiscoveryPolicy::kMaxCandidates;++i){
      const WledScanEntry &p=wledScan[i];
      JsonObject x=arr.createNestedObject();
      x["ip"]=p.ip;x["name"]=p.name;x["version"]=p.version;
      x["online"]=p.online;x["leds"]=p.ledCount;
      x["maxSegments"]=p.maxSegments;
      x["savedSlot"]=-1;
      for(uint8_t j=0;j<MAX_WLED;++j)
        if(cfg.wled[j]==p.ip){x["savedSlot"]=j;break;}
    }
    if(doc.overflowed())return errorResponse("Discovery list too large",503);
    sendJson(doc);
  });
  server.on("/api/wled/discover", HTTP_POST, []() {
    if(!authorized())return;
    if(WiFi.status()!=WL_CONNECTED)
      return errorResponse("Connect GameDay to Wi-Fi before discovering WLED",503);
    if(wledScanning)return errorResponse("Discovery already running",409);
    if(wledScanFinished &&
       uint32_t(millis()-wledLastScanStarted)<WledDiscoveryPolicy::kScanCooldownMs)
      return errorResponse("Wait 15 seconds before scanning again",429);
    wledScanTotal=wledScanNext=wledScanFound=0;
    wledScanError="";
    wledScanStarted=wledLastScanStarted=millis();
    wledScanFinished=false;
    wledScanPendingMdns=true;
    wledScanning=true;
    StaticJsonDocument<96> response;
    response["ok"]=true;response["started"]=true;
    sendJson(response);
  });
  server.on("/api/wled/probe", HTTP_POST, []() {
    if(!authorized())return;
    if(wledScanning)return errorResponse("Finish device discovery first",409);
    StaticJsonDocument<160> request;
    if(deserializeJson(request,server.arg("plain")))
      return errorResponse("Invalid WLED probe JSON",400);
    const String ip=request["ip"]|String("");
    IPAddress addr;
    if(!addr.fromString(ip)||!validWledLocalIp(addr))
      return errorResponse("Use a WLED IPv4 on the GameDay Wi-Fi subnet",400);
    WledScanEntry info;
    if(!probeWledInfo(ip,info))
      return errorResponse("No compatible WLED JSON response at that IP",404);
    StaticJsonDocument<300> response;
    response["ok"]=true;
    response["ip"]=info.ip;response["name"]=info.name;
    response["version"]=info.version;response["leds"]=info.ledCount;
    response["maxSegments"]=info.maxSegments;
    sendJson(response);
  });
  server.on("/api/wled/segments", HTTP_GET, []() {
    if(!authorized())return;
    const String index=server.arg("slot");
    if(index.length()!=1 || index[0]<'0' || index[0]>'7')
      return errorResponse("Specify a WLED slot 0–7",400);
    const uint8_t slot=uint8_t(index[0]-'0');
    if(cfg.wled[slot].isEmpty())return errorResponse("Save a WLED IP first",400);
    const String snapshot=getWledState(cfg.wled[slot]);
    if(snapshot.isEmpty())return errorResponse("WLED controller offline or JSON unavailable",503);
    DynamicJsonDocument filter(640);
    JsonObject shape=filter["seg"][0].to<JsonObject>();
    for(const char *k:{"id","start","stop","len","n","on","sel","fx","bri"})
      shape[k]=true;
    filter["on"]=true;
    DynamicJsonDocument state(4608);
    if(deserializeJson(state,snapshot,DeserializationOption::Filter(filter)))
      return errorResponse("Unsupported WLED segment state",503);
    DynamicJsonDocument response(3800);
    response["ok"]=true;
    response["slot"]=slot;
    response["ip"]=cfg.wled[slot];
    response["name"]=cfg.wledName[slot];
    response["controllerOn"]=state["on"]|false;
    response["mappingEnabled"]=cfg.wledZoneEnabled[slot];
    response["maxManagedSegments"]=WledZonePolicy::kMaxSegments;
    JsonArray rows=response.createNestedArray("segments");
    JsonArray segments=state["seg"].as<JsonArray>();
    for(uint8_t i=0;i<segments.size() &&
        i<WledZonePolicy::kMaxSegments;++i){
      JsonObject seg=segments[i];
      const int id=seg["id"]|(int)i;
      const int begin=seg["start"]|0;
      int end=seg["stop"]|0;
      if(end<=begin)end=begin+(seg["len"]|0);
      if(!WledZonePolicy::validSegment(id)||end<=begin)continue;
      JsonObject row=rows.createNestedObject();
      row["id"]=id;row["start"]=begin;row["stop"]=end;
      row["name"]=String(seg["n"]|String("Segment "+String(id)));
      row["enabled"]=seg["on"]|true;
      row["selected"]=seg["sel"]|false;
      row["role"]=uint8_t(WledZonePolicy::role(cfg.wledZoneMap[slot],uint8_t(id)));
    }
    response["count"]=rows.size();
    response["truncated"]=segments.size()>WledZonePolicy::kMaxSegments;
    if(response.overflowed())
      return errorResponse("Segment inventory exceeds device memory",503);
    sendJson(response);
  });
  server.on("/api/wled/segment-config", HTTP_POST, []() {
    if(!authorized())return;
    const String raw=server.arg("plain");
    if(raw.length()>1600)return errorResponse("Segment mapping request too large",400);
    DynamicJsonDocument request(1900);
    if(deserializeJson(request,raw))
      return errorResponse("Invalid segment mapping JSON",400);
    if(!request["slot"].is<int>() || !request["enabled"].is<bool>() ||
       !request["roles"].is<JsonArray>())
      return errorResponse("Supply slot, enabled, and roles array",400);
    const int slot=request["slot"].as<int>();
    if(!WledDiscoveryPolicy::validSlot(slot) || cfg.wled[slot].isEmpty())
      return errorResponse("Configure a saved WLED controller first",400);
    if(restoring[slot] || wledActiveEvent!=CelebrationEngine::Event::NONE)
      return errorResponse("Stop celebration and restore lights before editing zones",409);
    const bool enabled=request["enabled"].as<bool>();
    JsonArray roles=request["roles"].as<JsonArray>();
    if(roles.size()>WledZonePolicy::kMaxSegments)
      return errorResponse("Maximum 16 segment roles",400);
    uint32_t packed=0;
    uint16_t used=0;
    bool assigned=false;
    for(JsonVariant item:roles){
      if(!item["id"].is<int>()||!item["role"].is<int>())
        return errorResponse("Each role needs integer id and role",400);
      const int id=item["id"].as<int>(),role=item["role"].as<int>();
      if(!WledZonePolicy::validSegment(id) ||
         !WledZonePolicy::validRole(role) || (used & (1U<<id)))
        return errorResponse("Invalid or repeated segment role",400);
      used|=uint16_t(1U<<id);
      packed=WledZonePolicy::assign(packed,uint8_t(id),
                                   WledZonePolicy::Role(role));
      if(role)assigned=true;
    }
    if(enabled&&!assigned)
      return errorResponse("Assign at least one segment before enabling zone control",400);
    cfg.wledZoneMap[slot]=packed;
    cfg.wledZoneEnabled[slot]=enabled;
    if(!enabled && cfg.wledDdpEnabled[slot]){
      cfg.wledDdpEnabled[slot]=false;
      prefs.putBool(("ddpOn"+String(slot)).c_str(),false);
      wledDdp[slot].status="Disabled: segment mapping off";
    }
    prefs.putUInt(("zoneMap"+String(slot)).c_str(),packed);
    prefs.putBool(("zoneOn"+String(slot)).c_str(),enabled);
    StaticJsonDocument<144> response;
    response["ok"]=true;response["slot"]=slot;
    response["enabled"]=enabled;response["assigned"]=__builtin_popcount(packed);
    sendJson(response);
  });
  server.on("/api/wled/segment-test", HTTP_POST, []() {
    if(!authorized())return;
    StaticJsonDocument<128> request;
    if(deserializeJson(request,server.arg("plain")) ||
       !request["slot"].is<int>()||!request["id"].is<int>())
      return errorResponse("Supply integer slot and segment ID",400);
    const int slot=request["slot"].as<int>();
    const int id=request["id"].as<int>();
    if(!WledDiscoveryPolicy::validSlot(slot)||
       !WledZonePolicy::validSegment(id)||cfg.wled[slot].isEmpty())
      return errorResponse("Invalid saved controller or segment",400);
    if(restoring[slot] || wledActiveEvent!=CelebrationEngine::Event::NONE)
      return errorResponse("Finish WLED celebration/restore before segment test",409);
    const String original=getWledState(cfg.wled[slot]);
    if(original.isEmpty())
      return errorResponse("Cannot snapshot WLED before identifying segment",503);
    DynamicJsonDocument filter(512);
    filter["on"]=true;
    JsonObject shape=filter["seg"][0].to<JsonObject>();
    for(const char *k:{"id","start","stop","len"})shape[k]=true;
    DynamicJsonDocument state(3072);
    if(deserializeJson(state,original,DeserializationOption::Filter(filter)))
      return errorResponse("Cannot parse WLED segment list",503);
    if(!(state["on"]|false))
      return errorResponse("Switch this WLED device on first; segment preview will not wake unrelated LEDs",409);
    bool exists=false;
    JsonArray segs=state["seg"].as<JsonArray>();
    for(uint8_t i=0;i<segs.size() && i<WledZonePolicy::kMaxSegments;++i){
      JsonObject row=segs[i];
      const int sid=row["id"]|(int)i;
      const int begin=row["start"]|0;
      const int end=row["stop"]|0;
      if(sid==id && (end>begin||(row["len"]|0)>0)){exists=true;break;}
    }
    if(!exists)return errorResponse("Segment not present in current WLED layout",404);
    DynamicJsonDocument command(256);
    command["udpn"]["nn"]=true;command["transition"]=0;
    JsonObject segment=command["seg"][0].to<JsonObject>();
    segment["id"]=id;segment["on"]=true;segment["bri"]=110;
    segment["fx"]=0;segment["pal"]=0;
    JsonArray cols=segment.createNestedArray("col");
    JsonArray primary=cols.createNestedArray();
    primary.add(255);primary.add(255);primary.add(255);
    String body;serializeJson(command,body);
    savedState[slot]=original;
    savedWledIp[slot]=cfg.wled[slot];
    wledRestoreMapped[slot]=true; // preview always restores targeted segment
    wledTouchedSegments[slot]=uint16_t(1U<<id);
    wledEventOwner[slot]=0; // manual identify is not an NHL goal
    restoring[slot]=true;
    restoreRetries[slot]=0;
    restoreAt[slot]=millis()+1800UL;
    const bool applied=postWled(cfg.wled[slot],body);
    if(!applied)restoreAt[slot]=millis();
    wledDeviceResult[slot]=applied?"Identifying segment":"Segment test failed; restoring";
    StaticJsonDocument<128> response;
    response["ok"]=applied;
    response["slot"]=slot;response["id"]=id;
    response["durationMs"]=1800;
    sendJson(response);
  });
  // 8C scene builder routes: authenticated, no NHL observation/follow changes.
  server.on("/api/wled/scenes", HTTP_GET, []() {
    if(!authorized())return;
    const String input=server.arg("slot");
    if(input.length()!=1 || input[0]<'0' || input[0]>'7')
      return errorResponse("Specify a WLED slot 0–7",400);
    const uint8_t slot=uint8_t(input[0]-'0');
    if(cfg.wled[slot].isEmpty())return errorResponse("Save WLED controller first",400);
    DynamicJsonDocument response(4400);
    response["ok"]=true;response["slot"]=slot;
    response["name"]=cfg.wledName[slot];
    response["mappingEnabled"]=cfg.wledZoneEnabled[slot];
    response["wledIp"]=cfg.wled[slot];
    response["globalGoalSeconds"]=cfg.seconds;
    JsonArray options=response.createNestedArray("effects");
    for(uint8_t j=0;j<WledScenePolicy::kAllowedFxCount;++j)
      options.add(WledScenePolicy::kAllowedFx[j]);
    JsonArray rows=response.createNestedArray("scenes");
    for(uint8_t event=1;event<=WledScenePolicy::kEvents;++event){
      for(uint8_t zone=1;zone<=WledScenePolicy::kZones;++zone){
        const int idx=WledScenePolicy::index(event,zone);
        const WledScenePolicy::Scene &sc=cfg.wledScenes[slot][idx];
        JsonObject obj=rows.createNestedObject();
        obj["event"]=event;obj["zone"]=zone;
        obj["custom"]=sc.fx!=WledScenePolicy::kInherit;
        obj["fx"]=sc.fx==WledScenePolicy::kInherit?0:sc.fx;
        obj["speed"]=sc.speed;obj["intensity"]=sc.intensity;
        obj["brightness"]=sc.brightness;
        obj["colorMode"]=sc.colorMode;
        char hex[8];
        snprintf(hex,sizeof(hex),"#%02X%02X%02X",sc.r,sc.g,sc.b);
        obj["hex"]=hex;obj["duration"]=sc.duration;
      }
    }
    if(response.overflowed())return errorResponse("Scene list memory unavailable",503);
    sendJson(response);
  });
  server.on("/api/wled/scenes", HTTP_POST, []() {
    if(!authorized())return;
    const String body=server.arg("plain");
    if(body.length()>3600)return errorResponse("Scene request too large",400);
    DynamicJsonDocument request(4800);
    if(deserializeJson(request,body) || !request["slot"].is<int>() ||
       !request["scenes"].is<JsonArray>())
      return errorResponse("Expected slot and scenes array",400);
    const int slot=request["slot"].as<int>();
    if(!WledDiscoveryPolicy::validSlot(slot) || cfg.wled[slot].isEmpty())
      return errorResponse("Save the WLED controller before editing scenes",400);
    if(wledActiveEvent!=CelebrationEngine::Event::NONE || restoring[slot])
      return errorResponse("Finish active WLED effects and restore before saving scenes",409);
    JsonArray rows=request["scenes"].as<JsonArray>();
    if(rows.size()!=WledScenePolicy::kSceneCount)
      return errorResponse("Provide all 15 event-zone templates",400);
    WledScenePolicy::Scene next[WledScenePolicy::kSceneCount];
    uint16_t seen=0;
    for(JsonVariant item:rows){
      if(!item["event"].is<int>() || !item["zone"].is<int>() ||
         !item["custom"].is<bool>())
        return errorResponse("Each template requires event, zone and custom",400);
      const int event=item["event"].as<int>(),zone=item["zone"].as<int>();
      const int idx=WledScenePolicy::index(uint8_t(event),uint8_t(zone));
      if(event<1||event>WledScenePolicy::kEvents||
         zone<1||zone>WledScenePolicy::kZones||
         idx<0||(seen&(uint16_t(1U)<<idx)))
        return errorResponse("Invalid or repeated event-zone template",400);
      seen|=uint16_t(1U)<<idx;
      if(!item["custom"].as<bool>())continue; // exact Phase 8B inheritance
      for(const char *field:{"fx","speed","intensity","brightness",
                              "colorMode","duration"})
        if(!item[field].is<int>())
          return errorResponse("Template parameters must be integers",400);
      const int fx=item["fx"].as<int>(),speed=item["speed"].as<int>();
      const int intensity=item["intensity"].as<int>();
      const int brightness=item["brightness"].as<int>();
      const int mode=item["colorMode"].as<int>();
      const int duration=item["duration"].as<int>();
      if(fx<0||fx>254||speed<0||speed>255||
         intensity<0||intensity>255||brightness<1||brightness>255||
         mode<0||mode>3||duration<0||duration>30)
        return errorResponse("Scene parameter outside allowed range",400);
      const String hex=item["hex"]|String("");
      if(hex.length()!=7 || hex[0]!='#')
        return errorResponse("Scene color must be #RRGGBB",400);
      uint32_t rgb=0;
      for(uint8_t k=1;k<7;++k){
        const char c=hex[k];
        const uint8_t digit=c>='0'&&c<='9'?c-'0':
          c>='a'&&c<='f'?c-'a'+10:
          c>='A'&&c<='F'?c-'A'+10:255;
        if(digit>15)return errorResponse("Invalid hexadecimal scene color",400);
        rgb=(rgb<<4)|digit;
      }
      WledScenePolicy::Scene &sc=next[idx];
      sc.fx=uint8_t(fx);sc.speed=uint8_t(speed);
      sc.intensity=uint8_t(intensity);sc.brightness=uint8_t(brightness);
      sc.colorMode=uint8_t(mode);
      sc.r=uint8_t(rgb>>16);sc.g=uint8_t(rgb>>8);sc.b=uint8_t(rgb);
      sc.duration=uint8_t(duration);
      if(!WledScenePolicy::valid(sc,uint8_t(event)))
        return errorResponse("Unsupported effect or scene duration",400);
    }
    if(seen!=uint16_t((1U<<WledScenePolicy::kSceneCount)-1U))
      return errorResponse("Every event-zone pair must be supplied",400);
    // Persist one complete CRC-protected blob; never partially apply a
    // multi-zone edit to a live GameDay celebration.
    uint8_t blob[WledScenePolicy::kBlobBytes]={};
    WledScenePolicy::encode(next,blob);
    const String key="scene"+String(slot);
    if(prefs.putBytes(key.c_str(),blob,sizeof(blob))!=sizeof(blob))
      return errorResponse("Scene library flash write failed",503);
    for(uint8_t i=0;i<WledScenePolicy::kSceneCount;++i)
      cfg.wledScenes[slot][i]=next[i];
    StaticJsonDocument<112> response;
    response["ok"]=true;response["slot"]=slot;response["saved"]=true;
    sendJson(response);
  });
  server.on("/api/wled/scene-preview", HTTP_POST, []() {
    if(!authorized())return;
    StaticJsonDocument<384> request;
    const String raw=server.arg("plain");
    if(raw.length()>350 || deserializeJson(request,raw) ||
       !request["slot"].is<int>() || !request["event"].is<int>() ||
       !request["zone"].is<int>())
      return errorResponse("Invalid scene preview request",400);
    const int slot=request["slot"].as<int>();
    const int event=request["event"].as<int>(),zone=request["zone"].as<int>();
    if(!WledDiscoveryPolicy::validSlot(slot) ||
       !WledZonePolicy::validRole(zone) || zone==0 ||
       event<1||event>WledScenePolicy::kEvents ||
       cfg.wled[slot].isEmpty() || !cfg.wledZoneEnabled[slot])
      return errorResponse("Enable segment zones on a saved WLED first",400);
    if(wledActiveEvent!=CelebrationEngine::Event::NONE || restoring[slot])
      return errorResponse("Stop active WLED celebration/restore before preview",409);
    const String original=getWledState(cfg.wled[slot]);
    if(original.isEmpty())return errorResponse("Cannot snapshot WLED before preview",503);
    DynamicJsonDocument filter(480);
    filter["on"]=true;
    JsonObject shape=filter["seg"][0].to<JsonObject>();
    for(const char *k:{"id","start","stop","len"})shape[k]=true;
    DynamicJsonDocument state(3072);
    if(deserializeJson(state,original,DeserializationOption::Filter(filter))||
       !(state["on"]|false))
      return errorResponse("WLED must be ON and report valid segment state",409);
    const WledZonePolicy::Role role=WledZonePolicy::Role(zone);
    const WledScenePolicy::Scene &scene=wledScene(uint8_t(slot),
                    CelebrationEngine::Event(event),role);
    const Game *game=testMatchup();
    const uint32_t homeRgb=game?teamRgb(game->home):0x38B4DEUL;
    const uint32_t awayRgb=game?teamRgb(game->away):0xE6A44FUL;
    const uint32_t trigger=zone==int(WledZonePolicy::AWAY)?awayRgb:homeRgb;
    const uint32_t fallback=WledZonePolicy::zoneColor(role,uint8_t(event),
                                      trigger,homeRgb,awayRgb);
    const uint32_t team=role==WledZonePolicy::HOME?homeRgb:
      role==WledZonePolicy::AWAY?awayRgb:trigger;
    const uint32_t rgb=WledScenePolicy::rgb(scene,fallback,team);
    const CelebrationEngine::Effect chosen=WledScenePolicy::effect(
      scene,CelebrationEngine::effect(CelebrationEngine::Event(event),0,
                                    cfg.wledGoalStyle));
    const uint8_t bri=WledScenePolicy::brightness(scene,cfg.brightness);
    DynamicJsonDocument command(2900);
    command["udpn"]["nn"]=true;command["transition"]=0;
    JsonArray targets=command.createNestedArray("seg");
    uint16_t mask=0;
    JsonArray segments=state["seg"].as<JsonArray>();
    for(uint8_t j=0;j<segments.size() &&
        j<WledZonePolicy::kMaxSegments;++j){
      JsonObject seg=segments[j];
      const int id=seg["id"]|(int)j;
      const int first=seg["start"]|0;
      const int end=seg["stop"]|0;
      if(!WledZonePolicy::validSegment(id) ||
         (end<=first && (seg["len"]|0)<=0) ||
         WledZonePolicy::role(cfg.wledZoneMap[slot],uint8_t(id))!=role)
        continue;
      JsonObject out=targets.createNestedObject();
      out["id"]=id;out["on"]=true;out["bri"]=bri;
      out["fx"]=chosen.fx;out["sx"]=chosen.speed;
      out["ix"]=chosen.intensity;out["pal"]=0;
      JsonArray cols=out.createNestedArray("col");
      JsonArray primary=cols.createNestedArray();
      primary.add((rgb>>16)&255);primary.add((rgb>>8)&255);primary.add(rgb&255);
      mask|=uint16_t(1U)<<id;
    }
    if(!mask || command.overflowed())
      return errorResponse("No valid WLED segments assigned to this zone",400);
    String body;serializeJson(command,body);
    savedState[slot]=original;savedWledIp[slot]=cfg.wled[slot];
    wledTouchedSegments[slot]=mask;wledCurrentSceneSegments[slot]=0;
    wledRestoredSceneSegments[slot]=0;wledRestoreMapped[slot]=true;
    wledEventOwner[slot]=0;restoring[slot]=true;
    restoreRetries[slot]=0;
    // Manual rehearsal is deliberately short even if the live scene
    // duration is 30 seconds; use the real saved scene effect/color.
    restoreAt[slot]=millis()+3000UL;
    const bool applied=postWled(cfg.wled[slot],body);
    if(!applied)restoreAt[slot]=millis();
    wledDeviceResult[slot]=applied?"Scene preview":"Scene preview failed; restoring";
    StaticJsonDocument<120> response;
    response["ok"]=applied;response["slot"]=slot;
    response["event"]=event;response["zone"]=zone;
    response["restoreMs"]=3000;
    sendJson(response);
  });
  // 8D DDP only streams FULL, dedicated RGB controllers. A configuration
  // change cannot silently opt in another saved WLED device.
  server.on("/api/wled/ddp", HTTP_GET, []() {
    if(!authorized())return;
    const String input=server.arg("slot");
    if(input.length()!=1 || input[0]<'0' || input[0]>'7')
      return errorResponse("Select a WLED slot 0–7",400);
    const uint8_t slot=uint8_t(input[0]-'0');
    StaticJsonDocument<512> result;
    result["slot"]=slot;
    result["configured"]=!cfg.wled[slot].isEmpty();
    result["enabled"]=cfg.wledDdpEnabled[slot];
    result["fps"]=cfg.wledDdpFps[slot];
    result["running"]=wledDdp[slot].active;
    result["manual"]=wledDdp[slot].manual;
    result["frames"]=wledDdp[slot].frames;
    result["packets"]=wledDdp[slot].packets;
    result["pixels"]=wledDdp[slot].layout.pixels;
    result["chunksPerFrame"]=WledDdpPolicy::packetCount(wledDdp[slot].layout.pixels);
    result["pixelsPerChunk"]=WledDdpPolicy::kChunkPixels;
    result["mode"]=wledDdp[slot].status;
    result["fullControllerRequired"]=true;
    result["maxPixels"]=WledDdpPolicy::kMaxPixels;
    result["maxStreams"]=WledDdpPolicy::kMaxConcurrent;
    result["ddpPort"]=WledDdpPolicy::kPort;
    result["livePPUsesJson"]=true;
    sendJson(result);
  });
  // Phase 8E Quick Setup: informative preflight, never changes NVS or LEDs.
  server.on("/api/wled/ddp/setup", HTTP_GET, []() {
    if(!authorized())return;
    const String input=server.arg("slot");
    if(input.length()!=1||input[0]<'0'||input[0]>'7')
      return errorResponse("Select a saved WLED controller",400);
    const uint8_t slot=uint8_t(input[0]-'0');
    StaticJsonDocument<640> result;
    result["slot"]=slot;
    result["name"]=cfg.wledName[slot];
    result["ip"]=cfg.wled[slot];
    result["configured"]=!cfg.wled[slot].isEmpty();
    result["enabled"]=cfg.wledEnabled[slot];
    result["mappingEnabled"]=cfg.wledZoneEnabled[slot];
    result["ddpEnabled"]=cfg.wledDdpEnabled[slot];
    result["fps"]=cfg.wledDdpFps[slot];
    result["maxPixels"]=WledDdpPolicy::kMaxPixels;
    result["ready"]=false;
    String reason="Save a controller address first";
    WledScanEntry info;
    if(cfg.wled[slot].isEmpty()){
      // No network operations for unsaved controllers.
    }else if(restoring[slot]||wledDdp[slot].active||
             wledActiveEvent!=CelebrationEngine::Event::NONE){
      reason="Celebration active; stop and restore before setup";
    }else if(!probeWledInfo(cfg.wled[slot],info)){
      reason="WLED not reachable. Check Wi-Fi and its IP address";
    }else{
      result["online"]=true;
      result["pixels"]=info.ledCount;
      result["chunks"]=WledDdpPolicy::packetCount(info.ledCount);
      result["rgbw"]=info.rgbw;
      const String snapshot=getWledState(cfg.wled[slot]);
      WledDdpPolicy::Layout layout;
      if(info.live){
        reason="Another realtime stream is active. Stop it in WLED first";
      }else if(info.rgbw){
        reason="RGBW LEDs are not supported by GameDay DDP; use native WLED";
      }else if(info.ledCount>WledDdpPolicy::kMaxPixels){
        reason="Strip exceeds 4096 RGB LEDs; use native WLED";
      }else if(snapshot.isEmpty()){
        reason="Cannot read WLED state. Check network connection";
      }else if(!readWledDdpLayout(snapshot,info.ledCount,0,
                                  WledZonePolicy::NEUTRAL,layout,reason)){
        // Geometry helper supplies an actionable explanation.
      }else{
        result["segments"]=layout.count;
        const WledDdpPolicy::Fail resultCode=WledDdpPolicy::validate(
          layout,uint8_t(CelebrationEngine::Event::GAME_START),
          WledZonePolicy::kUnknownSide);
        if(resultCode!=WledDdpPolicy::OK){
          reason=String(WledDdpPolicy::reason(resultCode))+
                 ". Fix WLED segments so all LEDs are covered exactly once";
        }else if(activeDdpCount()>=WledDdpPolicy::kMaxConcurrent){
          reason="Two DDP streams are already in use. Try again later";
        }else{
          result["ready"]=true;
          reason="Compatible. Choose a team zone and confirm to enable";
        }
      }
    }
    result["reason"]=reason;
    sendJson(result);
  });
  // One confirmed operation replaces manual segment-role save, device enable
  // and DDP opt-in. WLED segment bounds/effects are never written or altered.
  server.on("/api/wled/ddp/quick-setup", HTTP_POST, []() {
    if(!authorized())return;
    StaticJsonDocument<256> request;
    const String body=server.arg("plain");
    if(body.length()>256||deserializeJson(request,body)||
       !request["slot"].is<int>()||!request["mode"].is<int>()||
       !request["fps"].is<int>())
      return errorResponse("Choose controller, zone mode and frame rate",400);
    const int slot=request["slot"].as<int>();
    const int mode=request["mode"].as<int>();
    const int fps=request["fps"].as<int>();
    if(!WledDiscoveryPolicy::validSlot(slot)||mode<0||mode>3||
       !WledDdpPolicy::validFps(fps)||cfg.wled[slot].isEmpty())
      return errorResponse("Invalid WLED controller or Quick Setup choice",400);
    if(!(request["exclusiveConfirmed"]|false))
      return errorResponse("Confirm that this WLED controller is dedicated to GameDay",400);
    if(restoring[slot]||wledDdp[slot].active||
       wledActiveEvent!=CelebrationEngine::Event::NONE)
      return errorResponse("Stop and restore active celebrations before configuring",409);
    if(mode==0&&!cfg.wledZoneEnabled[slot])
      return errorResponse("Existing zones are not configured. Choose Both, Home or Away",400);
    WledScanEntry info;
    if(!probeWledInfo(cfg.wled[slot],info))
      return errorResponse("Cannot reach WLED. Check Wi-Fi or IP",503);
    if(info.live||info.rgbw||info.ledCount>WledDdpPolicy::kMaxPixels)
      return errorResponse(info.live?"Stop other realtime streaming in WLED":
          info.rgbw?"RGBW is not supported by DDP":
                    "More than 4096 LEDs; use native JSON",400);
    const String snapshot=getWledState(cfg.wled[slot]);
    if(snapshot.isEmpty())return errorResponse("Cannot read WLED segments",503);
    WledDdpPolicy::Layout layout;
    String reason;
    if(!readWledDdpLayout(snapshot,info.ledCount,cfg.wledZoneMap[slot],
                          uint8_t(mode),layout,reason))
      return errorResponse(reason.c_str(),400);
    const WledDdpPolicy::Fail geometry=WledDdpPolicy::validate(
      layout,uint8_t(CelebrationEngine::Event::GAME_START),
      WledZonePolicy::kUnknownSide);
    if(geometry!=WledDdpPolicy::OK)
      return errorResponse((String(WledDdpPolicy::reason(geometry))+
         ". Check LED coverage and zone assignments").c_str(),400);
    uint32_t newMap=cfg.wledZoneMap[slot];
    if(mode!=0){
      newMap=0;
      for(uint8_t j=0;j<layout.count;++j)
        newMap=WledZonePolicy::assign(newMap,layout.ranges[j].id,
          WledZonePolicy::Role(mode));
    }
    const uint32_t oldMap=cfg.wledZoneMap[slot];
    const bool oldZones=cfg.wledZoneEnabled[slot];
    const bool oldDdp=cfg.wledDdpEnabled[slot];
    const bool oldEnabled=cfg.wledEnabled[slot];
    const uint8_t oldFps=cfg.wledDdpFps[slot];
    cfg.wledZoneMap[slot]=newMap;
    cfg.wledZoneEnabled[slot]=true;
    cfg.wledDdpEnabled[slot]=true;
    cfg.wledEnabled[slot]=true;
    cfg.wledDdpFps[slot]=uint8_t(fps);
    WledDdpSession candidate;
    const bool prepared=prepareWledDdp(uint8_t(slot),
      CelebrationEngine::Event::GAME_START,0x31837AUL,
      WledZonePolicy::kUnknownSide,snapshot,candidate);
    const String failure=wledDdp[slot].status;
    if(!prepared){
      cfg.wledZoneMap[slot]=oldMap;
      cfg.wledZoneEnabled[slot]=oldZones;
      cfg.wledDdpEnabled[slot]=oldDdp;
      cfg.wledEnabled[slot]=oldEnabled;
      cfg.wledDdpFps[slot]=oldFps;
      return errorResponse((String("DDP check: ")+failure).c_str(),400);
    }
    // Preferences writes are checked; best-effort rollback if flash fails.
    const String suffix=String(slot);
    const bool saved=prefs.putUInt(("zoneMap"+suffix).c_str(),newMap)>0 &&
      prefs.putBool(("zoneOn"+suffix).c_str(),true)>0 &&
      prefs.putUChar(("ddpFps"+suffix).c_str(),uint8_t(fps))>0 &&
      prefs.putBool(("ddpOn"+suffix).c_str(),true)>0 &&
      prefs.putBool(("enable"+suffix).c_str(),true)>0;
    if(!saved){
      cfg.wledZoneMap[slot]=oldMap;
      cfg.wledZoneEnabled[slot]=oldZones;
      cfg.wledDdpEnabled[slot]=oldDdp;
      cfg.wledEnabled[slot]=oldEnabled;
      cfg.wledDdpFps[slot]=oldFps;
      prefs.putUInt(("zoneMap"+suffix).c_str(),oldMap);
      prefs.putBool(("zoneOn"+suffix).c_str(),oldZones);
      prefs.putUChar(("ddpFps"+suffix).c_str(),oldFps);
      prefs.putBool(("ddpOn"+suffix).c_str(),oldDdp);
      prefs.putBool(("enable"+suffix).c_str(),oldEnabled);
      return errorResponse("Could not save DDP settings; existing preferences restored",503);
    }
    wledDdp[slot].status="DDP enabled; ready for a safe 3-second test";
    StaticJsonDocument<192> result;
    result["ok"]=true;result["slot"]=slot;
    result["enabled"]=true;result["ddpEnabled"]=true;
    result["mappingEnabled"]=true;result["fps"]=fps;
    result["pixels"]=info.ledCount;
    result["chunks"]=WledDdpPolicy::packetCount(info.ledCount);
    sendJson(result);
  });
  server.on("/api/wled/ddp/config", HTTP_POST, []() {
    if(!authorized())return;
    StaticJsonDocument<256> request;
    const String body=server.arg("plain");
    if(body.length()>256 || deserializeJson(request,body) ||
       !request["slot"].is<int>() || !request["enabled"].is<bool>() ||
       !request["fps"].is<int>())
      return errorResponse("Specify slot, enabled, and fps",400);
    const int slot=request["slot"].as<int>();
    const int rate=request["fps"].as<int>();
    if(!WledDiscoveryPolicy::validSlot(slot) ||
       !WledDdpPolicy::validFps(rate) || cfg.wled[slot].isEmpty())
      return errorResponse("Select a saved controller and 10–20 fps",400);
    if(restoring[slot] || wledDdp[slot].active ||
       wledActiveEvent!=CelebrationEngine::Event::NONE)
      return errorResponse("Stop WLED effects and wait for restoration",409);
    const bool enabled=request["enabled"].as<bool>();
    if(enabled){
      if(!(request["exclusiveConfirmed"]|false))
        return errorResponse("Confirm this controller is dedicated to DDP and all pixels may be replaced",400);
      if(!cfg.wledZoneEnabled[slot])
        return errorResponse("Enable Home/Away/Neutral segment roles before DDP",400);
      const String snapshot=getWledState(cfg.wled[slot]);
      if(snapshot.isEmpty())return errorResponse("Controller must be online",503);
      const bool prior=cfg.wledDdpEnabled[slot];
      cfg.wledDdpEnabled[slot]=true; // temporary eligibility for validation
      WledDdpSession candidate;
      const bool okay=prepareWledDdp(uint8_t(slot),
          CelebrationEngine::Event::GAME_START,0x31837AUL,
          WledZonePolicy::kUnknownSide,snapshot,candidate);
      cfg.wledDdpEnabled[slot]=prior;
      if(!okay)return errorResponse("DDP requires a WLED-on dedicated RGB strip with complete, non-overlapping zone coverage and matching scene durations",400);
    }
    if(prefs.putBool(("ddpOn"+String(slot)).c_str(),enabled)==0 ||
       prefs.putUChar(("ddpFps"+String(slot)).c_str(),uint8_t(rate))==0)
      return errorResponse("Unable to save DDP preferences",503);
    cfg.wledDdpEnabled[slot]=enabled;
    cfg.wledDdpFps[slot]=uint8_t(rate);
    wledDdp[slot].status=enabled?"DDP opted in (exclusive full controller)":
                                   "Disabled (native JSON)";
    StaticJsonDocument<144> response;
    response["ok"]=true;response["slot"]=slot;
    response["enabled"]=enabled;response["fps"]=rate;
    sendJson(response);
  });
  server.on("/api/wled/ddp/test", HTTP_POST, []() {
    if(!authorized())return;
    StaticJsonDocument<128> request;
    if(deserializeJson(request,server.arg("plain")) ||
       !request["slot"].is<int>())
      return errorResponse("Specify a WLED slot",400);
    const int slot=request["slot"].as<int>();
    if(!WledDiscoveryPolicy::validSlot(slot) ||
       !cfg.wledDdpEnabled[slot] || !cfg.wledEnabled[slot])
      return errorResponse("Enable DDP and WLED participation first",409);
    if(wledActiveEvent!=CelebrationEngine::Event::NONE || restoring[slot] ||
       wledDdp[slot].active)
      return errorResponse("An NHL celebration or restore is active",409);
    if(ESP.getFreeHeap()<45000UL)
      return errorResponse("Memory budget protected: try native JSON",503);
    const String snapshot=getWledState(cfg.wled[slot]);
    if(snapshot.isEmpty())
      return errorResponse("Cannot snapshot WLED before DDP test",503);
    WledDdpSession proposed;
    if(!prepareWledDdp(uint8_t(slot),CelebrationEngine::Event::GAME_START,
          0x31837AUL,WledZonePolicy::kUnknownSide,snapshot,proposed))
      return errorResponse("DDP safety check failed; review the device status",400);
    savedState[slot]=snapshot;
    savedWledIp[slot]=cfg.wled[slot];
    wledDdp[slot]=proposed;
    wledDdp[slot].active=true;
    wledDdp[slot].manual=true;
    wledDdp[slot].started=millis();
    wledDdp[slot].status="DDP 3-second diagnostic";
    wledTouchedSegments[slot]=0;
    for(uint8_t j=0;j<proposed.layout.count;++j)
      wledTouchedSegments[slot]|=
        uint16_t(1U)<<proposed.layout.ranges[j].id;
    wledRestoreMapped[slot]=true;
    wledCurrentSceneSegments[slot]=0;
    wledRestoredSceneSegments[slot]=0;
    wledEventOwner[slot]=0; // diagnostic is not an NHL event
    restoring[slot]=true;restoreRetries[slot]=0;
    restoreAt[slot]=millis()+WledDdpPolicy::kPreviewMs;
    const bool sent=sendWledDdpFrame(uint8_t(slot),millis());
    if(!sent){
      restoreAt[slot]=millis(); // release and restore before next request
      wledDdp[slot].status="DDP test failed; restore requested";
    }
    StaticJsonDocument<144> response;
    response["ok"]=sent;response["slot"]=slot;
    response["durationMs"]=WledDdpPolicy::kPreviewMs;
    response["realNhlEvent"]=false;
    sendJson(response);
  });
  server.on("/api/wled/ddp/stop", HTTP_POST, []() {
    if(!authorized())return;
    // Do not restart the same power play on the following NHL polling tick.
    wledPpManuallyStoppedGameId=selectedGameId;
    suppressArmedPowerPlays();
    wledPriorityQueue.clear();
    scheduleWledRestoreAll();
    StaticJsonDocument<96> response;
    response["ok"]=true;response["restoreRequested"]=true;
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
    else if(type=="so-start")event=CelebrationEngine::Event::SHOOTOUT_START;
    else if(type=="so-goal")event=CelebrationEngine::Event::SHOOTOUT_GOAL;
    else if(type=="so-save")event=CelebrationEngine::Event::SHOOTOUT_SAVE;
    else if(type=="so-miss")event=CelebrationEngine::Event::SHOOTOUT_MISS;
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
        type=="so-start"?GameEventLog::Type::PREVIEW_SHOOTOUT_START:
        type=="so-goal"?GameEventLog::Type::PREVIEW_SHOOTOUT_GOAL:
        type=="so-save"?GameEventLog::Type::PREVIEW_SHOOTOUT_SAVE:
        type=="so-miss"?GameEventLog::Type::PREVIEW_SHOOTOUT_MISS:
                         GameEventLog::Type::PREVIEW_STOP;
    GameEventLog::Entry &previewRow =
        addPreviewEvent(requestedTeam.length() ? nullptr : previewGame,
                        previewType, previewTeam);
    bool wledStarted = false, rearStarted = false, overlayStarted = false;
    bool audioStarted = false;
    if (event == CelebrationEngine::Event::NONE) {
      // The ordinary WLED Stop & restore button must not restart the same PP
      // when its former 30-second suppression interval expires.
      wledPpManuallyStoppedGameId=selectedGameId;
      suppressArmedPowerPlays();
      wledPriorityQueue.clear();
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
      wledStarted = startWledEvent(event,color,previewRow.sequence,
                        side=="home"?WledZonePolicy::kHomeSide:
                                     WledZonePolicy::kAwaySide);
      if (wledStarted && event == CelebrationEngine::Event::POWER_PLAY)
        wledPpTestUntil = millis() + 9000UL;
      else
        wledPpTestUntil = 0;
      // Start the rear flash after potentially slow WLED snapshots/POSTs.
      if (combined && cfg.rearLedEnabled) {
        if(event>=CelebrationEngine::Event::SHOOTOUT_START&&cfg.rearGoal){
          triggerRearShootout(color,event==CelebrationEngine::Event::SHOOTOUT_GOAL);
          rearStarted=true;
        }else if (event == CelebrationEngine::Event::GOAL && cfg.rearGoal) {
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
        event == CelebrationEngine::Event::SHOOTOUT_START?EventOverlay::Kind::SHOOTOUT_START:
        event == CelebrationEngine::Event::SHOOTOUT_GOAL?EventOverlay::Kind::SHOOTOUT_GOAL:
        event == CelebrationEngine::Event::SHOOTOUT_SAVE?EventOverlay::Kind::SHOOTOUT_SAVE:
        event == CelebrationEngine::Event::SHOOTOUT_MISS?EventOverlay::Kind::SHOOTOUT_MISS:
        EventOverlay::Kind::NONE;
    if (overlayKind != EventOverlay::Kind::NONE)
      overlayStarted = queueGameOverlay(requestedTeam.length() ? nullptr : previewGame,
                                       overlayKind, previewTeam, previewRow.sequence);
    // Full rehearsal can exercise the goal horn OR PP announcement, while
    // remaining a PREVIEW event that changes no NHL eligibility or follow state.
    if (combined && event == CelebrationEngine::Event::GOAL) {
      startHorn(previewRow.sequence);
    } else if (combined && event == CelebrationEngine::Event::POWER_PLAY) {
      startPowerPlaySound(previewRow.sequence);
    } else if (combined && event == CelebrationEngine::Event::GAME_START) {
      startGameStartSound(previewRow.sequence);
    } else if (combined && (event == CelebrationEngine::Event::FINAL_WIN ||
                            event == CelebrationEngine::Event::FINAL_LOSS)) {
      startGameFinishSound(previewRow.sequence);
    }
    audioStarted = previewRow.audio == GameEventLog::Audio::STARTED;
    StaticJsonDocument<512> response;
    response["event"] = CelebrationEngine::label(wledActiveEvent);
    response["devices"] = wledEligibleCount(event == CelebrationEngine::Event::NONE
        ? CelebrationEngine::Event::GOAL : event);
    response["wledStarted"] = wledStarted;
    response["rearStarted"] = rearStarted;
    response["overlayStarted"] = overlayStarted;
    response["audioStarted"] = audioStarted;
    response["audioStatus"] = GameEventLog::audioLabel(previewRow.audio);
    response["audioSource"] = previewRow.audioSource;
    response["audioFallback"] = previewRow.audioFallback;
    response["team"] = previewTeam;
    response["hex"] = rgbHex(color);
    response["ok"] = event == CelebrationEngine::Event::NONE ||
        (combined ? (wledStarted || rearStarted || overlayStarted || audioStarted)
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
    // An OS may use alternate/versioned captive detector paths.
    // Do NOT hijack unknown API endpoints or any home-LAN requests.
    if(requestFromSetupAp() && server.method()==HTTP_GET &&
       !server.uri().startsWith("/api/")){
      sendCaptiveSetupRedirect();
      return;
    }
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
  Serial.printf("[S3] NHL embedded logos: %u teams, %ux%u SVG rasters, native sprite renderer\n",
                (unsigned)(sizeof(TEAM_LOGOS)/sizeof(TEAM_LOGOS[0])),
                (unsigned)TEAM_LOGO_SIZE, (unsigned)TEAM_LOGO_SIZE);
  goalGif.beginStorage();
  newsPhotos.begin(goalGif.mounted()); // Same existing LittleFS mount, no format.
  soundStorage.begin(goalGif.mounted()); // one shared LittleFS mount
  s3Audio.setStorageMounted(soundStorage.mounted());
#else
  tft.init();
  newsPhotos.begin(); // Dedicated SPIFFS image cache; NVS unaffected.
#endif
  tft.setRotation(1);
  tft.fillScreen(BG);
  textAt("GlowDeck GameDay", 15, 30, WHITE, 4);
  textAt("Starting local controls...", 15, 68, MUTED);

#ifdef GLOWDECK_WAVESHARE_S3
  touch.begin(); // FT6336 on shared I2C: GPIO8 SDA / GPIO7 SCL.
  // Audio uses this same initialized Wire bus; P7 amp remains OFF at boot.
  // A missing codec fails gracefully without preventing the scoreboard.
  s3Audio.begin();
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
#ifdef GLOWDECK_WAVESHARE_S3
  if (!s3Audio.setVolume(cfg.s3Volume))
    Serial.printf("[S3 Audio] Saved %u%% gain could not be applied; current %u%%\n",
                  unsigned(cfg.s3Volume),unsigned(s3Audio.volume()));
  // Keep the horn usable even if a formerly selected WAV disappeared.
  if(cfg.s3Sound && soundStorage.mounted() &&
     !soundStorage.available(cfg.s3Sound)){
    Serial.println("[S3 WAV] selected clip missing; reset to built-in");
    cfg.s3Sound=0;
    prefs.putUChar("s3Sound",0);
  }
  s3Audio.setSelectedSource(cfg.s3Sound);
  if(cfg.s3PpSound && (!soundStorage.mounted() ||
     !soundStorage.available(cfg.s3PpSound))){
    Serial.println("[S3 WAV] PP clip missing; announcement disabled");
    cfg.s3PpSound=0;
    prefs.putUChar("s3PpSound",0);
  }
  if(cfg.caseLedsEnabled)
    caseLedDriver.enable(true); // Only after previously saved opt-in
#endif
  // Some ILI9341-compatible CYDs boot with visually inverted colors.
  // Runtime setting is stored in NVS, so the user can change it from web or touch.
  tft.invertDisplay(cfg.invertScreen);
#ifdef GLOWDECK_WAVESHARE_S3
  // Respect saved ST7796 inversion; never override a user's corrected colors.
  // Full-native art is RGB565 big-endian, byte-compatible with S3 sprite RAM.
  Serial.printf("[S3 Color] ST7796 invert=%s (saved in NVS) | art=RGB565BE native 480x320\n",
                cfg.invertScreen ? "ON" : "OFF");
#endif
  applyTimezone();

  WiFi.persistent(false);
  WiFi.setSleep(false);
  WiFi.mode(WIFI_AP_STA);
  if (!WiFi.softAP(apSsid.c_str(), apPassword.c_str())) {
    Serial.println("AP start failed");
  } else {
    // Catch-all DNS resolves OS connectivity probes to the setup AP; the
    // HTTP handlers above then present GameDay instead of a dead Internet
    // check. If DNS cannot start the normal manual QR flow still works.
    captiveDns.setErrorReplyCode(DNSReplyCode::NoError);
    captiveDnsReady=captiveDns.start(53, "*", WiFi.softAPIP());
    Serial.printf("[Setup Portal] DNS %s | AP %s | http://192.168.4.1/\n",
                  captiveDnsReady?"ready":"unavailable",apSsid.c_str());
  }
  registerRoutes();
  beginStation();
  setupImprovSerial();
  // On a new factory install show the QR instantly instead of an empty
  // scoreboard. Users who already have credentials keep their normal screen.
  if(WiFi.status()!=WL_CONNECTED){screen=Screen::SETUP;hotspotSetupShown=true;}
  Serial.printf("[Setup] Wi-Fi: %s, initial screen: %s, QR fallback: %s\n",
                cfg.ssid.isEmpty()?"not configured":"saved credentials",
                screen==Screen::SETUP?"SETUP":"LIVE",
                screen==Screen::SETUP?"visible":"not needed");
  draw();
#ifdef GLOWDECK_WAVESHARE_S3
  tft.present();
#endif
  Serial.printf("AP: %s | Password: %s | Browse: http://192.168.4.1\n",
                apSsid.c_str(), apPassword.c_str());
  Serial.printf("Web admin username: admin | password: %s (unless changed)\n",
                cfg.adminPassword.c_str());
}

// Mirror every event's Activity API results to USB serial after any change.
// NHL observations, audio completion and WLED restores finish independently.
// Throttle to 4Hz and avoid heap/flash allocations during live playback.
void tickEventAudit(){
  static uint32_t lastAt=0;
  const uint32_t now=millis();
  if(uint32_t(now-lastAt)<250UL)return;
  lastAt=now;
  for(uint8_t index=0;index<gameEventHistory.size();++index){
    GameEventLog::Entry *e=gameEventHistory.newest(index);
    if(!e)continue;
    uint32_t fingerprint=2166136261UL;
    const auto mix=[&fingerprint](uint32_t value){
      fingerprint=(fingerprint^value)*16777619UL;
    };
    mix(e->sequence);mix(uint8_t(e->rear));mix(uint8_t(e->audio));
    mix(e->audioSource);mix(e->audioFallback);mix(e->audioFrames);
    mix(e->audioExpectedFrames);mix(e->audioDurationMs);mix(e->audioPeak);
    mix(e->audioRetries);mix(e->audioReadOffset);
    mix(uint32_t(e->audioI2sCode));mix(e->audioVolume);
    mix(e->audioAmpVerified);mix(uint8_t(e->audioFailure));
    mix(e->wledAcknowledgedMask);
    for(uint8_t i=0;i<MAX_WLED;++i)mix(uint8_t(e->wled[i]));
    mix(uint8_t(e->hue));mix(e->hueAcknowledged);
    if(fingerprint==e->serialFingerprint)continue;
    e->serialFingerprint=fingerprint;
    Serial.printf("[EVENT AUDIT] #%lu type=%s game=%lu %s@%s team=%s"
                  " score=%u-%u period=%u preview=%u rear=%s\n",
      (unsigned long)e->sequence,GameEventLog::typeLabel(e->type),
      (unsigned long)e->gameId,e->away,e->home,e->team,
      unsigned(e->awayScore),unsigned(e->homeScore),
      unsigned(e->period),unsigned(e->preview),
      GameEventLog::rearLabel(e->rear));
    Serial.printf("[EVENT AUDIT] #%lu audio=%s source=%u fallback=%u"
                  " frames=%lu/%lu elapsed=%lums peak=%lu retries=%lu"
                  " i2sCode=%ld readOffset=%lu volume=%u ampVerified=%u"
                  " failure=%s speakerVerified=NO\n",
      (unsigned long)e->sequence,GameEventLog::audioLabel(e->audio),
      unsigned(e->audioSource),unsigned(e->audioFallback),
      (unsigned long)e->audioFrames,(unsigned long)e->audioExpectedFrames,
      (unsigned long)e->audioDurationMs,(unsigned long)e->audioPeak,
      (unsigned long)e->audioRetries,(long)e->audioI2sCode,
      (unsigned long)e->audioReadOffset,unsigned(e->audioVolume),
      unsigned(e->audioAmpVerified),
      GameEventLog::audioFailureLabel(e->audioFailure));
    if(e->hue!=GameEventLog::Wled::NA)
      Serial.printf("[EVENT AUDIT] #%lu HUE=%s commandAccepted=%u\n",
        (unsigned long)e->sequence,GameEventLog::wledLabel(e->hue),
        unsigned(e->hueAcknowledged));
    for(uint8_t i=0;i<MAX_WLED;++i){
      if(e->wled[i]==GameEventLog::Wled::NA &&
         !(e->wledAcknowledgedMask&(1U<<i)))continue;
      Serial.printf("[EVENT AUDIT] #%lu WLED%u=%s commandAccepted=%u\n",
        (unsigned long)e->sequence,unsigned(i+1),
        GameEventLog::wledLabel(e->wled[i]),
        unsigned((e->wledAcknowledgedMask&(1U<<i))!=0));
    }
  }
}


void tickHueCelebrations(){
  // Verified, armed NHL PP evidence, independent of WLED participation.
  const uint32_t now=millis();
  uint32_t ppId=0,ppSeq=0,ppColor=0;
  if(hue.config.enabled && (hue.config.events&CelebrationEngine::PP_FLAG)){
    for(uint8_t i=0;i<MultiGameArming::kMaxArmed;++i){
      const Game *g=gameById(armedGames.ids[i]);
      if(!g||!CelebrationEngine::isLive(g->state.c_str())||
         g->intermission||!g->ppDataKnown||!g->ppSide||
         uint32_t(now-g->ppUpdatedAt)>=80000UL||
         wledPpSuppressed(g->id))continue;
      ppId=g->id;
      ppColor=teamRgb(g->ppSide==1?g->home:g->away);
      ppSeq=(g->id==selectedGameId)?armedPpLogSeq:0;
      if(!ppSeq)for(uint8_t row=0;row<gameEventHistory.size();++row){
        const GameEventLog::Entry *e=gameEventHistory.newest(row);
        if(e && e->gameId==g->id &&
           e->type==GameEventLog::Type::POWER_PLAY_START){
          ppSeq=e->sequence;break;
        }
      }
      break;
    }
  }
  if(hue.event==CelebrationEngine::Event::POWER_PLAY){
    if(hue.gameId && (!ppId||hue.gameId!=ppId||hue.color!=ppColor))
      hue.stop();
    else if(!hue.gameId && hueTestUntil &&
            int32_t(now-hueTestUntil)>=0){
      hueTestUntil=0;hue.stop();
    }
  }
  if(hue.idle()&&huePriorityQueue.count){
    MultiGameArming::Request next;
    if(huePriorityQueue.pop(next,armedGames))
      dispatchHueEvent(next.event,next.color,next.sequence,next.gameId);
  }
  static uint32_t lastPpAttempt=0;
  if(ppId&&hue.idle()&&!huePriorityQueue.count&&
     uint32_t(now-lastPpAttempt)>=8000UL){
    lastPpAttempt=now;
    dispatchHueEvent(CelebrationEngine::Event::POWER_PLAY,ppColor,ppSeq,ppId);
  }
  // WAV streaming runs in loopTask; even a local TLS handshake can starve
  // its audio feeder. Defer Hue I/O until playback is no longer active.
  // This does not delay NHL polling, rear LEDs or existing WLED effects.
#ifdef GLOWDECK_WAVESHARE_S3
  if(!s3Audio.playing())
#endif
    hue.tick();
  if(hueSeenRevision!=hue.revision){
    hueSeenRevision=hue.revision;
    GameEventLog::Entry *row=gameEventHistory.find(hue.auditSequence);
    if(row){
      switch(hue.result){
        case HueBridge::Result::PENDING:row->hue=GameEventLog::Wled::PENDING;break;
        case HueBridge::Result::ACCEPTED:row->hue=GameEventLog::Wled::ACCEPTED;break;
        case HueBridge::Result::SNAPSHOT_FAILED:
          row->hue=GameEventLog::Wled::SNAPSHOT_FAILED;break;
        case HueBridge::Result::COMMAND_FAILED:
          row->hue=GameEventLog::Wled::COMMAND_FAILED;break;
        case HueBridge::Result::RESTORED:row->hue=GameEventLog::Wled::RESTORED;break;
        case HueBridge::Result::RESTORE_FAILED:
          row->hue=GameEventLog::Wled::RESTORE_FAILED;break;
        default:break;
      }
      row->hueAcknowledged=row->hueAcknowledged||hue.accepted;
    }
  }
}

void loop() {
  // Process one DNS packet per iteration without a blocking captive task.
  // Ignore STA/LAN HTTP requests outside the existing authenticated routes.
  if(captiveDnsReady)captiveDns.processNextRequest();
  // Handle Improv frames from ESP Web Tools in the normal loop. It shares
  // Serial with diagnostics; never echo supplied Wi-Fi passwords.
  for(uint8_t i=0;i<32 && Serial.available();++i)
    improvSerial.handleSerial();
  server.handleClient();
  handleTouch();
  tickHorn();
  tickEventAudit();
  tickRestores();
  tickPowerPlaySessions(millis());
  tickRearLed();
#ifdef GLOWDECK_WAVESHARE_S3
  tickCaseLeds();
#endif
  tickWledCelebrations();
  tickWledDdpStreams(); // <=20 FPS/stream, no HTTP in frame path
  tickWledDiscovery();
  tickHueCelebrations();

  const uint32_t now = millis();
  const bool connected = WiFi.status() == WL_CONNECTED;
  if(connected){
    wifiOfflineSince=0;
    // A previously offline controller returns to the regular scoreboard.
    if(hotspotSetupShown && screen==Screen::SETUP){
      screen=Screen::LIVE;
      needsDraw=true;
    }
    hotspotSetupShown=false;
  } else {
    if(!wifiOfflineSince)wifiOfflineSince=now;
    // If an SSID was saved but unreachable, stay on the setup QR instead of
    // continuing to cycle empty daily games/standings. Keep the 20s grace for
    // an in-progress disconnect and never obscure an active NHL overlay.
    if(!overlayController.active() && screen!=Screen::SETUP &&
       screen!=Screen::CALIBRATE &&
       (cfg.ssid.isEmpty() || uint32_t(now-wifiOfflineSince)>=20000UL)){
      hotspotSetupShown=true;
      screen=Screen::SETUP;
      needsDraw=true;
      Serial.println("[Setup] Offline - presenting hotspot QR recovery");
    }
  }
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
      // WLED 8A discovery uses this existing responder; no new server.
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
    const bool restoreSoon=wledRestoreWouldConflict(
        now,NhlFetchPolicy::kResponseTimeoutMs+500UL);
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
  // Active WLED PP effects MUST NOT starve this request: otherwise the
  // tracker never sees a verified penalty expiry and PP_END won't be logged.
  if (connected && uint32_t(now - lastPenaltyLedgerAttempt) >= 20000UL &&
      rearEvent != RearEvent::GOAL && rearEvent != RearEvent::SHOOTOUT) {
    const bool restoringLights=wledRestoreWouldConflict(
        millis(),NhlFetchPolicy::kResponseTimeoutMs+500UL);
    if (!restoringLights) {
      lastPenaltyLedgerAttempt = millis();
      fetchFollowedPowerPlays();
      observePowerPlayEvents();
    }
  }
  // SO play-by-play stays separate from regular scores and PP requests.
  // Never block a pending WLED state restoration with a new NHL request.
  if(connected && uint32_t(millis()-lastShootoutPoll)>=6000UL){
    bool liveSo=false;
    for(uint8_t i=0;i<gameCount;++i)
      if(games[i].periodType=="SO" &&
         CelebrationEngine::isLive(games[i].state.c_str()))liveSo=true;
    if(liveSo&&!wledRestoreWouldConflict(millis(),5000UL)){
      lastShootoutPoll=millis();fetchShootoutPlays();
    }
  }
  tickForgeNews(millis());
  if (overlayController.tick(millis())) {
    // A queued alert may take over; after the final event, re-pin the
    // user's selected NHL game rather than advancing to another slide.
    if(!overlayController.active() && overlayReturnHeldGameId){
      carouselHold.pin(overlayReturnHeldGameId);
      overlayReturnHeldGameId=0;
    }
    // If a finger was already touching the overlay as it expired, consume
    // that gesture until release instead of treating it as a carousel swipe.
    if(!overlayController.active() && touchWasDown)swallowLiveGesture=true;
    lastCarouselAdvance = millis();
    overlayLastFrame = 0;
    partialLiveUpdate = false;
    needsDraw = true;
  }
  // Manual dismissal uses dismiss() instead of tick(). Restore either way.
  if(!overlayController.active() && overlayReturnHeldGameId){
    carouselHold.pin(overlayReturnHeldGameId);
    overlayReturnHeldGameId=0;
    lastCarouselAdvance=millis();
    partialLiveUpdate=false;
    needsDraw=true;
  }
  if(!overlayController.active() && screen==Screen::LIVE &&
     cfg.cycleGames && newsViewEnabled() && newsCache.count>1 &&
     uint32_t(now-lastCarouselAdvance)>=NhlNews::kCardMs){
    advanceNews(1);
  } else if (!overlayController.active() && screen == Screen::LIVE &&
      cfg.cycleGames && !newsViewEnabled() && !carouselHold.paused &&
      carouselPageCount() > 1 &&
      uint32_t(now - lastCarouselAdvance) >= uint32_t(cfg.cycleSeconds) * 1000UL) {
    advanceCarousel(1);
  }

  // Five frames/second affects ONLY the bottom ticker strip; scoreboard
  // updates and NHL networking retain their existing cadence and priority.
  if(!overlayController.active() && screen==Screen::LIVE &&
     showingGamePage() && gameCount) {
    const LiveCarousel::Page visible=currentCarouselPage();
    if(visible.index<gameCount && gameIsLive(games[visible.index])) {
      const Game &g=games[visible.index];
      if(liveTickerTextGameId!=g.id ||
         uint32_t(now-liveTickerRefreshAt)>=2500UL) {
        liveTickerRefreshAt=now;
        refreshLiveTickerText(g);
      }
      if(uint32_t(now-liveTickerScrollAt)>=NhlLiveTicker::kStepMs &&
         liveTickerTextWidthPx>
#ifdef GLOWDECK_WAVESHARE_S3
            425
#else
            264
#endif
         ) {
        // One pixel/30ms, not a whole character/180ms. Keep I/O bounded:
        // no extra NHL fetch, full-frame transfer, or UI allocations.
        liveTickerScrollAt=now;
        ++liveTickerPixelOffset;
        const String first=liveTickerGlyph(liveTickerScroll);
        const uint16_t glyphWidth=tft.textWidth(first,2);
        if(liveTickerPixelOffset>=glyphWidth) {
          liveTickerPixelOffset=0;
          liveTickerScroll=liveTickerNextByte(liveTickerScroll);
        }
        liveTickerDirty=true;
      }
    } else liveTickerDirty=false;
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
    const bool restoringLights=wledRestoreWouldConflict(
        millis(),NhlFetchPolicy::kResponseTimeoutMs+500UL);
    if (!restoringLights && rearEvent != RearEvent::GOAL && rearEvent != RearEvent::SHOOTOUT) {
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
    const bool lightRestorePending=wledRestoreWouldConflict(
        millis(),NhlFetchPolicy::kResponseTimeoutMs+500UL);
    const uint8_t currentIndex = featuredIndex % gameCount;
    if (!lightRestorePending &&
        !statsAreFresh(games[currentIndex], statsFor(currentIndex))) {
      lastCarouselStatsCheck = millis();
      fetchDetailStats(games[currentIndex].id);
    }
  }
#if defined(GLOWDECK_WAVESHARE_S3) && defined(GLOWDECK_S3_PHASE6_LIVE) && GLOWDECK_S3_PHASE6_LIVE
  // Expire a visibly stale PP/goalie-pulled badge on time even if the NHL
  // request is delayed or Wi-Fi drops. Compare the displayed bits first to
  // avoid scheduling redraws every loop iteration.
  if(!needsDraw && !overlayController.active() && !partialLiveUpdate &&
     screen==Screen::LIVE && showingGamePage() && gameCount &&
     s3Arena.ready()) {
    const LiveCarousel::Page visible=currentCarouselPage();
    if(visible.kind==LiveCarousel::Kind::GAME && visible.index<gameCount) {
      const Game &g=games[visible.index];
      const S3Broadcast::Phase phase=S3Broadcast::classify(
          g.state.c_str(),g.intermission);
      const S3Broadcast::SpecialTeams fresh=S3Broadcast::specialTeams(
          phase,g.ppDataKnown,g.ppSide,g.ppUpdatedAt,
          g.emptyNetKnown,g.emptyNetSide,g.emptyNetUpdatedAt,now);
      if(S3Broadcast::needsStatusRefresh(s3BroadcastRenderedGameId,
           s3BroadcastRenderedStatus,g.id,fresh.signature()))
        partialLiveUpdate=true;
    }
  }
#endif
#ifdef GLOWDECK_WAVESHARE_S3
  bool displayChanged = false;
  bool s3ScoreOnlyFrame = false;
  bool s3TickerFrame=false;
  bool s3NewsTickerFrame=false;
  uint8_t s3PartialMask=S3BroadcastFrame::BOTH;
#endif
  tickNewsTicker(now);
  const bool fullDrawThisLoop=needsDraw;
  if (needsDraw) {
    partialLiveUpdate = false;
    draw();
    liveTickerDirty=false;
    newsTickerDirty=false;
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
#if defined(GLOWDECK_S3_PHASE6_LIVE) && GLOWDECK_S3_PHASE6_LIVE
      if(s3Arena.ready()){
        const Game &g=games[page.kind==LiveCarousel::Kind::GAME
                            ? page.index:featuredIndex % gameCount];
        const auto next=captureS3ArenaFrame(g,millis());
        // A game switch always repaints logos/footer; it cannot be handled
        // by readout-only rectangles. Ordinary updates use only changed bits.
        if(S3BroadcastFrame::needsFullScene(s3BroadcastLastFrame,next)){
          drawS3ArenaScoreboard(g);
          displayChanged=true;
        } else {
          s3PartialMask=S3BroadcastFrame::changed(s3BroadcastLastFrame,next);
          if(s3PartialMask){
            drawS3ArenaReadout(g,s3PartialMask);
            s3ScoreOnlyFrame=true;
            displayChanged=true;
          }else{
            ++s3PerfSkippedFrames;
          }
        }
      } else
#endif
        drawS3Readout(games[page.kind==LiveCarousel::Kind::GAME
                           ? page.index:featuredIndex % gameCount]);
      // Phase 6B.4: do not override the exact partial/full/unchanged
      // decision made above. The Phase 5 fallback still uses both plates.
#if defined(GLOWDECK_S3_PHASE6_LIVE) && GLOWDECK_S3_PHASE6_LIVE
      if(!s3Arena.ready()){
        s3ScoreOnlyFrame=true;
        displayChanged=true;
      }
#else
      s3ScoreOnlyFrame=true;
      displayChanged=true;
#endif
    } else {
      drawLiveReadout(games[featuredIndex % gameCount]);
      displayChanged=true;
    }
#else
    drawLiveReadout(games[featuredIndex % gameCount]);
#endif
  }
  if(!fullDrawThisLoop && !overlayController.active() &&
     liveTickerDirty && screen==Screen::LIVE && showingGamePage() &&
     gameCount) {
    const LiveCarousel::Page page=currentCarouselPage();
    if(page.index<gameCount && gameIsLive(games[page.index])) {
#ifdef GLOWDECK_WAVESHARE_S3
      if(tft.nativeLayout() && s3Arena.ready()){
#if defined(GLOWDECK_S3_PHASE6_LIVE) && GLOWDECK_S3_PHASE6_LIVE
        drawS3ArenaTicker(games[page.index]);
        s3TickerFrame=true;
        displayChanged=true;
#endif
      }else
#endif
        drawLiveTickerCYD(games[page.index]);
    }
    liveTickerDirty=false;
  }
  if(!fullDrawThisLoop && !overlayController.active() &&
     newsTickerDirty && newsCache.count && newsViewEnabled()) {
    drawNewsTicker();
#ifdef GLOWDECK_WAVESHARE_S3
    s3NewsTickerFrame=true;
    displayChanged=true;
#endif
  }
  newsTickerDirty=false;
#ifdef GLOWDECK_WAVESHARE_S3
  if (displayChanged) {
    const uint32_t transferStarted=millis();
    uint32_t bytesSent=S3BroadcastFrame::kFullBytes;
    if((s3ScoreOnlyFrame||s3TickerFrame||s3NewsTickerFrame) && !overlayController.active()) {
      bytesSent=s3ScoreOnlyFrame?
          (s3Arena.ready()?
          S3BroadcastFrame::transferBytes(s3PartialMask):
          uint32_t(S3Raster::kClockScore.w*S3Raster::kClockScore.h*2+
                   S3Raster::kShots.w*S3Raster::kShots.h*2)):0;
      if(s3TickerFrame)bytesSent+=480UL*20UL*2UL;
      if(s3NewsTickerFrame)bytesSent+=480UL*24UL*2UL;
    }
    if ((s3ScoreOnlyFrame||s3TickerFrame||s3NewsTickerFrame) && !overlayController.active()) {
      // Two small SPI transfers instead of the full 480x320 (~300 KiB).
      // Full redraws always transmit all pixels, so changing away from
      // standings/stats/events can never preserve portions of old pages.
#if defined(GLOWDECK_S3_PHASE6_LIVE) && GLOWDECK_S3_PHASE6_LIVE
      if(s3Arena.ready()){
        const auto top=S3NativeLayout::kBroadcastReadoutTop;
        const auto bottom=S3NativeLayout::kBroadcastReadoutBottom;
        if(s3ScoreOnlyFrame && (s3PartialMask&S3BroadcastFrame::TOP))
          tft.presentRegion(top.x,top.y,top.w,top.h);
        if(s3ScoreOnlyFrame && (s3PartialMask&S3BroadcastFrame::BOTTOM))
          tft.presentRegion(bottom.x,bottom.y,bottom.w,bottom.h);
        if(s3TickerFrame)tft.presentRegion(0,269,480,20);
      } else
#endif
      {
        const auto score=S3Raster::kClockScore;
        const auto shots=S3Raster::kShots;
        if(s3ScoreOnlyFrame){
          tft.presentRegion(score.x,score.y,score.w,score.h);
          tft.presentRegion(shots.x,shots.y,shots.w,shots.h);
        }
      }
      // Text cards are independent of the arena framebuffer on S3.
      if(s3NewsTickerFrame)tft.presentRegion(0,286,480,24);
    } else {
      tft.present();
    }
#if defined(GLOWDECK_S3_PHASE6_LIVE) && GLOWDECK_S3_PHASE6_LIVE
    if(s3Arena.ready()){
      if(s3ScoreOnlyFrame||s3TickerFrame||s3NewsTickerFrame)++s3PerfPartialFrames;
      else ++s3PerfFullFrames;
      s3PerfTransferBytes+=bytesSent;
      const uint32_t elapsed=millis()-transferStarted;
      if(elapsed>s3PerfMaxTransferMs)s3PerfMaxTransferMs=elapsed;
      if(uint32_t(millis()-s3PerfReportedAt)>=60000UL){
        Serial.printf("[S3 6B.4] full=%lu partial=%lu skipped=%lu SPI~%luB "
                      "maxTransfer=%lums freePSRAM=%uB\n",
                      (unsigned long)s3PerfFullFrames,
                      (unsigned long)s3PerfPartialFrames,
                      (unsigned long)s3PerfSkippedFrames,
                      (unsigned long)s3PerfTransferBytes,
                      (unsigned long)s3PerfMaxTransferMs,
                      unsigned(ESP.getFreePsram()));
        s3PerfReportedAt=millis();
        s3PerfFullFrames=0;
        s3PerfPartialFrames=0;
        s3PerfSkippedFrames=0;
        s3PerfTransferBytes=0;
        s3PerfMaxTransferMs=0;
      }
    }
#endif
  }
#endif
  // Image HTTP and decode are lower priority than the entire NHL render
  // pipeline. A photo never triggers a light, sound or game event.
  if (!overlayController.active())tickNewsPhoto(millis());
  // Update one optional pregame conference record after the current frame
  // is painted. This keeps the first season/standings view responsive.
  if (!overlayController.active()) tickPregameConferenceRecords();
  if (rebootAt && int32_t(now - rebootAt) >= 0) ESP.restart();
  delay(5);
}
