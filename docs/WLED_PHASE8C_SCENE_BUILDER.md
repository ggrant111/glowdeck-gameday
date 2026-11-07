# Phase 8C — Native WLED Celebration Scene Builder

**Software release:** GameDay CYD and Waveshare ESP32-S3.
**Transport:** WLED **native JSON** only. UDP/DDP deferred to 8D.
**Physical acceptance:** pending real WLED testing after firmware OTA.

## New experience

Open **Customize → WLED → Saved WLED controllers → Celebration scene
builder** to configure one controller's five NHL event types:

| Event | Default (when no scene override is enabled) |
| --- | --- |
| Goal | Original strobe → chase, governed by existing global goal style |
| Power play | Breathing pulse while NHL confirms an advantage |
| Game start | Wipe, original 4.5s timing |
| Final win | Twinkle, original 7s timing |
| Final loss | Fade, original 7s timing |

Each event includes three separate **HOME / AWAY / NEUTRAL** zone
templates. Enable a template to override its effect, speed (0–255),
intensity (0–255), brightness (1–255), color policy, and optional
duration. Leave the template **Inherit** to preserve Phase 8B's exact
existing behavior. A segment must already be assigned to a zone under
**Manage segments**. Controllers with zone mapping disabled continue
to receive the original whole-controller effects; the scene library
does not change them.

Effect choices are deliberately restricted to seven WLED modes already
used by GameDay's existing effects engine, rather than exposing IDs
that may be reserved or unsupported on various WLED firmware releases:

- Solid (0), Breathe (2), Wipe (3), Fade (12), Twinkle (17),
  Strobe (23) and Chase (28).

Colors: **Automatic** (Phase 8B default), **Team primary**, **White**
or **Custom #RRGGBB**. HOME/AWAY Team resolves to the NHL matchup's
home or away primary color; NEUTRAL Team resolves to the triggering
team. Automatic keeps default team/white colors from 8B.

### Timing and restoration

For goal, start, win and loss scenes, choose **0** to use existing
GameDay timing or **2–30 seconds** for a zone-specific duration.
Only currently participating zones count towards the event's WLED
duration. After a shorter zone ends, GameDay restores only that zone's
affected segment IDs to their saved state; longer zones keep running.
The last participating zone sets the event's maximum runtime.

Official power plays deliberately **do not allow a custom fixed
duration**: they remain active only while NHL evidence confirms a
man advantage (and can be interrupted by a goal). Speed and intensity
control the native WLED pulse behavior.

State restoration uses the same snapshot as 8B, filters by exact segment
ID and never changes LED boundaries, WLED's saved presets or unrelated
segments. The original full-controller restore path is retained for
unmapped legacy controllers. When all customized zones have already
restored, GameDay retires the snapshot cleanly without reporting a
false error. If any restoration POST fails, standard bounded retries
and the Activity diagnostic status apply.

### Safe manual testing

- **Preview saved scene** triggers a **three-second** isolated rehearsal
  of the chosen event+zone only, on the saved controller.
- A saved scene can be previewed without a real NHL goal. The speaker,
  screen and other WLED controllers are not triggered.
- Preview is blocked during an active event or ongoing restore.
- The controller must already be **ON**; GameDay will not globally
  power it on or wake unrelated strips just for a scene preview.
- Preview always snapshots WLED first; state restoration is requested
  after three seconds (or immediately if the POST fails).
- Make edits, **Save all scene templates**, and only then Preview.
  Unsaved draft changes are not used for preview.

## Storage, migration and resource limits

The scene library stores exactly 15 compact records (5 events ×
3 zones) per controller. It fits into one fixed, versioned
**142-byte, CRC-protected NVS blob** at key `sceneN`, where N=0..7.
Saved changes are validated as a complete set, written atomically
as one NVS key, then applied in RAM. Missing/corrupted blobs default
to all **Inherit** scenes.

Existing slots `ip0..7`, `enable0..7`, `event0..7`,
`wname0..7`, `zoneOn0..7`, `zoneMap0..7` are untouched.
No flash partition change, file-system migration, or new GPIO is
needed. WAV library, animated goal GIF, CYD rear LED, S3 external
case LEDs and the ES8311 onboard speaker are unchanged.

Runtime relies on fixed-size scene records, preexisting state
snapshots, and a rate-limited ~400 ms scan for zone expiration.
There are no additional background internet requests and **no new
NHL event detection**.

## Local authenticated APIs

| Endpoint | Behavior |
| --- | --- |
| GET `/api/wled/scenes?slot=N` | Saved 15 templates, zone enabled state, effect choices and default goal timing |
| POST `/api/wled/scenes` | Validate and persist all 15 scene templates in one CRC-protected blob |
| POST `/api/wled/scene-preview` | Preview one **saved** event+zone for 3 seconds and restore touched segments |

Example save body shape:

```json
{
  "slot": 0,
  "scenes": [
    {"event": 1, "zone": 1, "custom": true,
     "fx": 23, "speed": 220, "intensity": 180,
     "brightness": 100, "colorMode": 1,
     "hex": "#31837A", "duration": 6},
    {"event": 1, "zone": 2, "custom": false}
  ]
}
```

This shows the format of two entries. The real POST **requires all
15 event/zone pairs** (one for each unique pair), to prevent partial
reconfigurations. The browser sends the full validated draft.

The isolated preview accepts:
`{"slot":0,"event":1,"zone":1}`.
Roles are 1=HOME, 2=AWAY, 3=NEUTRAL. Events are 1=Goal,
2=Power play, 3=Start, 4=Win, 5=Loss.

## Acceptance tests after S3/CYD OTA

1. **Migration:** Open Saved controllers and confirm all existing
   WLED devices and zone mappings survived unchanged. An uncustomized
   scene must display **Inherit**.
2. **Default safety:** With 8C scene templates disabled, run goal
   previews on an existing mapped controller and verify 8B behavior.
   An unmapped controller must behave as the pre-8C legacy mode.
3. **Independent zone effects:** Configure HOME Goal = Strobe/team,
   AWAY Goal = Chase/team, NEUTRAL Goal = Solid/white. Save, reload,
   then preview each separately without altering other segments.
4. **Custom color:** Set an RGB hex color for a chosen scene and verify
   exact color channel order. Compare to WLED's own color picker.
5. **Per-zone durations:** Give HOME Goal 3s and NEUTRAL Goal 8s.
   Trigger a home-goal Test Lab preview. HOME should restore around 3s
   while NEUTRAL remains active until its timeout; both return to the
   prior colors/FX. Note that *single-zone* manual scene previews
   intentionally end after 3s regardless of configured live duration.
6. **Power-play evidence:** Confirm an armed home PP pulses only HOME
   and NEUTRAL until NHL indicates that PP ended. Preview should never
   create a synthetic NHL power-play event.
7. **All controllers:** Try 2–8 controllers, monitor display and audio
   responsiveness, free heap and restore latency. CYD RAM is tighter
   than S3, so the existing 45 KB heap floor may skip overloaded slots.
8. **Offline/reboot:** Save scenes, reboot, confirm persistence.
   Disconnect WLED during a preview, confirm restore failures are
   reported but the NHL scoreboard keeps functioning.
9. **Security:** Try submitting an invalid FX ID, corrupted/short JSON,
   duplicate event-zone pair or duration outside limits; the API must
   reject the edit without changing saved scenes.
10. **Team eligibility:** Only the NHL game armed for WLED triggers
    lights. Following an unrelated game or enabling league-wide screen
    events must not change live WLED eligibility.

**CI can compile both firmware targets and validate scene policy,
persistence checksums, API validation and legacy contracts. It cannot
simulate actual LAN effects, optic brightness, latency or power loss.**

## Next phase: 8D optional DDP

If the native JSON scene builder performs well on real WLED controllers,
8D can add optional per-pixel DDP streaming with explicit strip/range
ownership and a way to disable streaming at any time. DDP remains
entirely **absent** from 8C.

Reference: [WLED JSON API](https://kno.wled.ge/interfaces/json-api/).
