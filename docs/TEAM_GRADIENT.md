# GlowDeck GameDay v0.3.2 — NHL team-color gradient backdrop

The 320×240 ESP32-2432S028R game carousel now paints a matchup-specific **away team primary color on the left, home team primary color on the right**, with a smooth darkened blend in the center. The appearance is based on the supplied Dallas Stars vs Washington Capitals reference: two recognizable team-color zones, soft transition, textured broadcast treatment, and white high-contrast scoreboard text.

## Implementation

- Every **pregame**, **live**, and **final** game slide calls `drawTeamGradientBackground(const Game&)` instead of the old hard-coded blue arena background.
- Team colors come from the existing 32-club `teamRgb()` palette used by WLED goal celebrations. No new logo or team-color assets are required.
- `include/TeamGradient.h` performs integer-only fixed-point easing, mixing and RGB888→RGB565 conversion with a subtly dark center. Color brightness ranges from approximately 73% at the outer edges to 29% in the center, preserving white score/VS/time legibility.
- Rendered directly onto the ILI9341 as **80 four-pixel-wide vertical strips** plus sparse subtle mesh dots. **No framebuffer, PNG, sprite or dynamic memory allocation** is introduced. The TFT renderer's 320×240 layout and existing memory safety measures are preserved.
- Solid neutral-charcoal score, status, SOG and stat plates remain opaque so only their interiors require repainting during the frequent NHL live-score polls. Team backgrounds and logos only redraw when the carousel slide changes.
- The black header remains readable, with its live/next/final badge and top-right menu.
- **Standings, game selection, stats-detail, setup, Wi-Fi and update screens** retain their existing neutral backgrounds. The team gradient is applied exclusively to full-screen game carousel slides.
- Existing NHL score polling, armed/followed game selection, WLED celebrations, rear CYD RGB, sound, OTA and Preferences are unchanged.

## Visual acceptance checklist

1. Open a pregame **Dallas @ Washington** matchup. The left half should show Dallas green, the right Washington red, blended into a darker center, not electric blue.
2. Check a different matchup such as **Boston @ Rangers**. The gradient should update from Bruins gold to Rangers blue when that slide appears.
3. When the game is live, scores and period/clock should remain readable against charcoal score plates, and the gradient should **not redraw on every 10-second score update**.
4. Swipe to all eight conference/division standings slides: they should remain neutral/dark.
5. Validate logos after latest dark-mode artwork fix. Confirm rear LED and the selected WLED controller goal alerts still operate, including while the screen shows a different matchup.
6. Check dashboard Free Memory / Stack Headroom (minimum) to ensure rendering has not affected memory stability.

## Testing

`tests/test_team_gradient.cpp` verifies 320×240 bounds, full left/right colors, center darkening, smoothstep midpoint, RGB565 primary colors and out-of-range x clamping. CI builds firmware and publishes the OTA app binary. Physical CYD validation remains necessary to judge the effect of panel inversion and overall contrast.
