"""Guard live ticker source integration and independent partial-area transfer."""
from pathlib import Path
s=Path("src/main.cpp").read_text()
header=Path("include/NhlLiveTicker.h").read_text()
assert '#include "NhlLiveTicker.h"' in s
assert 'liveTickerFeed=liveTickerWork' in s
assert 'goal["firstName"]["default"]=true' in s
assert 'goal["lastName"]["default"]=true' in s
assert 'penalty["committedByPlayer"]=true' in s
assert 'penalty["servedBy"]=true' in s
assert 'NhlLiveTicker::validPlayerName(valueText)' in s
assert 'if(player.isEmpty())player=fullName(committed);' in s
assert 'if(boxesFresh && !active)text+="EMPTY";' in s
assert 'else if(!boxesFresh)text+=g.intermission?"INTERMISSION":"--";' in s
assert 'uint8_t liveTickerPixelOffset=0;' in s
assert 'liveTickerNextByte(uint16_t index)' in s
assert '++liveTickerPixelOffset;' in s
assert 'String liveTickerWindow(uint16_t maxWidth,uint8_t font)' in s
assert 'tft.textWidth(liveTickerText,2)>' not in s
assert 'kStepMs = 30' in header
assert 'validPlayerName' in header
assert 'liveTickerWork.applyPowerPlayGoals()' in s
assert 'tickerTargetVisible' in s
assert 'liveTickerWork.addPenalty' in s and 'liveTickerWork.addGoal' in s
assert 'BOX: ' in s and 'GOALS: ' in s and 'SOG: ' in s
assert 'drawLiveTickerCYD(g);' in s and 'drawS3ArenaTicker(g);' in s
assert 'tft.presentRegion(0,269,480,20)' in s
assert 'bool s3TickerFrame=false;' in s
assert '(s3ScoreOnlyFrame||s3TickerFrame)' in s
assert 'if(!fullDrawThisLoop && !overlayController.active()' in s
assert 'gameIsLive(g)' in s and 'NhlLiveTicker::kStepMs' in s
assert 'kMaxGoals = 16' in header and 'kMaxPenalties = 20' in header
# Keep the scored-goal/PP/goal horn eligibility separated from ticker UI.
assert 'observePowerPlayEvents();' in s
print("Live ticker CYD/S3, NHL names, bounded storage and 480x20 repaint contract OK")
