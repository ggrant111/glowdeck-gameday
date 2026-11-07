# GlowDeck GameDay — WLED Phase 8E: Light Streaming Quick Setup

**Goal:** Enable an existing WLED controller's DDP stream from the same
**Customize → WLED** card where the controller is managed. The older
**Manage segments** and **Test Lab** functions remain for advanced use.

## New user journey

1. Discover or verify the WLED controller and **Save celebration settings**.
   WLED must be online, ON, on the same LAN and use RGB LEDs.
2. On the saved device, select **Set up light streaming**.
   The live check reports LED count, chunk count and an actionable issue if
   setup cannot proceed. This check sends no pixel data or WLED commands.
3. Pick **Both teams · entire strip (recommended)** for a straightforward
   strip. The triggering NHL team's primary color is the default for goals,
   power plays, starts and finishes; white requires an explicit scene setting.
   Home-only and Away-only are available. If zone roles already exist,
   **Keep my existing segment assignments** is the default.
4. Pick 10, 15, or 20 FPS. For 1,000+ pixels, 10 FPS is recommended; the
   saved FPS remains selected for previously configured controllers.
5. Read and check the dedicated-controller consent box. Choose **Enable &
   test for 3 seconds**, or **Enable without testing**.
6. The firmware validates full LED coverage and compatible WLED live state;
   enables mapped zones, controller celebration participation, and DDP in
   one confirmed operation; and preserves the original scene definitions.
   If testing is selected, GameDay sends a three-second GAME_START
   diagnostic and automatically restores WLED.
7. **Advanced settings & safety** provides stop/restore, disable DDP,
   and a link to the full Test Lab.

## Safety and compatibility

DDP is opt-in and can take exclusive control of all pixels on a dedicated
WLED device during eligible events. Nothing automatically turns on from
device discovery or visiting Quick Setup. The readiness GET endpoint is
read-only. The confirmed POST endpoint stores only GameDay preferences;
it **does not modify the WLED LED ranges**, effects, or presets.

All saved segment roles survive by default. An explicit Home/Away/Both
choice updates the corresponding GameDay roles for all current segments;
it does not create new WLED segments. Existing mixed Home/Away mappings
can still route goal celebrations through native JSON to avoid erasing
other-team pixels. Unassigned pixels, overlapping segments, RGBW, current
third-party realtime streaming, controller OFF, or more than 4,096 pixels
block opt-in with an explanation. Existing armed-game eligibility, PP
(native JSON), WLED restoration and Fire TV behavior are unchanged.

The setup API verifies suitability and the live preview uses the same
3-second test endpoint as before; the UX does not bypass those checks.
Preferences writes have best-effort rollback on flash errors. The user
must confirm the controller is exclusively used for GameDay streaming.

## Routes and firmware

- `GET /api/wled/ddp/setup?slot=0`: live read-only status, LED count,
  estimated packet count, existing zone/DDP state and actionable reason.
- `POST /api/wled/ddp/quick-setup` with
  `{"slot":0,"mode":3,"fps":10,"exclusiveConfirmed":true}`
  confirms exclusive use, validates geometry and duration, then stores
  zone/DDP/participation in one action. `mode=3` is both teams (Neutral),
  `1` Home, `2` Away, `0` keep existing roles.
- Existing `/api/wled/ddp/test`, `/api/wled/ddp/stop`,
  `/api/wled/ddp/config` remain supported.

Both new routes use the existing local dashboard authorization.
Phase 8E introduces no new NVS keys and runs on the CYD and S3.

## Hardware acceptance

Check 300, 1,000 and 2,048 RGB LEDs where possible. Confirm that
**Both teams** covers the full strip, that a three-second test returns to
the prior effects, and that an existing Home/Away split defaults to
**Keep existing assignments**. Try a WLED that's OFF, a partially
assigned LED layout, a controller receiving another stream, and RGBW:
Quick Setup should show why enabling is blocked without changing any
settings. Finally, confirm an NHL goal triggers only for an eligible,
armed game. Hardware acceptance is **not** implied by CI.
