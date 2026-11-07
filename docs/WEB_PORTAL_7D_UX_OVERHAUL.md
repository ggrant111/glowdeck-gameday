# GlowDeck GameDay — Phase 7D web portal UX overhaul

**Scope:** A complete user-facing IA and visual refresh of the embedded
local, offline-capable GameDay dashboard on the Waveshare S3 and CYD.

## Audit: what was confusing

The old left navigation had **eight first-level pages**:

| Before | Problem |
|---|---|
| Overview | Live status next to two WLED test buttons and ten low-level health stats |
| Live games | Useful game search, stars, arm and stats actions |
| Favorites & alerts | Screen options mixed with manual overlay-preview actions |
| WLED celebrations | WLED device configuration mixed with real controller test commands |
| Rear RGB LED | Following settings, color palette, channel diagnostics and combined tests on one page |
| Activity | Useful event log |
| Device settings | Wi-Fi, WLED intensity, CYD contrast, admin password, S3 WAVs, speaker tests, GPIO case wiring and GIF settings mixed together |
| Firmware update | Firmware and restart on a disconnected tab |

The same action looked different by board. Multiple `Save` buttons had
overlapping scope, leading to uncertainty about what was persisted.

## New architecture

The 6 top-level destinations have a clear, mutually exclusive job:

| New page | Primary user question | Internal sections |
| --- | --- | --- |
| **Overview** | What's happening right now? | Arena-inspired featured game, live status, shortcuts, recent events, armed-game mode |
| **Live games** | Which games am I following or arming? | Daily NHL games, search, filters, follow stars, arm and details |
| **Customize** | How should GameDay behave? | Favorites & Following, Screen, WLED, Sound, On-device LEDs |
| **Test Lab** | Does it actually work? | Screen, Audio (S3), WLED, LED hardware (board-aware), Full celebration |
| **Device** | How do I maintain/connect the controller? | Wi-Fi & Access, Firmware & Maintenance, Connection & health |
| **Activity** | What happened? | Filterable 64-event session log, command status and recent history |

The nested sections use a **single-active-category** segmented navigation,
not six long stacked pages. On small screens the category rail scrolls
horizontally. The active page is indicated visually, with reduced-motion
support and large touch targets.

### What moved to the Test Lab

- Screen: GOAL, PP start/end, game start, final win/loss, dismissal;
  S3 installed GIF preview.
- Speaker (S3 only): current goal sound, independent 1 kHz tone, codec
  diagnostics. Contextual WAV audition remains in the upload library.
- WLED: goal home/away, PP, game start, win/loss, restore and quick team color.
- CYD physical LED: game/team goal, PP, individual R/G/B, off.
- Waveshare S3 external case LEDs: separate home/away goal, PP, individual
  R/G/B, off, **only if wired and explicitly enabled**.
- Full celebration: combined WLED + screen (+ CYD rear RGB when available),
  and stop/restore.

These actions retain the **existing** endpoints. In particular, manual
test actions are not treated as observed NHL goals and do not change
follow or arm state.

## Visual system: broadcast, not generic admin

The redesigned visual language is based on the Waveshare S3 native 480×320
broadcast scoreboard: midnight navy `#070E18`, ice-blue readouts,
team-primary accents, segmented score readouts, and restrained celadon
green from the existing GlowDeck palette. The featured-scoreboard card
uses actual NHL game data already fetched by `/api/games`; no extra NHL
network request or duplicate event logic is introduced.

Controls are mobile-first with keyboard focus, accessible navigation,
reduced-motion support, single-purpose headings, clear state/preview
separation, and a visible save-state hint.

## Compatibility & saving behavior

The change is **HTML/CSS/JavaScript-only** in `include/WebUi.h`. It
does **not** modify any NHL APIs, TCP/WLED logic, Wi-Fi/OTA endpoints,
ES8311 driver, goal GIF player, CYD pins, case LED GPIO or NVS key names.
It reuses the **existing DOM control nodes**, reparented into a
declarative task/category map by `mountPortalModules()`. It never
clones a Save/Preview button or duplicates its callback. If adding a new
feature, insert a normal `.card` in its existing section or component,
then map its root control to a new portal destination.

- **Save settings** in Customize and **Save device settings** in Device
  both use the existing `saveConfig()` and `/api/config`, gathering the
  full set of loaded WLED/CYD/display/horn/network fields. Editing other
  tabs before saving does not discard those changes. A visible amber
  dirty-state hint appears for these shared settings.
- Favorite team selection still has its own **Save favorite teams**
  action (existing `/api/display/favorites`).
- S3 speaker volume uses its existing **Apply volume** action.
- S3 WAV upload/use/delete writes immediately to sound library/NVS.
- S3 external case LED configuration uses **Save case lighting
  configuration** with required wiring confirmation.
- These dedicated controls are clearly called out in the Customize
  footer; the shared button does not falsely claim to persist them.

Older in-dashboard `data-tab="display"`, `"devices"`, `"rear"`,
`"settings"`, and `"firmware"` routes resolve to their new task
categories, including a direct section jump. No hard-coded old
`#display` link breaks user workflows.

## Regression and hardware acceptance

Automated:
- Node syntax validation of embedded JavaScript.
- Unique static DOM IDs and zero orphaned literal `$("...")` selectors.
- All six routes, legacy route redirects and thirteen section destinations.
- Correct source → destination moves for every class of test.
- CYD vs S3 feature-specific visibility and no duplicated controls.
- Shared Save compatibility, unsaved-change indicator and actual NHL game
  scoreboard rendering rather than simulated values.
- Existing firmware integration tests and **both** CYD and Waveshare
  S3 PlatformIO builds.

Manual acceptance **after OTA** (CI is not a browser screenshot or a
physical touchscreen test):
1. Open the web dashboard on phone and desktop. Verify six-page nav.
2. Browse a game and tap ★, then return to Overview. Follow vs WLED Arm
   must remain separate.
3. Open Customize and use each of the five subcategories. Change WLED
   brightness, confirm the unsaved indicator, save, reload and verify it
   persists. Verify another section's loaded fields were not cleared.
4. From Customize use **Screen tests**, **Test WLED** or **Audio tests**
   shortcuts. Confirm Test Lab opens the corresponding category.
5. On S3, verify speaker tests and case-LED controls appear; on CYD,
   the speaker codec/case test categories must not appear and CYD rear
   RGB tests remain available.
6. Keep the unbuilt S3 case outputs **disabled**: their new location must
   not change the Phase 7C external GPIO safety state.
7. Verify successful WAV upload, selected sound preview, goal GIF preview,
   WLED restore, admin password change, local OTA and reboot.
8. Check browser console for JS errors, confirm viewport layout at 320,
   375, 768 and desktop widths; test keyboard/screen reader focus.
9. Confirm NHL event polling and device UI are unchanged while portal
   navigation and Test Lab are active.

## Rollback

This is a UI-only release. In an unexpected web UI regression, install
the last verified S3 or CYD `firmware.bin` using the existing
Firmware / OTA method, or use the previous GitHub tag/commit through
PlatformIO. No database migration or flash partition change is involved.
