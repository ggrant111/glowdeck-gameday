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
# Main scoreboard already displays SOG and the team on a live PP.
# The ticker carries complementary facts only. A goal's PP strength is
# historical context and remains on the scorer entry.
ticker=s.split('String buildLiveTickerText(const Game &g) {',1)[1].split(
    'void refreshLiveTickerText(const Game &g) {',1)[0]
assert 'BOX: ' in ticker and 'GOALS: ' in ticker
assert 'HITS: ' in ticker and 'BLOCKS: ' in ticker
assert 'PIM: ' in ticker and 'FACEOFF: ' in ticker
assert 'SOG: ' not in ticker
assert 'POWER PLAY: ' not in ticker
assert '   |   PP: ' not in ticker
assert 'if(goal.powerPlay)text+=" PP";' in ticker
assert 'cached.hitsAway=row["awayValue"].as<String>();' in s
assert 'cached.hitsHome=row["homeValue"].as<String>();' in s
assert 'cached.blocksAway=row["awayValue"].as<String>();' in s
assert 'cached.blocksHome=row["homeValue"].as<String>();' in s
assert 'category.equalsIgnoreCase("blockedShotAttempts")' in s
assert 'category.equalsIgnoreCase("hits")' in s
assert 'String hitsAway, hitsHome, blocksAway, blocksHome;' in s
assert '" / "+g.home+" "+stats.faceHome;' in ticker
assert '"% / "+g.home+" "+stats.faceHome+"%"' not in ticker
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
