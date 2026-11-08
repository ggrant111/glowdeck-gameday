"""Regression: PP_END names short-handed team in overlay, S3/CYD and Activity."""
from pathlib import Path
s=Path("src/main.cpp").read_text(encoding="utf-8")
presentation=Path("include/NhlPowerPlayPresentation.h").read_text()
events=s.split("void observePowerPlayEvents() {",1)[1].split(
    "Game *gameById(",1)[0]
end=events.split("if(change.ended){",1)[1].split("if(change.started){",1)[0]
start=events.split("if(change.started){",1)[1].split("if(change.resumed)",1)[0]
assert '#include "NhlPowerPlayPresentation.h"' in s
assert 'advantageSide==1?2:advantageSide==2?1:0' in presentation
assert "NhlPowerPlayPresentation::shortHandedSide(change.ended)" in end
assert "const String &team=shortSide==1?g.home:g.away;" in end
assert "const String &ppTeam=change.ended==1?g.home:g.away;" in end
assert "GameEventLog::Type::POWER_PLAY_END,team,0" in end
assert "queueGameOverlay(&g,EventOverlay::Kind::POWER_PLAY_END,team,seq,true)" in end
assert "backAtFullStrength=%s previousPP=%s" in end
assert "const String &team=change.started==1?g.home:g.away;" in start
assert "GameEventLog::Type::POWER_PLAY_START,team,0" in start
assert "queueGameOverlay(&g,EventOverlay::Kind::POWER_PLAY_START," in start
# Both screen variants should explicitly name the penalized team.
assert 'description = String(e.team) + " BACK AT FULL STRENGTH";' in s
assert 'return String(e.team)+" BACK AT FULL STRENGTH";' in s
# The shared queue uses the team parameter for its logo and color.
queue=s.split("bool queueGameOverlay(const Game *g",1)[1].split(
    "bool",1)[0]
assert "item.color = teamRgb(team);" in queue
assert "EventOverlay::copy(item.team, team.c_str()" in queue
print("PASS PP_END overlay/logo/colors/event history use formerly shorthanded team; PP_START unchanged")
