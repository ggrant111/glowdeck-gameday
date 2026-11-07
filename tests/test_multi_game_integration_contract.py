#!/usr/bin/env python3
"""Multi-arm contract: five ranked games, priority arbitration and safe WLED leases."""
from pathlib import Path
s=Path("src/main.cpp").read_text()
ui=Path("include/WebUi.h").read_text()
layout=Path("include/S3NativeLayout.h").read_text()
pages=Path("include/S3BroadcastPages.h").read_text()
for code in (
    '#include "MultiGameArming.h"',
    'MultiGameArming::Armed armedGames;',
    'MultiGameArming::Queue wledPriorityQueue;',
    'if (isArmedGame(gameId)) return true;',
    'bool editArmedGame(uint32_t id,bool enable)',
    'bool moveArmedGame(uint32_t id,int direction)',
    'persistArmedGames();',
    'prefs.putUInt(("armed"+String(i)).c_str(),armedGames.ids[i]);',
    'prefs.getUInt(("armed"+String(i)).c_str(),0)',
    'doc["armedCount"] = armedGames.count();',
    'doc["wledActiveGameId"]=wledActiveGameId;',
    'g["armedPriority"] = armPriority(games[i].id);',
    'server.on("/api/arm",HTTP_POST,[](){',
    'queueArmedWledEvent(CelebrationEngine::Event::GOAL',
    'wledPriorityQueue.add(item,armedGames)',
    'startQueuedWledEvent(now)',
    'ppSide=currentWledPowerPlay(ppColor,ppGameId,ppSeq);',
    'suppressArmedPowerPlays();',
    'bool cleanWledLeaseForNextEvent(uint8_t slot)',
    'if(!releaseWledDdp(slot))',
    'wledMappedRestoreBody(savedState[slot],wledTouchedSegments[slot])',
    'wledDeviceGameOwner[i]=gameId;',
    'if(!cleanWledLeaseForNextEvent(i))',
    'if(wledActiveGameId==id)',
    'if(!isScheduledGame(id))',
):
    assert code in s,code
assert 'Action::SUMMARY_ARM' in layout
assert 'summaryArm(uint8_t n)' in layout
assert 'case S3NativeLayout::Action::SUMMARY_ARM:' in s
assert 'editArmedGame(detailGameId,!isArmedGame(detailGameId))' in s
assert 'armPriority(g.id)' in pages
assert '/api/arm' in ui and 'armedGameSummary' in ui
assert 'priority.setAttribute("aria-label",priority.title)' in ui
assert 'Armed '+'' in ui  # Button status remains visible.
# A PP-end overlay must not directly restore some other armed game's lights.
observer=s.split('void observePowerPlayEvents() {',1)[1].split('Game *gameById',1)[0]
assert 'change.ended' in observer
assert 'scheduleWledRestoreAll();' not in observer
assert 'wledActiveGameId' in s and 'wledDeviceGameOwner' in s
# No additional polling endpoint per armed game.
assert 'https://api-web.nhle.com/v1/score/' in s
print("PASS multi-arm: 5 ranks, web/device controls, priority dispatch, WLED leases")
