# Philips Hue · GameDay Phase 9

GlowDeck GameDay supports Philips Hue **alongside** existing WLED, rear LED and S3 case LED outputs. It does not require Hue Cloud, HomeKit, an Entertainment Area, or a third-party server.

## Getting started

1. Connect the GlowDeck controller and **Hue Bridge** to the same trusted LAN. A Hue Bridge (v2 / CLIP v2 capable) is required; Bluetooth-only Hue lights are not supported.
2. In **Customize → Philips Hue**, select **Find bridges**, or type the Bridge's local IPv4 address. mDNS might be blocked by some routers; manual IP always works.
3. Press the **physical link button** on the Bridge and select **Pair bridge** shortly afterward. GameDay securely stores the resulting application key in its on-device NVS and does not expose it through the dashboard's GET APIs or serial logs.
4. Select **Refresh lights**. Choose **up to four** light resources, set the event mask and brightness, enable Hue, and select **Save Hue lighting**. WLED slots and scenes do not change.
5. Open **Test Lab → Hue**, test both teams' colors and a power play, then **Stop & restore**. A power-play preview stops automatically after nine seconds. A successful HTTP response confirms command acceptance only, **not visible LED output**.

## Behavior

- **Goals:** chosen lights display the scoring team's NHL primary color, with game event eligibility restricted to the same armed-game/home-away rules as existing celebrations.
- **Power plays:** the confirmed man-advantage team color gently pulses (no rapid Hue Entertainment streaming). Intermissions and lost PP eligibility restore the original lights; returning to a verified PP reactivates them.
- **Game start and final:** optional NHL team-colored scenes.
- The lights retain an original state snapshot and restore their prior on/off, brightness and color (or white color-temperature, where available), including after preemption from power play to goal.
- Local HTTP commands, snapshots, state restoration and failures appear in the Hue Test Lab, the Activity event history and USB serial audit. The last 64 NHL events are session-only, as before.
- Hue is opt-in, does not change NHL event eligibility, and does not modify WLED's 8 controllers, zones, JSON effects or DDP streams.

## Compatibility, scope and security

- The ESP32 firmware speaks **Hue Bridge CLIP v2 local HTTPS** for lights and issues a v1 `POST /api` **only to obtain its local application key**, after a physical link-button press.
- Local IPv4 requests are constrained to the connected Wi-Fi subnet, so public IPs and remote cloud endpoints are rejected.
- Bridge self-signed/mismatched certificates are **not independently authenticated** by the ESP32 firmware. Use only a trusted home network; the bridge key still travels over encrypted TLS. For stronger device identity, future releases should pin bridge certificates/IDs.
- Bridge light writes are HTTPS **requests**, not high-rate Entertainment-area streaming. Different Hue models may respond differently to brightness transitions. Non-color Hue lights use brightness-only effects.
- Up to **four** individually selected Bridge light resources are supported by this first release. Hue Bridge group and entertainment area selection, certified TLS verification, remote access and discovery beyond local mDNS are out of scope.
- On-device light changes cannot be tested without the user's Bridge and bulbs. Verify physically using Test Lab before relying on live NHL events.

## Diagnostics

If pairing reports a 101 (link-button) error, press the bridge button and immediately retry. If discovery finds nothing, set a reserved/static bridge IP in your router and enter it manually. If bulbs remain unchanged, verify they are selected and Hue is enabled, then check Hue HTTP status in activity/serial. A restore failure means the previous lighting may require manual reset. Keep GameDay and Hue Bridge on the same home LAN and avoid an untrusted guest Wi-Fi network.
