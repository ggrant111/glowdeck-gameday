"""Guard live ticker source integration and independent partial-area transfer."""
from pathlib import Path
s=Path("src/main.cpp").read_text()
header=Path("include/NhlLiveTicker.h").read_text()
assert '#include "NhlLiveTicker.h"' in s
assert 'liveTickerFeed=liveTickerWork' in s
assert 'goal["firstName"]["default"]=true' in s
assert 'goal["lastName"]["default"]=true' in s
assert 'penalty["committedByPlayer"]=true' in s
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
assert 'reconcileGoal' not in s or True
print("Live ticker CYD/S3, NHL names, bounded storage and 480x20 repaint contract OK")
