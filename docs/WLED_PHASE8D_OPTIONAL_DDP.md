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

The DDP transmitter uses RGB24 data with the [official 10-byte
header](https://kno.wled.ge/interfaces/ddp/) on **UDP 4048**. Each frame
is split into consecutive chunks of at most **480 RGB pixels / 1,440
channel bytes**. Each chunk has DDP v1 flags `0x40`, except the **last
chunk only**, which uses `0x41` (PUSH) to display the full frame.
Sequences advance per packet through 1–15 (never zero); data type is
`0x0B` (RGB24) and destination ID `0x01`. The 32-bit big-endian
channel offset is `startingPixel * 3`; the 16-bit big-endian data
length is the number of RGB bytes **in that packet**, not the whole
frame. Neither an optional timecode nor a PC/server is required.
Packets are at most **1,450 bytes** total and fit ordinary 1,500-byte
Ethernet MTU links without IP fragmentation.
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
| LEDs per DDP controller | **1–4,096 RGB pixels** (implementation cap) |
| Frame rate | 10, **15**, or 20 FPS |
| Simultaneous DDP streams | **2** per S3/CYD |
| Maximum DDP event duration | **30 seconds** |
| Manual Test Lab preview | **3 seconds**, then stop/restore |
| Max UDP packet | **1,450 bytes** (10 header + 480×3), MTU-safe |
| DDP packets per full frame | **1–9** (at most 480 LEDs per packet) |
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
4. Set frame rate: **10 FPS recommended for 1,000+ LEDs**; 15 FPS is the
   default and 20 FPS should be used only when your LAN and controller
   handle the load smoothly. Enable the DDP checkbox.
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
  status, frame count, packet count, chunks/frame, LED count and fallback reason
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
480-pixel boundaries, last-chunk PUSH, sequence wrap, 1,000–4,096 LEDs,
Home/Away/Neutral ownership, missing/overlapping/duplicate ranges,
role-policy fallback, RGB brightness scaling, 10–20 FPS limits,
authenticated routes, and unchanged NHL eligibility. Builds run for
both the CYD and Waveshare ESP32-S3.

When your WLED devices are available, verify all the following:

1. On a **non-dedicated** Home+Away segmented controller, a Home goal
   must use native JSON, leaving the Away LEDs untouched.
2. On a dedicated all-Home RGB strip of 1,000 LEDs, enable DDP and
   verify the manual three-second realtime test and state restoration.
3. On a dedicated full-strip Home+Away layout, test Game Start. Check
   the Home and Away regions show their team colors and previous
   effects return; goal events should fall back to JSON when the
   opposite side's pixels need protection.
4. Test missing/unassigned LEDs, RGBW controller, over-4,096 LEDs and
   controller OFF: DDP must not turn on, and status should explain why.
5. Try different 8C zone duration settings and confirm native JSON
   fallback when a single DDP lifetime cannot safely represent them.
6. Test 10/15/20 FPS and 1–2 streams at 480, 1,000, 2,048 and 4,096
   LEDs; confirm the NHL scoreboard,
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

## Performance and limitations

DDP has no 300-pixel protocol ceiling. The 4,096 RGB pixel cap is a
conservative, explicit GlowDeck firmware setting (not a WLED limit).
Chunking limits packet sizes but does **not** compress RGB frames or
remove Wi-Fi bandwidth requirements. As a rough sender-side example,
4,096 pixels at 20 FPS is ~246 kB/s of RGB payload **per device**, or
~1.97 Mbit/s before UDP/IP, Wi-Fi overhead and retransmission effects.
Two such streams plus normal NHL/API and web traffic can overload a
2.4-GHz ESP32 or a busy wireless LAN; prefer 10 FPS and test real
hardware. Each controller's frame is transmitted sequentially without
a pixel-sized framebuffer; packets can be dropped because DDP uses UDP.
A missing chunk can produce an incomplete frame, but the next full
frame replaces it. Packet counts in Test Lab help diagnose traffic;
they do not guarantee remote receipt. Native JSON safety fallback and
NHL event eligibility are unchanged.

Sources:
- https://kno.wled.ge/interfaces/ddp/
- https://kno.wled.ge/interfaces/json-api/
- https://docs.hyperion-project.org/user/leddevices/network/wled.html
