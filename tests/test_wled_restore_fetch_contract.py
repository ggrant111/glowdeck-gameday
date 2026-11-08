"""Prevent PP lighting from starving NHL scoreboard, penalty verification and stats."""
from pathlib import Path
s=Path("src/main.cpp").read_text()
policy=Path("include/WledRestoreFetchPolicy.h").read_text()
events=Path("include/GameEventLog.h").read_text()
ticker=Path("include/NhlPowerPlayTracker2.h").read_text()

assert '#include "WledRestoreFetchPolicy.h"' in s
assert "restoreAt==kHeldUntilEventEnd" in policy
assert "kHeldUntilEventEnd=UINT32_MAX" in policy
assert "int32_t(restoreAt-now)<=int32_t(fetchBudgetMs)" in policy
assert "bool wledRestoreWouldConflict(uint32_t now,uint32_t fetchBudgetMs)" in s
assert "WledRestoreFetchPolicy::dueSoon(" in s
assert "restoreAt[i] == UINT32_MAX" in s, "Restoration must still skip active PP leases"

loop=s.split("void loop() {",1)[1]
score=loop.split("const bool connected =",1)[1].split(
    "// Validate on-ice PP",1)[0]
ledger=loop.split("// Validate on-ice PP",1)[1].split(
    "if (overlayController.tick(",1)[0]
standings=loop.split("// Standings are requested",1)[1].split(
    "// Prefetch stats",1)[0]
stats=loop.split("// Prefetch stats",1)[1].split(
    "#if defined(GLOWDECK_WAVESHARE_S3)",1)[0]

assert "wledRestoreWouldConflict(" in score
assert "if (!restoreSoon && uint32_t(now - lastPoll) >= interval)" in score
assert "pollNhl();" in score
assert "wledRestoreWouldConflict(" in ledger
assert "fetchFollowedPowerPlays();" in ledger
assert "observePowerPlayEvents();" in ledger
assert "wledRestoreWouldConflict(" in standings
assert "wledRestoreWouldConflict(" in stats
assert "fetchDetailStats(games[currentIndex].id);" in stats

for segment in (score,ledger,standings,stats):
    assert "if (restoring[i]) restoringLights = true;" not in segment
    assert "if (restoring[i]) lightRestorePending = true;" not in segment
assert "int32_t(restoreAt[i] - now)" not in score

# The PP end must be recorded from a verified session transition and remain
# discoverable under both All and Power Plays in the Activity history.
observer=s.split("void observePowerPlayEvents() {",1)[1].split(
    "Game *gameById(",1)[0]
assert "session->takePending()" in observer
assert "if(change.ended)" in observer
assert "GameEventLog::Type::POWER_PLAY_END" in observer
assert "queueGameOverlay(&g,EventOverlay::Kind::POWER_PLAY_END" in observer
assert "if(change.started)" in observer
assert "POWER_PLAY_END" in ticker or "pendingEnd=side" in ticker
assert 'if (!strcmp(category, "powerplays"))' in events
assert "t == Type::POWER_PLAY_END" in events
print("PASS: indefinite WLED PP lease cannot block scoreboard/PP ledger/stats; verified PP_END reaches Activity")
