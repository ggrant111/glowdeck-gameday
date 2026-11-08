#pragma once
#include <stdint.h>

// One state machine per NHL game, shared by the PP badge, rear/case LEDs,
// WLED and event history. A temporary absence of the NHL situation object
// must NEVER be mistaken for a new PP or a verified end.
namespace NhlPowerPlayTracker2 {
constexpr uint32_t kUnverifiedGraceMs = 120000UL;
constexpr uint32_t kEndConfirmationMs = 10000UL;
constexpr uint8_t kEndConfirmationSamples = 2;
constexpr uint32_t kIntermissionMaxMs = 1800000UL;

enum class Phase : uint8_t {
  IDLE, VERIFYING, ACTIVE, HELD, INTERMISSION, UNKNOWN
};
enum class Reason : uint8_t {
  NONE, VERIFIED, LABEL_MISSING, LEDGER_STALE, CLOCK_MISSING,
  END_PENDING, EXPIRED, PP_GOAL, INTERMISSION, RESUMED,
  NEW_GAME, FINAL, EVIDENCE_CONFLICT, TIMEOUT, NEW_SESSION
};
inline const char *phaseName(Phase p) {
  switch (p) {
    case Phase::IDLE: return "IDLE";
    case Phase::VERIFYING: return "VERIFYING";
    case Phase::ACTIVE: return "ACTIVE";
    case Phase::HELD: return "HELD";
    case Phase::INTERMISSION: return "INTERMISSION";
    case Phase::UNKNOWN: return "UNKNOWN";
  }
  return "?";
}
inline const char *reasonName(Reason r) {
  switch (r) {
    case Reason::NONE: return "none";
    case Reason::VERIFIED: return "matched label+penalty";
    case Reason::LABEL_MISSING: return "penalty active; NHL label absent";
    case Reason::LEDGER_STALE: return "official penalty evidence stale";
    case Reason::CLOCK_MISSING: return "live clock unavailable";
    case Reason::END_PENDING: return "awaiting independent end sample";
    case Reason::EXPIRED: return "game clock passed penalty end";
    case Reason::PP_GOAL: return "verified PP goal terminated minor";
    case Reason::INTERMISSION: return "penalty carried into intermission";
    case Reason::RESUMED: return "same session resumed";
    case Reason::NEW_GAME: return "new NHL game ID";
    case Reason::FINAL: return "game no longer live";
    case Reason::EVIDENCE_CONFLICT: return "NHL sources disagree";
    case Reason::TIMEOUT: return "no trusted evidence within grace";
    case Reason::NEW_SESSION: return "new verified PP opportunity";
  }
  return "?";
}
struct Sample {
  uint32_t gameId=0;
  uint32_t nowMs=0;
  bool live=false;
  bool final=false;
  bool intermission=false;
  bool ledgerFresh=false;
  bool clockKnown=false;
  uint16_t gameAt=0;
  uint8_t ledgerSide=0; // official ledger, 0=even, 1=home PP, 2=away PP
  uint8_t labelSide=0;  // current scoreboard PP situation (or 0=absent)
  bool labelKnown=false;
  uint32_t penaltyKey=0; // stable key for currently responsible penalty
  uint16_t penaltyAt=0;
  uint16_t penaltyEndAt=0;
  bool verifiedPpGoalEnd=false; // official PP goal on tracked minor
};
struct Delta {
  uint8_t started=0, ended=0;
  bool resumed=false, stateChanged=false;
};
struct Watch {
  uint32_t gameId=0;
  uint32_t session=0; // monotonic within one game's watch
  uint32_t anchorKey=0;
  uint16_t anchorPenaltyAt=0, expectedEndAt=0;
  uint8_t side=0; // preserved even during intermission/brief feed gaps
  Phase phase=Phase::IDLE;
  Reason reason=Reason::NONE;
  uint32_t lastEvidenceMs=0;
  uint32_t intermissionAt=0;
  uint32_t endCandidateAtMs=0;
  uint8_t endCandidateSamples=0;
  bool everTrusted=false;
  uint8_t pendingStart=0, pendingEnd=0;
  bool pendingResume=false;

  void reset(uint32_t id=0) { *this=Watch(); gameId=id; }
  bool evidenceInGrace(uint32_t now) const {
    return everTrusted && uint32_t(now-lastEvidenceMs)<=kUnverifiedGraceMs;
  }
  uint8_t outputSide(uint32_t now,bool inIntermission=false) const {
    return side && phase!=Phase::INTERMISSION && !inIntermission &&
           evidenceInGrace(now) ? side : 0;
  }
  void start(uint8_t team,const Sample &s) {
    ++session;
    if(!session)++session; // 0 reserved for no session
    side=team;
    anchorKey=s.penaltyKey;
    anchorPenaltyAt=s.penaltyAt;
    expectedEndAt=s.penaltyEndAt;
    lastEvidenceMs=s.nowMs;
    everTrusted=true;
    endCandidateSamples=0;endCandidateAtMs=0;
    pendingStart=team;
    phase=Phase::ACTIVE;reason=Reason::NEW_SESSION;
  }
  void end(Reason why) {
    pendingEnd=side;
    side=0;
    anchorKey=0;anchorPenaltyAt=0;expectedEndAt=0;
    endCandidateSamples=0;endCandidateAtMs=0;
    phase=Phase::IDLE;reason=why;everTrusted=false;
  }
  Delta observe(const Sample &s) {
    Delta d;
    if(!s.gameId)return d;
    const Phase before=phase;
    if(gameId!=s.gameId)reset(s.gameId);
    if(!s.live){
      if(s.final && side) {
        // A game final retires the lighting session, but is not a separate
        // live POWER_PLAY_END overlay.
        side=0;anchorKey=0;everTrusted=false;
        endCandidateSamples=0;
        pendingStart=pendingEnd=0;
      }
      phase=Phase::IDLE;reason=Reason::FINAL;
    } else if(s.intermission) {
      if(side){
        if(phase!=Phase::INTERMISSION)intermissionAt=s.nowMs;
        phase=Phase::INTERMISSION;
        reason=Reason::INTERMISSION;
      } else {
        phase=Phase::IDLE;reason=Reason::INTERMISSION;
      }
    } else if(side && phase==Phase::INTERMISSION &&
              intermissionAt &&
              uint32_t(s.nowMs-intermissionAt)>kIntermissionMaxMs) {
      phase=Phase::UNKNOWN;reason=Reason::TIMEOUT;
    } else if(s.ledgerFresh && s.clockKnown) {
      if(s.ledgerSide && s.ledgerSide==s.labelSide) {
        if(!side || side!=s.ledgerSide) {
          if(side)end(Reason::NEW_SESSION);
          start(s.ledgerSide,s);
        } else {
          if(phase==Phase::INTERMISSION || phase==Phase::UNKNOWN ||
             phase==Phase::HELD)d.resumed=true;
          lastEvidenceMs=s.nowMs;everTrusted=true;
          expectedEndAt=s.penaltyEndAt;
          endCandidateSamples=0;endCandidateAtMs=0;
          // Changing the anchor while the same team continuously has
          // the advantage is still the same uninterrupted PP opportunity.
          phase=Phase::ACTIVE;
          reason=d.resumed?Reason::RESUMED:Reason::VERIFIED;
        }
      } else if(side && s.ledgerSide==side) {
        // Still the same official serving penalty, despite a missing or
        // contradictory live situation label (TV timeout/feed refresh).
        lastEvidenceMs=s.nowMs;everTrusted=true;
        if(s.penaltyEndAt)expectedEndAt=s.penaltyEndAt;
        endCandidateSamples=0;endCandidateAtMs=0;
        if(phase==Phase::INTERMISSION || phase==Phase::UNKNOWN)d.resumed=true;
        phase=Phase::HELD;
        reason=d.resumed?Reason::RESUMED:Reason::LABEL_MISSING;
      } else if(side && s.ledgerSide==0) {
        const bool expired=expectedEndAt && s.gameAt>=expectedEndAt;
        if(s.verifiedPpGoalEnd || expired) {
          end(s.verifiedPpGoalEnd?Reason::PP_GOAL:Reason::EXPIRED);
        } else {
          if(!endCandidateSamples)endCandidateAtMs=s.nowMs;
          if(endCandidateSamples<255)++endCandidateSamples;
          if(endCandidateSamples>=kEndConfirmationSamples &&
             uint32_t(s.nowMs-endCandidateAtMs)>=kEndConfirmationMs)
            end(Reason::VERIFIED);
          else {
            phase=Phase::HELD;reason=Reason::END_PENDING;
          }
        }
      } else if(side && s.ledgerSide!=side) {
        // The scoreboard and ledger do not agree on a transition yet.
        phase=Phase::HELD;reason=Reason::EVIDENCE_CONFLICT;
      } else {
        phase=s.ledgerSide?Phase::VERIFYING:Phase::IDLE;
        reason=s.ledgerSide?Reason::EVIDENCE_CONFLICT:Reason::VERIFIED;
      }
    } else if(side) {
      phase=Phase::HELD;
      reason=!s.clockKnown?Reason::CLOCK_MISSING:Reason::LEDGER_STALE;
    } else {
      phase=Phase::VERIFYING;
      reason=!s.clockKnown?Reason::CLOCK_MISSING:Reason::LEDGER_STALE;
    }
    // Keep session identity on an outage. Do not fabricate a PP_END event;
    // physical outputs stop after the bounded grace period until reverified.
    if(side && phase!=Phase::INTERMISSION &&
       !evidenceInGrace(s.nowMs)){
      phase=Phase::UNKNOWN;reason=Reason::TIMEOUT;
    }
    if(d.resumed)pendingResume=true;
    d.started=pendingStart;
    d.ended=pendingEnd;
    d.resumed=pendingResume;
    d.stateChanged=(before!=phase)||d.started||d.ended;
    // Caller uses takePending() after all involved games were refreshed.
    return d;
  }
  Delta takePending() {
    Delta d;
    d.started=pendingStart;d.ended=pendingEnd;
    d.resumed=pendingResume;
    pendingStart=0;pendingEnd=0;pendingResume=false;
    return d;
  }
  // Called on every LED/display tick so prolonged Wi-Fi loss expires safely
  // without spuriously logging the end of an NHL power play.
  bool tick(uint32_t now,bool intermission) {
    if(!side)return false;
    if(intermission)return false;
    if(evidenceInGrace(now))return false;
    if(phase==Phase::UNKNOWN)return false;
    phase=Phase::UNKNOWN;reason=Reason::TIMEOUT;
    return true;
  }
};
} // namespace NhlPowerPlayTracker2
