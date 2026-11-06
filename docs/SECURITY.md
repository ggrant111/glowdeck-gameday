# Network and security notes

GlowDeck GameDay v0.1.0 is intended for a **trusted home LAN**.

- The CYD starts a WPA2 setup access point called \`GlowDeck-XXXXXX\`. The per-device password is shown under **SETUP** on its display. The hotspot remains active so a lost home Wi-Fi connection does not lock you out.
- **Direct setup-hotspot access:** after joining the WPA2-protected `GlowDeck-XXXXXX` Wi-Fi network, open `http://192.168.4.1` without a second browser/admin password prompt. The firmware allows this only when the request arrived at the SoftAP's local IP from an IP in that AP's subnet. The hotspot itself **still requires its Wi-Fi password**.
- **Home-LAN access:** when connected through the board's home Wi-Fi (STA) interface, the web dashboard and APIs still require HTTP Basic authentication with username **admin**. The initial admin password matches the unique setup hotspot password. Change it from **Setup** after connecting. The AP exception is not based on the request's URL or Host header.
- **Uploads:** GIF uploads, firmware OTA, and all other dashboard APIs use the same AP-or-admin access rule. Other devices connected to the GlowDeck setup hotspot can change settings or upload firmware, so only share the hotspot password with people you trust.
- The dashboard uses **HTTP, not HTTPS**. Do not port-forward port 80 to the internet. Basic authentication over HTTP is not confidential against devices able to intercept local traffic.
- Wi-Fi credentials and admin password are stored in ESP32 NVS (not an encrypted vault). They are never sent back by \`GET /api/config\`.
- WLED device addresses are restricted to IPv4 literals to reduce accidental or malicious requests to unrelated hosts.
- The public NHL HTTPS connection currently uses \`setInsecure()\` because certificate bundle management is not yet integrated. It does **not transmit credentials**, but an on-path attacker could impersonate the scoreboard and trigger light events. CA verification is planned for the next security pass.
- Firmware updates require dashboard access (setup AP or admin authentication over home Wi-Fi) and are sent to the device on the local network. Never upload untrusted \`.bin\` files.
- Changing the admin password ends your old browser authentication session on the home Wi-Fi dashboard; sign in again with the new password there. The setup hotspot remains accessible without a second login.
