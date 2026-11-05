#!/usr/bin/env python3
"""Check embedded WEB_UI JavaScript and new WLED dashboard bindings."""
from pathlib import Path
import re
import subprocess

source = Path("include/WebUi.h").read_text(encoding="utf-8")
found = re.search(r"<script>(.*?)</script>", source, re.S)
assert found, "Web UI is missing a script tag"
script = found.group(1)
subprocess.run(["node", "--check", "-"], input=script, text=True, check=True)
controls = (
    "wledEventStatus", "wledSelectedCount", "wledGoalStyle", "favoriteSide",
    "wledPP", "wledGameStart", "wledFinal", "wledTestGoalHome",
    "wledTestGoalAway", "wledTestPP", "wledTestStart", "wledTestWin",
    "wledTestLoss", "wledStop", "wledDeviceFeedback",
)
for control in controls:
    assert f'id="{control}"' in source, f"Missing WLED control: {control}"
for method in ("/api/wled/preview", "wledPreview(", "function deviceSettings()"):
    assert method in script, f"Missing WLED UI function: {method}"
for control in (
    "recentGameEvents", "events", "eventCategory", "refreshEventLog",
    "clearEventLog", "eventCount", "eventList", "eventNewer", "eventOlder",
):
    assert f'id="{control}"' in source, f"Missing event log control: {control}"
for method in (
    "/api/events?category=", "/api/events/clear", "function createEventElement(",
    "function refreshEventSummary(", "function loadEventLog(",
):
    assert method in script, f"Missing event log UI method: {method}"
overlay_controls = (
  "overlayStatus", "overlayQueued", "overlayEnabled", "overlayGoals",
  "overlayPowerPlays", "overlayStart", "overlayFinal", "overlayFlash",
  "overlayLogos", "overlayGoalSeconds", "overlayFinalSeconds",
  "overlayTestGoal", "overlayTestPP", "overlayTestPPEnd",
  "overlayTestStart", "overlayTestWin", "overlayTestLoss", "overlayStop",
)
for control in overlay_controls:
    assert f'id="{control}"' in source, f"Missing overlay control: {control}"
assert "/api/overlay/preview" in script and "function previewOverlay(" in script
# CYD rear LED color diagnostics and combined celebration controls.
for control in (
    "rearMatchup", "rearTestHome", "rearTestAway", "rearTestPP",
    "rearTeam", "rearTeamSwatch", "rearSelectedHex", "rearTestTeam",
    "rearTestTeamPP", "rearDiagRed", "rearDiagGreen", "rearDiagBlue",
    "rearFullHome", "rearFullAway", "rearFullTeam", "rearFullStop",
    "rearFullResult",
):
    assert f'id="{control}"' in source, f"Missing rear RGB control: {control}"
for method in (
    "/api/rear/palette", "/api/rear/test", "renderRearTestControls(",
    "loadRearPalette(", "fullPreview(", "combined:true", "mode,side,team,channel",
):
    assert method in script, f"Missing rear RGB preview binding: {method}"
# New GameDay 0.4.0 IA: accessible navigation, favorites, game follow and alerts.
for control in (
    "dashLiveCount", "dashFavoriteCount", "dashFollowCount",
    "dashDisplayMode", "gameSearch", "favoriteTeamGrid",
    "favoriteTeamCount", "saveFavorites", "displayAllLiveEvents",
    "backgroundSkipped", "overlayDropped",
):
    assert f'id="{control}"' in source, f"Missing redesigned dashboard control: {control}"
for method in (
    "/api/display/favorites", "/api/rear/follow", "renderFavoriteTeams(",
    "favoriteTeamDraftMask", "renderGames(", "isFavoriteGame(",
    "data-game-filter", 'aria-pressed', "showGame(",
):
    assert method in source, f"Missing dashboard/favorite interaction: {method}"
assert 'class="section-intro"' in source, "Missing section hierarchy"
assert 'aria-label="GameDay navigation"' in source, "Missing accessible nav"
print("PASS: WLED celebration dashboard JavaScript and control bindings")
