# Phase 8B — WLED Native Segment Zones

**Implementation and CI phase.** Hardware acceptance still requires testing
on real WLED controllers. No DDP transport or new NHL event eligibility.

## Objective

Keep WLED's existing segment definitions, names, start/stop ranges,
physical LED count and presets intact, while letting GameDay assign
each existing segment to one of three celebration zones.

- **HOME** — only home goals / home PP; matching home team color.
- **AWAY** — only away goals / away PP; matching away team color.
- **NEUTRAL** — either team's goals and power plays use the triggering team's primary color by default. An explicitly configured White scene remains white.
- **UNASSIGNED** — **never intentionally modified** by mapped GameDay effects.

For GAME START and FINAL, all assigned zones can participate; HOME and
AWAY use their respective NHL team's primary color; NEUTRAL uses the triggering/event team's color by default.
The NHL arm/goal filter remains exactly the same: only the armed game
triggers WLED and its configured team-side restrictions still apply.

## Backward compatibility: legacy by default

Existing devices are **unmapped by default**. Their original behavior
continues unchanged: every valid WLED segment participates in the same
effect with the legacy global on/brightness fields. The four previously
saved devices, four new slots, per-device goal/PP/start/final masks and
state restoration continue to work.

To opt in, open **Customize → WLED → Saved controllers → Manage segments**
on a configured device:

1. GameDay reads its real `GET /json/state` and displays the actual
   segment IDs, optional names and LED start/stop bounds. Stop indices
   are exclusive (e.g. 40–79 is `start=40,stop=80`).
2. For each detected segment choose Unassigned, Home, Away, or Neutral.
   Existing WLED segment geometry is not edited by GameDay.
3. **Identify** runs a ~1.8-second **white solid** preview on only that
   segment ID and restores that segment's original state. This uses the
   WLED JSON API, not DDP, and does **not** create NHL events.
4. Check **Enable segment-based routing** and select **Save zone assignments**.
   The firmware requires at least one assigned segment to enable mapping.
   Role changes are persisted in GameDay NVS under `zoneOnN` and
   `zoneMapN` for controller N=0..7.
5. Disable the mapping checkbox and save to return that controller to
   legacy whole-controller behavior. Stored roles can be reassigned later.

This release manages up to **16 WLED segment IDs per controller** and
**8 controllers**, with one 32-bit two-bit-per-segment role map per slot.
Boards with more than 16 segments display a truncation warning; do not
enable zone routing if the required segments are outside 0..15.

## Native JSON and restoration safety

In mapped mode, GameDay sends `POST /json/state` with a
`seg:[{id,fx,sx,ix,pal,col,bri}]` array containing **only matching IDs**.
WLED's segment `start`, `stop`, `len`, `sel`, presets and LED buses
are never changed. WLED UDP sync is suppressed for those updates.

Unlike the unmapped legacy mode, the mapped output does **not** send
global `on:true` or global brightness: it uses the segment brightness
property to avoid waking unrelated segments. This means **the WLED
controller must already be ON** for a mapped celebration or identify
preview. If it is OFF, GameDay declines to issue that segment effect
instead of activating another segment by mistake.

The device snapshots the prior WLED state before touching a segment
and maintains a 16-bit mask of **only the IDs GameDay attempted to
change**. On completion it restores the original FX, colors, palette,
speed, intensity, brightness and on/off values for only those IDs.
Unassigned segments keep running. The legacy full-state restore is
retained for an unmapped controller.

If a real celebration supersedes a preview, the existing shared WLED
event engine owns the transition; previews cannot be started while a
WLED event or previous restore is active. Failed restores retry up to
three times as before; warnings remain visible in Activity.

During WLED snapshots, the firmware enforces a minimum 45KB heap budget
to protect NHL updates on the smaller original CYD. This safeguards
against trying to hold eight giant states simultaneously, but **real
eight-device performance and HTTP latency still require hardware tests**.

## API (authenticated, S3 + CYD)

- `GET /api/wled/segments?slot=N`: live WLED segment inventory with
  name, id, `[start, stop)` LED range, controller-on flag and saved
  zone assignment. No WLED state modifications.
- `POST /api/wled/segment-config`:
  `{"slot":0,"enabled":true,"roles":[{"id":0,"role":1},{"id":1,"role":2},{"id":2,"role":3}]}`
  with 0=Unassigned, 1=Home, 2=Away, 3=Neutral.
- `POST /api/wled/segment-test`: `{"slot":0,"id":1}` to
  identify an existing segment. Rejects an inactive/off WLED and
  concurrent celebrations; temporary scoped restore after 1.8 s.
- Existing `/api/wled/preview` uses the **new role map** when mapping
  is enabled; use home/away goal tests after configuring zones.

No cloud dependency or independent PC/server is required.

## Required real-controller acceptance

1. Install the successful 8B `firmware.bin` for your exact CYD/S3
   target and verify all previously configured WLED controllers survived.
2. With zone mapping OFF, run a goal preview: it must behave exactly
   like the previous release.
3. Create 3 segments on one WLED with distinct ranges, e.g.
   0–39, 40–79, 80–119. **GameDay must never create or resize them**.
4. Press **Identify** on each; only that segment should briefly turn white
   (others continue their previous state). Confirm exact physical ranges.
5. Map segment0=HOME, segment1=AWAY, segment2=NEUTRAL, enable zones and
   save. Run a home goal preview: only HOME and NEUTRAL change.
   Run an away goal: only AWAY and NEUTRAL change.
6. Let the animation finish: confirm every changed segment returns to
   its previous effect, brightness and color; unassigned segments stay
   unchanged throughout. Compare WLED snapshots before/after.
7. Check a home PP and away PP; goal interrupts the PP and verified PP
   may resume after the goal, honoring the armed NHL event logic.
8. Reboot and confirm assignments persist. Turn zone mode off again
   and verify legacy controller behavior returns.
9. Disconnect a controller mid-effect. Check that activity reports
   restoration retries/failures without impacting the S3 scoreboard,
   speaker or case LEDs.
10. Repeat with up to 8 configured controllers and monitor heap/latency.

## Future: 8C and 8D

8C adds per-zone effect templates and celebration scene composition.
8D **optionally** adds DDP (UDP 4048) for explicit pixel-range mapping.
DDP is deliberately absent from 8A/8B: WLED realtime data doesn't
automatically honor the same JSON segment selection rules.

WLED protocol reference:
https://kno.wled.ge/interfaces/json-api/
