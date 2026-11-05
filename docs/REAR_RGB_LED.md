# CYD rear RGB LED events (v0.2.5)

The original ESP32-2432S028R includes a rear RGB LED: GPIO4 red, GPIO17 green, GPIO16 blue. **Active LOW**: PWM duty 255 = OFF, duty 0 = maximum brightness. On stock boards these are separate from the TFT/backlight, XPT2046 touch and speaker pins. PSRAM-modified boards may reuse GPIO16/17.

## Behaviors

- **Goals:** Flash the scoring team's team-color RGB LED for 2–12 seconds; goal flashes take priority.
- **Power plays:** Gentle team-color pulse while a followed live game's NHL situation data identifies an attacking team, then off when the advantage ends or data goes stale.
- **Idle:** Off. Animations use nonblocking ESP32 PWM and run independently of WLED and the screen.
- **Following games:** The currently armed WLED game always drives the rear LED, plus up to four additional games selected under the web dashboard **CYD LED** tab. Extra followed game IDs expire when the NHL day changes.
- **Power-play detection:** The firmware reads the 'situation' field of the date-scoped NHL score feed, with a filtered fallback to `/v1/scoreboard/now` for live followed games. It does not mistake cumulative PP opportunities for a current man advantage. It stays off on missing/stale situations.

## Web controls

The CYD LED tab provides enable/disable, goal flash and PP pulse options, maximum brightness, goal duration, daily follow/unfollow checkboxes, and preview buttons for home/away goals, 9-second PP pulse and STOP. Settings are saved in NVS alongside Wi-Fi and WLED preferences.

## Physical validation

1. Flash the successful v0.2.5 build via the existing OTA page.
2. Open **CYD LED**, test a home and away flash, then a nine-second pulse.
3. Check that brightness and the enabled setting persist over restart.
4. Follow another daily game. Confirm a new goal produces a flash even if the front display is showing another game.
5. Confirm a reported power play produces a pulse that ends on penalty expiration/intermission.
6. Check original WLED state restore, horn and web UI still work.

Power-play events depend on the timeliness/completeness of the NHL's unofficial live JSON. These aren't guaranteed to coincide exactly with what appears in a TV broadcast.

CI tests `tests/test_rear_led.cpp` for PWM active-low math, blinking and pulsing timing; final behavior still requires a physical CYD test.

## Team-color verification and channel diagnostics (v0.3.8+)

The CYD web dashboard **CYD LED** page now labels the selected/featured NHL
home and away teams with their actual primary RGB hex values (from the same
firmware table used for observed game events). If no game is available, the
home/away tests are disabled instead of **silently testing Utah twice**.

- **Choose any NHL team's primary color:** Pick any of 32 NHL teams and run
  a goal flash or nine-second PP preview, even with no NHL game on the device.
  This uses the same source-of-truth palette as live goal and WLED events.
- **Red / Green / Blue diagnostics:** Hold one physical channel steady for
  three seconds each. If the wrong color lights, check the active-LOW channel
  mapping (GPIO4 red, GPIO17 green, GPIO16 blue), board revision and PWM
  brightness settings. These tests require no WLED controller.
- **Full celebration preview:** Home, away or selected-team goal previews
  use one RGB color for the rear LED, opted-in WLED devices and the on-screen
  overlay. Rear and screen outputs work with **zero WLED devices**.
  Full Stop clears the preview, requests WLED restoration and stops the
  rear LED; any real NHL power-play state may resume on the next tick.

API: authenticated `GET /api/rear/palette` returns all team abbreviations
and colors. `POST /api/rear/test` accepts `{"mode":"goal","side":"home"}`
or `{"mode":"goal","team":"DAL"}` (also `powerplay`), and hardware
`{"mode":"channel","channel":"red"}` (also green/blue). It returns
`team` and `hex` for each test. The existing
`POST /api/wled/preview` adds optional `{"combined":true,"team":"ANA"}`
for full preview; existing WLED-only requests keep their behavior.

These are source/CI checks; only observing the physical device can confirm
that RGB channels are soldered and wired correctly. The dashboard records
an attempted rear LED activation, not optical measurement.

## Corrected green/blue GPIO routing (v0.3.9)

**Physical device testing found that the v0.3.8 green and blue diagnostic
buttons lit opposite colors on this CYD.** v0.3.9 fixes the onboard
rear LED channel-to-pin mapping only:

| Rear LED channel | CYD GPIO |
| --- | --- |
| Red | 4 |
| Green | 17 |
| Blue | 16 |

NHL primary colors, WLED JSON effects, screen overlays and the
active-LOW PWM math are unchanged. A shared, unit-tested pin map prevents
regression. No rewiring is required.

**After installing v0.3.9 via OTA**, open CYD LED > Hardware RGB channel
diagnostics and verify that Red only, Green only and Blue only show those
colors independently. Then select Dallas (green, #006847) and Anaheim
(orange, #F47A38) for goal previews. CYD board revisions may differ.
