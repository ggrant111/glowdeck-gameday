# Phase 8A — WLED Discovery & Eight-Controller Management

**Status:** Firmware implementation + CI; physical LAN verification pending.
This release must not change NHL goal/PP/start/final eligibility, or
trigger any lights simply because a device was discovered.

## Capabilities

1. Navigate to **Customize → WLED → Discover & connect** and select
   **Scan Wi-Fi for WLED**.
2. GlowDeck performs an on-demand DNS-SD lookup of the official
   `_wled._tcp` mDNS service using its existing ESPmDNS responder.
   Results are restricted to the device's own Wi-Fi subnet.
3. Each discovered address is *verified* via `GET /json/info` with
   a bounded HTTP timeout and a required WLED version / LED count.
   GameDay displays the reported name, version and LED count.
   Configured controllers appear in scan results even if offline.
4. Choose **Add to GameDay**. This stages the IP and friendly name
   into the first unoccupied slot but **keeps the controller OFF**.
   Review the event permissions, toggle participation explicitly,
   and choose **Save settings** before NHL celebrations use it.
5. Devices not advertising mDNS can be added via **Manual IPv4 →
   Verify IP**. This checks the same WLED JSON endpoint before staging.
   The original manual edit fields remain for temporarily offline
   controllers, but no untrusted web URL is ever used by discovery.
6. All 8 controller slots support independent IP, friendly name,
   on/off opt-in, and goal/PP/start/final event checkboxes.

The app does *not* turn lights on, change WLED segments or initiate
a celebration during discovery. There is no continuous LAN sweep;
the scan is started only by a dashboard action, rate-limited to one
every 15 seconds and processed incrementally by the existing device
loop. WLED events, restoration and NHL polling take priority.

## Safe migration

The old 4 controller NVS keys `ip0..ip3`, `enable0..enable3` and
`event0..event3` remain unchanged. Slots `4..7` simply use the same
key pattern. New optional local names use `wname0..wname7`.
No preferences erase, partition change or controller reset is needed.

Phase 8A does **not** modify what constitutes an actual armed-game
NHL celebration. Display-only games remain display-only; starring a
game doesn't automatically arm WLED. Event log result capacity was
expanded to report eight controllers with the same per-controller
pending/accepted/restore statuses.

## HTTP endpoints

- `POST /api/wled/discover`: begins an on-demand, asynchronous scan.
  Does not change saved settings.
- `GET /api/wled/discovery`: scan status and verified candidates.
- `POST /api/wled/probe`: validate a local-subnet IPv4
  `{"ip":"192.168.1.71"}`, returns WLED name/version/LED count.
- Existing `GET/POST /api/config` supports eight `wled[]` objects,
  with optional `name` field.
- Existing `POST /api/wled/preview` unchanged; only opted-in and
  event-eligible devices participate.

All new endpoints require the existing GameDay dashboard authorization.
The mDNS query happens once per requested scan; at most 12 candidate
IP addresses are retained and probed, one per loop turn. Discovery does
not follow links, redirects or hostnames from responders.

## Physical acceptance after OTA

- Confirm your four existing controllers and event options still appear
  unchanged and still operate.
- Verify a discovered controller's reported name, IP, LED count and
  version match its own WLED web UI.
- Stage a new controller: it must stay OFF until explicitly enabled and
  saved. Reload the page and confirm its settings persist.
- Test a controller with mDNS disabled using manual IPv4 verification.
- Try an offline WLED; it must be labeled offline, not auto-deleted.
- Disconnect WLED during a scan. The NHL scoreboard and existing
  audio/case LED safety rules must remain operational.
- Verify status/event logs now show up to eight devices.
- With 5–8 live controllers, measure restore latency and free heap.
  The firmware compiles for both CYD and S3, but only real devices
  can validate multi-controller latency and LAN behavior.

## Next phase

Phase **8B** reads the existing WLED `/json/state.seg[]` list,
identifies exact LED ranges and provides HOME, AWAY and NEUTRAL
assignment without changing the user's original WLED strip layout.
An unmapped legacy controller must continue using the original
whole-controller celebration behavior.

Reference: [WLED JSON API](https://kno.wled.ge/interfaces/json-api/)
and WLED official mDNS `_wled._tcp` registration.
