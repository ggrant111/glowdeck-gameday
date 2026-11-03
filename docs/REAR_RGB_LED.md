# CYD rear RGB LED events (v0.2.5)

The original ESP32-2432S028R includes a rear RGB LED: GPIO4 red, GPIO16 green, GPIO17 blue. **Active LOW**: PWM duty 255 = OFF, duty 0 = maximum brightness. On stock boards these are separate from the TFT/backlight, XPT2046 touch and speaker pins. PSRAM-modified boards may reuse GPIO16/17.

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
