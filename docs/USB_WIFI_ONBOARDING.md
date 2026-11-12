# Guided first-run setup: USB Improv Wi-Fi + on-screen QR recovery

## User experience

GlowDeck GameDay supports two ways to finish Wi-Fi setup **without requiring an account, companion app, or separate PC server**.

### Preferred path: continue in the USB installer

1. Open the GitHub Pages [GlowDeck installer](https://ggrant111.github.io/glowdeck-gameday/) using a compatible Web Serial browser (desktop Chrome/Edge) and connect a supported board by USB data cable.
2. Select **Install & set up GameDay**, choose the COM/serial port, and install the matching factory firmware.
3. Keep USB connected. On reboot, ESP Web Tools detects the firmware's **Improv Wi-Fi Serial** service, and offers **Configure Wi-Fi**. The published manifest enables `"improv": true` for both chip families and waits up to 20 seconds to detect the board.
4. Select/enter the **2.4 GHz SSID** and password in ESP Web Tools. The browser sends credentials directly to the USB device via Improv Serial; there is **no Wi-Fi credential submission to GitHub Pages**.
5. The ESP32 joins the requested network. Only after a successful association and DHCP does it save the existing `ssid` and `wpass` NVS keys. **Incorrect credentials do not overwrite the previous saved Wi-Fi.**
6. Improv returns the ESP32's current `http://<LAN-IP>/` URL; click **Visit device** to continue the rest of GameDay configuration on the LAN. The local home-LAN web UI still uses the saved username `admin` and its existing admin password. For a fresh installation, the initial password matches the hotspot password shown under **SETUP** on the display; using USB Improv does not weaken existing HTTP authentication.
7. Configure NHL games, WLED, Hue, sound, and team preferences in the existing dashboard.

**Important:** An older (pre-Improv) release will still install but will not offer this USB Wi-Fi flow. The manifest's feature flag only works with compatible firmware built with the new serial handler.

### Backup path: scan the hotspot QR code

If the board cannot connect to home Wi-Fi:

- A **new/factory installation** opens the SETUP screen with a QR code immediately.
- If a configured board **loses Wi-Fi continuously for 20 seconds**, it navigates to SETUP once, rather than covering the NHL scoreboard on every brief dropout. The QR is also available any time you manually open the SETUP screen while offline.
- The QR content uses the standard Wi-Fi URI `WIFI:T:WPA;S:GlowDeck-XXXXXX;P:GDXXXXXX99;;` (device-specific values), with four modules of white quiet zone, rendered by a compact C QR library (version 3, low ECC). It encodes the **WPA2-protected GlowDeck setup hotspot**, not home-Wi-Fi credentials.
- Scan it using an iOS/Android camera, tap **Join** when your operating system asks, then open `http://192.168.4.1` in your browser and configure home Wi-Fi in **Device → Wi-Fi & Access**.

The camera **cannot silently force a network connection**; confirming the join prompt is an OS safety step. Joining the GlowDeck hotspot is a last-resort setup path when USB provisioning is not possible. The hotspot's WPA2 password remains required and its setup-AP browser exception remains restricted to the actual AP interface and subnet.

### Board parity

| Behavior | Original CYD | Waveshare S3 ST7796 |
|---|---|---|
| Improv via USB Serial | Yes, 115200 baud UART | Yes, native USB CDC |
| Scan and enter Wi-Fi through ESP Web Tools | Yes | Yes |
| Save only after connection success | Yes | Yes |
| Return local web dashboard URL | Yes | Yes |
| Offline setup QR code | 111×111 px at right of Setup screen | 111×111 px at right of Setup screen |
| No-password leak in provisioning serial logs | Yes | Yes |
| Hotspot remains available as recovery | Yes | Yes |

**Not supported:** Waveshare 3.5B (different display controller), unsupported CYD board variants, automatic Wi-Fi setup from an iPhone through ESP Web Tools USB Serial, automatic WLED/Hue discovery before GameDay joins the home network.

## Implementation and compatibility

- Uses [Improv Wi-Fi Serial](https://www.improv-wifi.com/serial/). **Waveshare S3** retains the original `jnthas/Improv WiFi Library@0.0.4` over native USB CDC, as confirmed working on physical hardware. **Original CYD** uses `GameDayImprovSerial.h`, a small, checked serial-frame handler over the CYD USB-UART bridge. The separate CYD implementation resets its parser before acting on each RPC, avoiding loss of the first byte in the next installer command. No BLE or secondary UART required.
- A regression test passes consecutive **state → info → networks → Wi-Fi settings** serial frames and checks checksum recovery, while the CYD display's offline redraw path forces the QR setup screen instead of the empty NHL carousel. The **S3 working path is intentionally unchanged**.
- Calls `improvSerial.handleSerial()` regularly from the main loop, bounded to up to 32 bytes per iteration. No new background service and no new user-facing HTTP API.
- A Wi-Fi attempt is bounded to 12 seconds; during active WLED celebrations, DDP streams, or pending restores, re-provisioning is rejected rather than interrupting lighting restoration.
- A failed connection attempts to restore previously saved Wi-Fi; only success updates `cfg.ssid`, `cfg.wifiPassword` and their two existing NVS keys. All other preferences remain unchanged.
- The S3 native 480×320 SETUP screen and CYD 320×240 SETUP screen both draw the QR on a quiet white background without changing touch navigation or OLED/TFT event logic.
- New build dependencies are pinned in **both** PlatformIO environments. CI builds both and checks the onboarding feature contract; no saved preferences migration or partition change is required.
- The older password-protected SoftAP still starts automatically, so a failed install or incorrect home password does not permanently lock a user out.
- For security, only trusted people should have physical USB access; setting Wi-Fi over local serial is an authorized possession-based operation. Do not put your *home* Wi-Fi password into bug reports or public serial logs. The setup hotspot password is separately shown on the device.

## Real-device test checklist

1. Flash the new **factory** image on a spare/erased original CYD. Confirm first boot displays a QR while offline.
2. Confirm the installer detects the device via Improv, offers Wi-Fi, saves a valid 2.4 GHz SSID, and returns the correct dashboard URL. Open it and verify home-LAN HTTP Basic auth.
3. Repeat on the supported Waveshare S3, including USB CDC port reconnection after flashing.
4. Try an incorrect password, an SSID not in range, and intentionally unavailable Wi-Fi. Confirm it does **not save credentials** and the hotspot QR is still usable.
5. Scan the QR using iOS and Android, accept the join prompt, and load `http://192.168.4.1`. Confirm WPA2 hotspot password is not typed manually.
6. On a configured board, temporarily disable home Wi-Fi. A brief outage must not take over the scoreboard; after 20 seconds offline SETUP should show the QR once. Reconnect and verify GameDay resumes normal updates.
7. Test WLED goal and PP animation, Hue, sound, and display while Improv is idle. During an ongoing WLED event, a USB re-provision attempt must not interrupt restoration.
8. Reboot, verify Wi-Fi and all saved settings (game follows, WLED/Hue, audio and admin password) remain intact.
9. Use an **app-only OTA** upgrade from an older release to this version. Ensure normal preferences remain, and the new Improv USB flow becomes available **without a factory reset**.

CI **cannot** prove a real phone decodes the QR correctly or that ESP Web Tools successfully provisions the physical board. These remain actual-hardware acceptance steps.
