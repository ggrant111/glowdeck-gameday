# GlowDeck GameDay v0.3.1 — WLED Celebration Engine

An event-driven WLED controller runs directly on the ESP32-2432S028R CYD. It uses **local HTTP JSON API calls** to WLED controllers, without a PC, extra server, cloud account, or additional wiring.

## Per-device participation

On **WLED / Lights** in the device's web dashboard, enter up to four local WLED IPs, and enable **Include this WLED device in celebrations** only for the devices that should react. For each enabled device select event classes independently:

- Goals
- Power plays
- Game start
- Final result

Devices without a configured IP or with their master checkbox unchecked are never targeted. A per-device event mask controls its participation in each event; prior enabled controllers default to all four event classes after upgrading. Configuration is saved to Preferences/NVS.

For every event, all participating controllers receive the **same team RGB color, WLED effect ID, brightness, speed, intensity and shared elapsed-event timeline**. Commands are sent directly to their local IPs; WLED's per-call \`"udpn":{"nn":true}\` prevents those commands from automatically broadcasting to OTHER controllers that the user did not select.

**Synchronization caveat:** HTTP requests are sent sequentially, and WLED devices render their built-in effects using their own internal clocks. They should produce equivalent celebration patterns, but exact frame-synchronized animation is **not** promised. True real-time UDP/DDP pixel-stream sync may be implemented in a later firmware release. Do not rely on WLED UDP Sync groups to enforce user device selection.

## Event timeline and precedence

Event | Effect | Duration
--- | --- | ---
Goal | 1/3 fast Strobe (WLED FX 23), then 2/3 Chase (FX 28) | Configurable 2–30 sec, default 8
Power play | Breathe team color (FX 2) | Until NHL reports PP over, stale, or intermission
Game start | Team-color Wipe (FX 3) | 4.5 sec
Final win | Twinkle team color (FX 17) | 7 sec
Final loss | Slow Fade team color (FX 12) | 7 sec

Goal style can also be set to Strobe only, Chase only or Solid. **Goals have priority** over other animations. If a goal occurs during a power play, the strobe/chase plays first, then power-play breathing resumes *if NHL still reports the advantage*. A game ending at the same time as a goal queues its final result behind the goal celebration.

Game-start and final events use the **armed** NHL matchup, not whichever carousel or standings slide is displayed. A global **favorite side** selector (home/away) chooses the color and determines win vs loss for final events. The original goals preference (home, away or both) remains independent. None of these events replay when the CYD first boots or when a game is armed mid-game.

Power-play detection uses the NHL \`situation\` skater counts/PP descriptions, with the previously implemented filtered live-scoreboard fallback. It never infers a live power play from cumulative PP statistics. API publication may lag TV.

## Snapshots, segments and safe restoration

Before starting an event, the CYD GETs \`http://IP/json/state\` from every eligible controller and stores a bounded snapshot in RAM. **No snapshot -> no effect** on that controller. Once snapshots are collected, it POSTs an identical event sequence to each eligible device at \`/json/state\`.

WLED's configured active segments are discovered from the saved JSON state. Up to 16 active segments per controller are temporarily assigned the event effect/color. The controller's original segments, state, brightness and effects are preserved in the snapshot and restored after the event, with up to three attempts. The firmware stores each snapshot's **original IP**, so subsequent edits to device IPs cannot restore the wrong controller.

Original WLED states are not written to flash because goal events may occur frequently. A sudden CYD power loss during an active celebration can therefore leave the WLEDs in that effect; they may require manual restoration. This is a known limitation. If a device is unreachable, only its own celebration is skipped; other opted-in devices still run.

The WLED snapshot save/restore and JSON payload construction use heap-backed documents rather than large stack-local buffers, minimizing recurrence of the earlier CYD \`loopTask\` stack crash. No 320x240 framebuffers or pixel-stream buffers are allocated.

## Web UI preview

Use the **WLED** web tab to save device/event flags and test:

- Home or away goal
- Power-play pulse (9-second preview)
- Game-start sweep
- Final win and final loss
- Stop and restore (suppresses a live PP retrigger for 30 seconds)

The UI reports each WLED's latest action or error (Playing, Restored, Snapshot failed, Command failed, Retrying restore, Restore failed), and shows the active engine event.

## Hardware acceptance tests

1. Connect two or more WLED controllers on the same Wi-Fi, configure their IPs, and check **Include in celebrations** on both.
2. Leave a third connected WLED *unchecked*. Run **Home goal** and verify **only the checked devices** strobe then chase in the same color; unselected lights must not change even with WLED UDP Sync settings.
3. For an enabled controller, disable the PP event checkbox and save. Run **Power play**: other opted-in PP devices breathe, but that controller doesn't change.
4. Run Start, Win, Loss preview; verify the effects are visually appropriate and restore previous lighting.
5. Run PP preview, then Goal preview while PP is active; goal should take priority. A live NHL power play may resume after goals based on actual situation data.
6. Run Stop and restore. Confirm previously active scenes, segments and brightness return.
7. Verify the rear CYD RGB LED remains independent, and league standings carousel/OTA/10-second armed score polling still work.
8. Use the dashboard Free memory/Stack headroom stats and 115200 serial output to watch stability with two or more devices. A successful build is not a physical network timing guarantee.

## WLED references

- [Official WLED JSON API](https://kno.wled.ge/interfaces/json-api/)
- [Official WLED effects list](https://github.com/wled/WLED/wiki/List-of-effects-and-palettes)

Those documents define effects and \`udpn.nn\`. WLED builds can exclude optional effects: in that case WLED may fall back to Solid for that controller. Effect 0/2/3/12/17/23/28 are long-established standard built-in modes.
