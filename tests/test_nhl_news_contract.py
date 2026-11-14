"""Phase 9 source-contract guard: idle-only Forge fetch + news UI parity."""
from pathlib import Path

main=Path("src/main.cpp").read_text()
service=Path("include/NhlNewsService.h").read_text()
header=Path("include/NhlNews.h").read_text()
web=Path("include/WebUi.h").read_text()
for name in ("newsAnyLive()", "newsViewEnabled()", "drawNewsCard()",
             "drawNewsTicker()", "tickForgeNews(uint32_t now)",
             'server.on("/api/news",HTTP_GET', 'server.on("/api/news/refresh",HTTP_POST'):
    assert name in main,name
assert 'wledRestoreWouldConflict(now,8500UL)' in main
assert 'overlayController.active() || rearEvent!=RearEvent::IDLE' in main
assert 'if (newsViewEnabled()) { drawNewsCard(); return; }' in main
assert 'if(screen==Screen::LIVE && newsViewEnabled())' in main
assert 'cfg.cycleGames && newsViewEnabled()' in main
assert 's3NewsTickerFrame' in main and 'presentRegion(0,286,480,24)' in main
assert 'NhlNewsService::fetch(newsCache,follows,teamId)' in main
assert 'DeserializationOption::Filter(filter)' in service
assert '"Accept-Encoding","identity"' in service
assert '"items"' in service and '"headline"' in service and '"contentDate"' in service
assert 'tags.slug=teamid-' in service
assert 'constexpr uint8_t kMaxStories = 10;' in header
assert 'newsCache.merge' not in main  # Service owns schema details
for name in ('id="news"','data-tab="news"','id="newsCards"','id="newsFilter"',
             'async function loadNews()', '/api/news?filter=', 'id="newsTicker"'):
    assert name in web,name
route=main.split('server.on("/api/news",HTTP_GET',1)[1].split('server.on(',1)[0]
assert 'if(!authorized())return;' in route
refresh=main.split('server.on("/api/news/refresh",HTTP_POST',1)[1].split('server.on(',1)[0]
assert 'if(!authorized())return;' in refresh and 'newsRefreshRequested=true;' in refresh
# No synchronous HTTP performed by a route, renderer, or live animation.
assert 'NhlNewsService::fetch(' not in main.split('void draw() {',1)[1]
assert 'NhlNewsService::fetch(' not in route and 'NhlNewsService::fetch(' not in refresh
print("PASS phase 9: Forge integration, idle guards, news cards, ticker, web endpoints")
