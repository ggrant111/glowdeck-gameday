# Phase 9 — NHL Forge news cards and ticker

Status: **Implemented on the Phase 9 feature branch; hardware validation required.**

## Features
- League-wide article feed and team-tag feed from Forge DAPI.
- Followed-team relevance: persistent favorite teams and both teams in today's starred/armed games. Following a game **never** arms physical lights, sound, WLED, or Hue.
- Editorial pregame and postgame categories. These classify titles with conservative keywords; the story API is **not** an authoritative source of injuries, lineups, game outcomes, or events.
- Read-only `GET /api/news?filter=all|followed|pregame|postgame` with headline, summary, team tag, published date, thumbnail, and article slug. Requires existing dashboard authorization.
- `POST /api/news/refresh` queues a safe update (HTTP 202) and will only fetch when NHL games and celebration hardware are idle.
- A self-hosted **NHL News** dashboard page offers story cards, an accessible headline marquee and article links, no additional proxy/server.
- CYD 320x240 and Waveshare S3 480x320 render a headline/summary card and scroll the next headline in a separate bottom strip. Swipe or tap to advance. Use the GAMES corner to open today's schedule.
- News cards rotate every 10 seconds when no NHL game state is LIVE/CRIT. During LIVE or intermission, existing scoreboard and NHL events take immediate precedence. Manual pinned games are respected.

## Source and API
```text
GET https://forge-dapi.d3.nhle.com/v2/content/en-us/stories
GET https://forge-dapi.d3.nhle.com/v2/content/en-us/stories?tags.slug=teamid-59
```
Forge's Deltatre Distribution API documents `items[]`, `headline`, `title`, `summary`, `slug`, `contentDate`, `tags[].slug`, `thumbnail.thumbnailUrl`, plus tag filtering. NHL-specific success and response schema must be verified on hardware. The backend is not an official, stable NHL developer contract, and may return HTTP errors.

## Performance and caching
- Fixed-size **10-article RAM cache** shared by the device and dashboard; no daily write wear or additional flash partition. Reboot clears news; network failures preserve already cached stories until reboot.
- Two small alternating fetches: general feed, then a team-specific tag feed when favorites exist. The team rotates among followed teams across fetch cycles. Followed-team news is prioritized on cards and filtered dashboard views.
- Update period: every **8 minutes** when safe, with the first followed-team fetch primed **30 seconds** after a successful league fetch; **2-minute** retry after failure. No requests until after boot, the first scoreboard load, Wi-Fi connection, and adequate free heap.
- Uses streamed, filtered ArduinoJson to discard full story bodies, relations and excess metadata, with bounded TLS timeouts and minimum free heap.
- Skips news HTTP calls during any game with state LIVE/CRIT, including intermission; during active event overlays or rear LED goals; or when a WLED restoration may be due. Goal/PP/SO polling and physical effects always take priority.
- News is editorial: it must never call a celebration, horn, WLED/Hue state change, or game arming function.
- Images are used in the browser from HTTPS URLs supplied by Forge. Touch displays use text-only news cards to avoid image downloads/decoding on ESP32.

## Manual verification
1. Flash a board-matched OTA image (not a factory erase) on a CYD and S3.
2. Open the web dashboard's **NHL News** page and verify HTTP status, last-refresh age, and real articles.
3. With no LIVE/CRIT games, confirm the card appears, advances every 10 seconds, swipe/tap advances, menu opens GAMES, and lower headline strip scrolls without full-screen flicker.
4. Add UTA and BOS favorite teams; confirm followed stories are ranked ahead of generic league articles when present and their colors/short labels display correctly. Star a game and verify both clubs enter the followed-team filter without arming.
5. Simulate or observe a FUT→LIVE transition: a score screen immediately takes the news card's place; overlay, audio, lights, PP tracker, and WLED restoration continue working.
6. During an intermission, confirm news does **not** replace the live scoreboard.
7. Disable Wi-Fi after caching; confirm cached headline behavior is retained until reboot and no crash occurs. Restart and verify empty state until the first successful Forge response.
8. Visit pregame/postgame filters; no unsupported injury/lineup/statistical facts should be inferred from article headlines.
9. Test a 503, empty `items`, invalid JSON, slow TLS timeout and memory pressure. Failed refreshes must retain the previous cache and leave game updates responsive.
10. Inspect S3 SPI partial-transfer logging and CYD redraw/touch operation for news ticker frames.

## Automated tests
`tests/test_nhl_news.cpp` covers team tags, headline classification, deduplication, favorites-first chronological sort and cache capacity.
`tests/test_nhl_news_contract.py` protects source integration: no live fetch, event precedence, authenticated endpoints, S3 partial redraw and UI hooks.
The main GitHub Actions workflow runs both tests before building board firmware.

## Limitations / next enhancements
- This release does not fetch full article bodies or show photographs on the TFT.
- Article tags may be incomplete, and editorial classification is best-effort.
- The Forge service may block direct connections or change its path; the local dashboard reports HTTP errors and keeps old headlines. No fabricated news or ESPN fallback is shown.
- To provide longer multi-paragraph pregame briefings, a future release can add on-demand article detail retrieval with stricter memory/performance limits.
