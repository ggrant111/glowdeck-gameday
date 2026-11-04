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
print("PASS: WLED celebration dashboard JavaScript and control bindings")
