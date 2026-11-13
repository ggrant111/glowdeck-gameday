"""Fail-closed contract for GameDay NHL shootout handling (both targets)."""
from pathlib import Path

main = Path("src/main.cpp").read_text()
policy = Path("include/NhlShootoutPolicy.h").read_text()
events = Path("include/CelebrationEngine.h").read_text()
web = Path("include/WebUi.h").read_text()

assert 'g.periodType!="SO"' in main, "Shootout goals may not trigger normal horns"
assert "observeShootoutStarts();" in main
assert "fetchShootoutPlays();" in main
assert '"/play-by-play"' in main
assert 'shot["periodDescriptor"]["periodType"]' in main
assert 'shot["details"]["eventOwnerTeamId"]' in main
assert "watch.lastSort=newest;" in main
assert "watch.playsKnown=true;" in main
assert "watch.transitionSeen=false;" in main
assert "No startHorn()/celebrate() for shootout shots" in main
assert "NhlShootoutPolicy::classify(" in main
assert "signalShootoutAttempt(" in main
assert "queueArmedWledEvent(evt,color,row.sequence" in main
assert "if(followed)" in main
assert "SHOOTOUT_GOAL" in events and "SHOOTOUT_FLAG = 16" in events
assert 'case Event::SHOOTOUT_SAVE:return {17' in events
assert '["shootout",16,"Shootouts"]' in web
assert 'value="shootouts"' in web
assert '"so-goal"' in web and '"so-save"' in web
for code in (505,506,507,537):
    assert f"type=={code}" in policy
print("PASS SO event contract, safe goal separation, UI and official NHL feed")
