#!/usr/bin/env python3
"""Compare the official NHL play-by-play with an observed CYD session log.

Uses NHL's official JSON from api-web.nhle.com, never a simulated hockey league.
Diagnostic only: does not fail CI when the external endpoint is unavailable.
"""
import json
import sys
from collections import Counter
from urllib.request import Request, urlopen

GAME_ID = 2026020246  # ANA @ VAN, 2026-11-03
BASE = "https://api-web.nhle.com/v1"

def retrieve(url):
    req = Request(url, headers={
        "User-Agent": "GlowDeckGameDay/0.3.7 (NHL event accuracy validation)",
        "Accept": "application/json",
        "Accept-Encoding": "identity",
    })
    with urlopen(req, timeout=20) as response:
        return json.load(response)

def main():
    for name, path in (
        ("play-by-play", f"/gamecenter/{GAME_ID}/play-by-play"),
        ("score", "/score/2026-11-03"),
        ("landing", f"/gamecenter/{GAME_ID}/landing"),
    ):
        try:
            doc = retrieve(BASE + path)
        except Exception as err:
            print(f"OFFICIAL {name}: UNAVAILABLE {type(err).__name__} {err}", flush=True)
            continue
        if name == "landing":
            print("LANDING top keys", list(doc.keys()))
            summary=doc.get("summary", {})
            print("LANDING summary keys", list(summary.keys()))
            for key in ("penalties","scoring","plays"):
                data=summary.get(key, doc.get(key, []))
                print("LANDING",key,"sample",json.dumps(data)[:5800])
            continue
        if name == "score":
            for game in doc.get("games", []):
                if game.get("id") == GAME_ID:
                    print("SCOREBOARD", json.dumps({
                        k:game.get(k) for k in ("id","gameState","periodDescriptor","clock")
                    }))
                    print("SCOREBOARD teams", json.dumps({
                        k:game.get(k) for k in ("awayTeam","homeTeam")
                    })[:900])
            continue
        print("OFFICIAL PBP keys:", sorted(doc.keys()))
        print("TEAMS:", json.dumps({
            key:doc.get(key) for key in ("id","gameState","gameDate","awayTeam","homeTeam")
        })[:1800])
        print("SUMMARY keys:",list(doc.get("summary",{}).keys()))
        penalties=doc.get("summary",{}).get("penalties",[])
        print("SUMMARY penalties sample:",json.dumps(penalties)[:4200])
        print("SUMMARY scoring sample:",json.dumps(doc.get("summary",{}).get("scoring",[]))[:2500])
        plays = doc.get("plays", [])
        print("TOTAL PLAYS",len(plays),"TYPES",dict(Counter(
            p.get("typeDescKey","?") for p in plays)))
        for p in plays:
            if p.get("typeDescKey") not in ("penalty", "goal", "period-end", "game-end", "period-start"):
                continue
            print("OFFICIAL EVENT",json.dumps({
                "id":p.get("eventId"),"sortOrder":p.get("sortOrder"),
                "type":p.get("typeDescKey"),
                "period":p.get("periodDescriptor",{}).get("number"),
                "periodType":p.get("periodDescriptor",{}).get("periodType"),
                "clock":p.get("timeInPeriod"),
                "remaining":p.get("timeRemaining"),
                "situationCode":p.get("situationCode"),
                "details":p.get("details"),
            }, separators=(",",":"))[:900])
        # Dedicated late-game sample to inspect whether strength reflects
        # actual penalties, pulled goalies, or 5v5 during user-observed PP loops.
        last = [p for p in plays
                if p.get("periodDescriptor",{}).get("number")==3 and
                p.get("typeDescKey") in ("penalty","goal","stoppage","faceoff","shot-on-goal")]
        print("THIRD PERIOD SAMPLE",len(last))
        for p in last[-25:]:
            print("LATE PLAY",json.dumps({
                "id":p.get("eventId"),
                "type":p.get("typeDescKey"),
                "clock":p.get("timeInPeriod"),
                "remaining":p.get("timeRemaining"),
                "situation":p.get("situationCode"),
                "owner":p.get("details",{}).get("eventOwnerTeamId"),
            }, separators=(",",":")))
    print("Official-data diagnostic complete",flush=True)

if __name__=="__main__":
    main()
