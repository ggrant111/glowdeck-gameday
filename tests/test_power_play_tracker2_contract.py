"""PP2 single-source-of-truth, lightweight NHL cadence, diagnostics and S3/CYD parity."""
from pathlib import Path
s=Path("src/main.cpp").read_text()
tracker=Path("include/NhlPowerPlayTracker2.h").read_text()
ledger=Path("include/NhlPenaltyLedger.h").read_text()
ui=Path("include/WebUi.h").read_text()

assert '#include "NhlPowerPlayTracker2.h"' in s
assert 'NhlPowerPlayTracker2::Watch ppSessions[MAX_GAMES];' in s
assert 'powerPlaySession(uint32_t id,bool create=true)' in s
assert 'session->observe(input)' in s
assert 'session->outputSide(now,g.intermission)' in s
assert 'void tickPowerPlaySessions(uint32_t now)' in s
assert 'tickPowerPlaySessions(millis());' in s
assert 'session->takePending()' in s
assert 'PowerPlayLogPolicy::observe(' not in s
assert 'g.ppSide = 0;\n    g.ppCandidate = 0;' not in s
assert 'if(priorId!=g.id)' in s
assert 'NhlPenaltyLedger::Book::fromRemaining(' in s
assert '((g.id/10000UL)%100UL)==2UL' in s
assert 'ledger->evidenceAt(input.gameAt)' in s
assert 'NhlLiveTicker::isPowerPlayGoal(strength)' in s
assert 'session->anchorKey' in s
assert 'verifiedPpGoalEnd' in tracker
assert 'Phase::HELD' in tracker and 'Phase::INTERMISSION' in tracker
assert 'kUnverifiedGraceMs = 120000UL' in tracker
assert 'kEndConfirmationSamples = 2' in tracker
assert 'std::' not in tracker # no hidden heap dependency
assert 'hasPpGoalSince' in ledger and 'Evidence evidenceAt(' in ledger
assert 'manpowerCandidate(' in s
assert 'wledPpStoppedSessions' in s
assert 'session->session!=wledPpStoppedSessions[i]' in s
assert 'if(!target||urgency>winningUrgency)' in s
assert 'const uint32_t age=candidate->loaded?' in s
assert 'session->side?35UL' in s
assert 'drawS3ArenaFitted("PP NEXT: "' in s
assert 'tft.drawString("PP NEXT",x+31,152,1);' in s
assert 'server.on("/api/pp-tracker",HTTP_GET,[](){' in s
assert '"powerPlaySession"' in s
assert 'id="ppTrackerRows"' in ui
assert 'async function refreshPpTracker()' in ui
assert '"/api/pp-tracker"' in ui
assert 'id="refreshPpTracker"' in ui

# A PP end must never globally restore WLED if a different armed PP
# could still own physical devices.
events=s.split("void observePowerPlayEvents() {",1)[1].split(
    "Game *gameById(",1)[0]
assert "scheduleWledRestoreAll();" not in events
assert 'session->takePending()' in events
assert 'if(change.ended)' in events
assert 'if(change.started)' in events
assert 'startPowerPlaySound(entry.sequence)' in events
print("PASS: PP2 shared state, session events, NHL verification, WLED, web UI, CYD/S3 and safe restore")
