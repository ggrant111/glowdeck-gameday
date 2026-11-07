# GlowDeck GameDay — WLED Phase 8D: Optional DDP Realtime Streaming

**State:** DDP transmitter and safety controls in CYD/S3 firmware; hardware
LAN acceptance pending. **OFF by default on every controller**.

## Why the ownership requirement matters

Unlike WLED's `/json/state` targeted segment commands, DDP can replace
the **whole LED output** while WLED is in realtime mode, including
pixels belonging to other WLED segments or programs. Segment IDs in
8B are *not* preserved automatically by real-time DDP. Sending only
pixels belonging to one Home segment is **not** a safe way to protect
the remaining Away/neutral segments.

GameDay therefore only uses DDP for a controller that the user explicitly
dedicates **exclusively to GameDay**, with **every pixel** owned by a
valid WLED Home, Away or Neutral segment that is eligible for that
particular NHL event. One unassigned LED, gap, overlap, opposite-team
segment, duplicate segment ID or unsupported WLED strip disables DDP
for that event. The native-JSON scene builder remains fully usable.

The DDP transmitter sends RGB24 frames with the [official 10-byte
header](https://kno.wled.ge/interfaces/ddp/): version/push `0x41`,
sequence, type **`0x0B`**, destination `0x01`, big-endian byte offset
zero and big-endian payload length. Network destination is **UDP 4048**.
It is implemented locally on the S3/CYD; no PC, server or cloud
software is needed. RGBW strip output is deliberately excluded.

## Which events use DDP?

| Existing event | DDP eligibility | Safe fallback |
| --- | --- | --- |
| Home goal | Only when **all** LEDs belong to Home or Neutral eligible zones | Native JSON for mixed Away segments |
| Away goal | Only when **all** LEDs belong to Away or Neutral eligible zones | Native JSON for mixed Home segments |
| Game start | All assigned Home/Away/Neutral pixels eligible, no gaps | Native JSON on incomplete maps |
| Final win/loss | All assigned Home/Away/Neutral pixels eligible, no gaps | Native JSON on incomplete maps |
| Verified power play | **Never DDP** in 8D; PP has an open-ended NHL-controlled duration | Original native JSON PP pulse |

For temporary events, DDP also requires all active zones to have the
**same effective scene duration**. If Home and Neutral have 3s and
8s, the per-zone native JSON engine handles different expiries and
restores correctly; it does not attempt unsafe partial DDP output.

When eligible, DDP renders 8C's saved Solid/Breathe/Wipe/Fade/Twinkle/
Strobe/Chase styles with each zone's team or custom color, speed,
intensity and brightness. These are light-weight **approximate
realtime pixel patterns**, not WLED's exact built-in effect algorithm.
No scene is automatically modified by the upgrade.

## Runtime and resource limits

| Parameter | Default / limit |
| --- | --- |
| DDP enabled | **False** until exclusive-use confirmation |
| LEDs per DDP controller | **1–300 RGB pixels** |
| Frame rate | 10, **15**, or 20 FPS |
| Simultaneous DDP streams | **2** per S3/CYD |
| Maximum DDP event duration | **30 seconds** |
| Manual Test Lab preview | **3 seconds**, then stop/restore |
| Max UDP packet | **910 bytes** (10 header + 300×3), MTU-safe |
| Native JSON controllers | **Up to 8**, unchanged |
| WLED source | Local same-subnet IP only, verified `/json/info` |

When a controller cannot safely use DDP (including third-party realtime
already active at event start, an unsupported RGBW layout, incomplete
ranges, Wi-Fi interruption or more than 2 simultaneous streams), it
continues using **8C native JSON**. Realtime does not mean native
JSON behavior is removed or that any extra game is armed.

## Setup

1. Update GameDay S3 or CYD to the successful 8D firmware. Check existing
   8C WLED devices/scenes and 8B zones remain unchanged.
2. Open **Customize → WLED → Saved WLED controllers → Real-time DDP · OFF**.
   Prefer a **dedicated WLED controller** that is not used for TV
   ambient lighting or other non-GameDay activity.
3. Confirm in WLED itself that all RGB pixels are described by existing
   **non-overlapping segments** covering LEDs 0 to N−1; assign each
   segment Home, Away or Neutral in GameDay.
4. Set frame rate: **15 FPS** suggested for Wi-Fi. Enable the DDP checkbox.
   Review and approve the explicit warning that **all pixels may be
   overwritten**, and choose **Save DDP preference**.
5. GameDay checks WLED `/json/info`, original `/json/state`, that
   it is ON and not already in realtime mode, pixel bounds and
   exclusive zone coverage before accepting the opt-in.
6. Open **Test Lab → WLED**. Select that controller, press **Test DDP for
   3 seconds**, then verify it returns to its previous animation.
   **Emergency stop & restore WLED** requests restoration across all
   WLED controllers; it does not stop NHL scoreboard polling, the S3
   speaker or case LEDs.
7. Enable real celebrations for your **armed** game as before. Every
   DDP attempt is re-validated against current WLED state and live
   pixel geometry; unsafe situations automatically fall back to JSON.

New saved NVS keys are `ddpOn0..7` and `ddpFps0..7`.
An absent key always disables DDP. If a saved controller IP changes
or segment mapping is disabled, DDP is automatically turned OFF to
avoid carrying exclusive-controller consent to another device.

## HTTP API

All routes require the existing dashboard Basic Auth or permitted
GlowDeck setup hotspot:

- `GET /api/wled/ddp?slot=0`: configured, enabled, FPS, streaming
  status, frame count, LED count and fallback reason
- `POST /api/wled/ddp/config`: `{"slot":0,"enabled":true,
  "fps":15,"exclusiveConfirmed":true}`
- `POST /api/wled/ddp/test`: `{"slot":0}` — manual three-second
  WLED Game Start style preview with no NHL event history
- `POST /api/wled/ddp/stop`: emergency stop/restore for WLED effects

Live DDP is entered only **after** GameDay successfully snapshots the
controller and validates the exclusive pixel map. To end streaming,
GameDay explicitly sends WLED `{"live":false,"udpn":{"nn":true}}`
before posting the original **mapped segment restoration** data.
If WLED cannot be reached, GameDay stops sending UDP and reports
restore failure/retry; WLED's own realtime timeout is a final backstop,
not a guarantee that a failed network restoration preserved every
effect perfectly.

## Tests and deferred physical acceptance

CI tests **DDP byte order and framing**, frame length/MTU,
Home/Away/Neutral ownership, missing/overlapping/duplicate ranges,
role-policy fallback, RGB brightness scaling, 10–20 FPS limits,
authenticated routes, and unchanged NHL eligibility. Builds run for
both the CYD and Waveshare ESP32-S3.

When your WLED devices are available, verify all the following:

1. On a **non-dedicated** Home+Away segmented controller, a Home goal
   must use native JSON, leaving the Away LEDs untouched.
2. On a dedicated all-Home strip of at most 300 LEDs, enable DDP and
   verify the manual three-second realtime test and state restoration.
3. On a dedicated full-strip Home+Away layout, test Game Start. Check
   the Home and Away regions show their team colors and previous
   effects return; goal events should fall back to JSON when the
   opposite side's pixels need protection.
4. Test missing/unassigned LEDs, RGBW controller, over-300 LEDs and
   controller OFF: DDP must not turn on, and status should explain why.
5. Try different 8C zone duration settings and confirm native JSON
   fallback when a single DDP lifetime cannot safely represent them.
6. Test 10/15/20 FPS and 1–2 streams; confirm the NHL scoreboard,
   goal horn, S3 case LEDs and Wi-Fi remain responsive.
7. Unplug Wi-Fi or reboot WLED midstream; verify status/restore
   behavior, original WLED effects and no persistent realtime lock.
8. Verify DDP stays OFF after restart unless explicitly enabled, and
   resets to OFF when controller IP or segment zone mapping changes.
9. Confirm with an **armed NHL game** that only eligible game events
   send external WLED commands; simply following an unrelated game
   or league-wide display events never starts DDP.
10. Check the UI at mobile and desktop widths and repeat the Test Lab
    emergency-stop flow.

**Hardware behavior is not certified by CI.** This release requires
explicit user acceptance on real WLED controllers before using DDP
on lighting that also serves another system.

Sources:
- https://kno.wled.ge/interfaces/ddp/
- https://kno.wled.ge/interfaces/json-api/
- https://docs.hyperion-project.org/user/leddevices/network/wled.html
