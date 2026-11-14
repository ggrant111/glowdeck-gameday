# Phase 9 — NHL Forge news cards and ticker

Status: **Implemented, with idle news fallback recovery; CYD and S3 hardware validation required.**

## Features
- League-wide article feed and team-tag feed from Forge DAPI; when unavailable, low-priority ESPN NHL news provides an editorial fallback.
- Followed-team relevance: persistent favorite teams and both teams in today's starred/armed games. Following a game **never** arms physical lights, sound, WLED, or Hue.
- Editorial pregame and postgame categories. These classify titles with conservative keywords; the story API is **not** an authoritative source of injuries, lineups, game outcomes, or events.
- Read-only `GET /api/news?filter=all|followed|pregame|postgame` with headline, summary, team tag, published date, thumbnail, and article slug. Requires existing dashboard authorization.
- `POST /api/news/refresh` queues a safe update (HTTP 202) and will only fetch when NHL games and celebration hardware are idle.
- A self-hosted **NHL News** dashboard page offers story cards, an accessible headline marquee and article links, no additional proxy/server.
- CYD 320x240 and Waveshare S3 480x320 render a headline/summary card and scroll the next headline in a separate bottom strip; supported JPEG thumbnails are displayed beside each story when safely cached. Swipe or tap to advance. Use the GAMES corner to open today's schedule.
- News cards rotate every 10 seconds when no NHL game state is LIVE/CRIT. During LIVE or intermission, existing scoreboard and NHL events take immediate precedence. Manual pinned games are respected.
- **Important**: the idle display is selected even with zero cached articles. While fetching, a visible "UPDATING HEADLINES" card replaces the old games/standings carousel and explains why no stories are currently available. A blank or failed Forge feed cannot force a silent return to the old idle carousel.

## Source and API
```text
GET https://forge-dapi.d3.nhle.com/v2/content/en-us/stories
GET https://forge-dapi.d3.nhle.com/v2/content/en-us/stories?tags.slug=teamid-59
GET https://site.api.espn.com/apis/site/v2/sports/hockey/nhl/news?limit=10  # fallback only
```
Forge's Deltatre Distribution API documents `items[]`, `headline`, `title`, `summary`, `slug`, `contentDate`, `tags[].slug`, `thumbnail.thumbnailUrl`, plus tag filtering. NHL-specific success and response schema must be verified on hardware. The backend is not an official, stable NHL developer contract, and may return HTTP errors.

## Performance and caching
- Fixed-size **10-article RAM metadata cache** shared by the device and dashboard; separate fixed-slot, size-limited JPEG thumbnail files use the existing SPIFFS/LittleFS data partition. Reboot clears news; network failures preserve already cached stories until reboot.
- Two small alternating Forge fetches: general feed, then a team-specific tag feed when favorites exist. The team rotates among followed teams across fetch cycles. Followed-team news is prioritized on cards and filtered dashboard views.
- Update period: every **8 minutes** when safe, with the first followed-team fetch primed **30 seconds** after a successful Forge league fetch; **2-minute** retry after both sources fail.
- Forge failure or zero usable articles queues ESPN on the **next loop** (never two blocking TLS calls in one loop). A successful fallback populates the same bounded cache and keeps original story source/links. ESPN is editorial-only and cannot trigger game events. No requests until after boot, the first scoreboard load, Wi-Fi connection, and adequate free heap.
- Uses streamed, filtered ArduinoJson to discard full story bodies, relations and excess metadata, with bounded TLS timeouts and minimum free heap.
- Skips news HTTP calls during any game with state LIVE/CRIT, including intermission; during active event overlays or rear LED goals; or when a WLED restoration may be due. Goal/PP/SO polling and physical effects always take priority.
- News is editorial: it must never call a celebration, horn, WLED/Hue state change, or game arming function.
- On-device cards now show cached JPEG photos from trusted NHL/ESPN CDN URLs when available. Downloads only run during idle periods, are capped at 20 KB (CYD) or 110 KB (S3). The CYD uses four fixed slots for the four highest-priority stories because its min_spiffs OTA partition provides only 128 KiB of filesystem storage; the S3 has ten slots. This prevents repeated downloads/flash writes on every news cycle. Unsupported PNG/WebP, incomplete responses and memory/storage errors fall back to readable text-only cards without blocking news.
- The bottom news ticker has its own continuous UTF-8-safe pixel scroll (30 ms steps), independent of card selection and image redraw. Switching cards never resets the ticker or switches it to a new story mid-scroll. News feed updates preserve offset when possible.
- Photos are downloaded **outside** the display renderer, using the existing CYD SPIFFS partition or S3 LittleFS mount (shared with goal GIF and WAV assets). They are decoded by the lightweight JPEG library directly into the TFT or existing S3 sprite; no second frame buffer is allocated.
- The browser dashboard also displays image URLs from Forge and now includes article photos from ESPN fallback when returned by the service.

## Manual verification
1. Flash a board-matched OTA image (not a factory erase) on a CYD and S3.
2. Open the web dashboard's **NHL News** page and verify HTTP status, last-refresh age, and real articles.
3. With no LIVE/CRIT games, confirm the card advances every 10 seconds and the bottom ticker stays at the exact same character/pixel position across card changes, manual swipes, and asynchronous thumbnail paints. No full-screen flicker from ticker-only updates.
4. Verify NHL/ESPN JPEG images appear next to story text on CYD and S3 without overlapping the header/footer. PNG/WebP or oversized/unsupported images should remain text-only. Confirm the S3 arena/overlay GIF still plays and uploaded sounds are intact; CYD touch/Wi-Fi setup and NVS settings remain unaffected.
5. Add UTA and BOS favorite teams; confirm followed stories are ranked ahead of generic league articles when present and their colors/short labels display correctly. Star a game and verify both clubs enter the followed-team filter without arming.
6. Simulate or observe a FUT→LIVE transition: a score screen immediately takes the news card's place; overlay, audio, lights, PP tracker, and WLED restoration continue working.
7. During an intermission, confirm news does **not** replace the live scoreboard.
8. Disable Wi-Fi after caching; confirm cached headline behavior is retained until reboot and no crash occurs. Restart and verify empty state until the first successful Forge response.
9. Visit pregame/postgame filters; no unsupported injury/lineup/statistical facts should be inferred from article headlines.
10. Test a 503, empty `items`, invalid JSON, slow TLS timeout and memory pressure. Failed refreshes must retain the previous cache and leave game updates responsive.
11. Inspect S3 SPI partial-transfer logging and CYD redraw/touch operation for news ticker frames.

## Automated tests
`tests/test_nhl_news.cpp` covers team tags, headline classification, deduplication, favorites-first chronological sort and cache capacity.
`tests/test_nhl_news_contract.py` protects source integration: no live fetch, event precedence, authenticated endpoints, S3 partial redraw and UI hooks.
The main GitHub Actions workflow also runs `tests/test_nhl_news_photos_contract.py`, covering ticker continuity and memory-bounded JPEG behavior, before compiling both boards.

## Limitations / next enhancements
- Full article text and progressive PNG/WebP rendering are not supported on the TFT. The device displays source-provided JPEG thumbnails when valid.
- Article tags may be incomplete, and editorial classification is best-effort.
- The Forge service may block direct connections or change its path. ESPN's public NHL news JSON is an alternate source, not a guaranteed service contract. If both sources fail, GameDay displays a visible loading/error card rather than stale game/standings slides. The local /api/news response includes source, lastError, lastAttemptMs, freeHeap, paused, and fallbackPending.
- To provide longer multi-paragraph pregame briefings, a future release can add on-demand article detail retrieval with stricter memory/performance limits.
