#pragma once
#include <Arduino.h>

// Self-contained interface: no CDN, filesystem upload, JavaScript framework or cloud server.
static const char WEB_UI[] PROGMEM = R"GLOWDECK(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#0a141c">
<title>GlowDeck GameDay | CYD</title>
<style>
:root{color-scheme:dark;--bg:#071117;--panel:#11232b;--panel2:#172e36;--edge:#2a4751;--primary:#31837a;--active:#66d6c0;--text:#f2f7f8;--muted:#aec2c8;--amber:#ffcc72;--danger:#f39c91;--shadow:0 15px 45px #0003}
*{box-sizing:border-box}
html{scroll-behavior:smooth}
body{margin:0;color:var(--text);background:radial-gradient(ellipse 1150px 600px at 80% -200px,#164840 0%,transparent 66%),var(--bg);font:15px/1.55 system-ui,-apple-system,"Segoe UI",sans-serif;-webkit-font-smoothing:antialiased}
header{padding:28px clamp(17px,4vw,48px);border-bottom:1px solid var(--edge);background:linear-gradient(105deg,#0b2930e8,#0a141bc7)}
.eyebrow{color:#88e2cf;text-transform:uppercase;font-weight:750;letter-spacing:.16em;font-size:11px}
.top{display:flex;align-items:center;justify-content:space-between;gap:16px;max-width:1200px;margin:auto}
h1{font-weight:800;font-size:clamp(24px,4vw,36px);line-height:1.2;margin:3px 0}
h2{font-size:19px;line-height:1.3;letter-spacing:-.02em;margin:0 0 11px}
h3{font-size:15px;margin:0 0 8px}
p{color:var(--muted);margin:6px 0 12px}
main{max-width:1260px;margin:0 auto;padding:20px clamp(12px,3vw,28px) 96px;display:grid;grid-template-columns:220px minmax(0,1fr);gap:23px;align-items:start}
.tabs{position:sticky;top:16px;display:flex;flex-direction:column;gap:7px;background:#101f27;border:1px solid var(--edge);border-radius:18px;padding:10px;box-shadow:var(--shadow)}
.tab{display:flex;align-items:center;text-align:left;font-size:14px;width:100%;white-space:nowrap;padding:12px 15px;min-height:47px;background:transparent;border:1px solid transparent;border-radius:11px;color:var(--muted)}
.tab.selected{background:#285f5b;border-color:#3c9388;color:white;box-shadow:inset 3px 0 var(--active)}
.panel{display:none;min-width:0}.panel.active{display:block}
.section-intro{margin:0 0 22px;padding:7px 2px}.section-intro h2{font-size:clamp(25px,3vw,31px);margin:6px 0}.section-intro p{max-width:670px}
.card{background:linear-gradient(160deg,#142a32,#102029);border:1px solid var(--edge);border-radius:18px;padding:clamp(16px,2vw,23px);margin:0 0 16px;box-shadow:0 7px 28px #0002;min-width:0}
.card.hero{border-color:#386d66;background:linear-gradient(125deg,#1a4b47 0%,#15343a 52%,#11242d 100%)}
.card.hero h2{font-size:clamp(19px,2vw,25px)}
.card .card-lead{margin-bottom:18px}
.stats{display:grid;grid-template-columns:repeat(auto-fit,minmax(146px,1fr));gap:10px}
.stat{background:#0d1b24aa;border:1px solid #28444f;border-radius:12px;padding:13px;min-width:0}
.stat b{display:block;font-size:17px;line-height:1.5;overflow-wrap:anywhere}
.stat span{display:block;color:var(--muted);font-size:12px;margin-bottom:3px}
button{font:inherit;color:var(--text);background:#223b44;border:1px solid #40616b;border-radius:11px;padding:10px 14px;cursor:pointer;min-height:44px;font-weight:670;transition:background .15s,border-color .15s,transform .15s}
button:hover{background:#2b4c55;border-color:#76beb0}
button:active{transform:scale(.985)}
button.primary,button.selected{background:var(--primary);border-color:#5ab2a4;color:#fff}
button:disabled{opacity:.45;cursor:not-allowed}
button.ghost{background:transparent}
button:focus-visible,input:focus-visible,select:focus-visible{outline:3px solid var(--active);outline-offset:2px}
.actions{display:flex;flex-wrap:wrap;align-items:center;gap:9px;margin:15px 0}
.actions>*{max-width:100%}
label{display:block;font-size:13px;color:#d3e1e5;margin:13px 0 6px;font-weight:650}
input,select{color:var(--text);background:#091922;border:1px solid #40606b;border-radius:11px;padding:11px 13px;font:inherit;width:100%;min-height:45px}
input[type=file]{max-width:100%}
input:focus,select:focus{outline:2px solid var(--active);outline-offset:1px}
.row{display:grid;grid-template-columns:repeat(auto-fit,minmax(198px,1fr));gap:13px}
.toggle{display:flex;align-items:center;gap:10px;min-height:33px}
.toggle input{width:21px;height:21px;min-height:auto;accent-color:#45a89b;flex-shrink:0}
.toggle label{margin:0}
.game{display:flex;justify-content:space-between;align-items:center;gap:12px;padding:14px 0;border-bottom:1px solid var(--edge)}
.game:last-child{border-bottom:0}
.game .score{font-size:18px;font-weight:800;letter-spacing:-.03em}
.game-row{background:#0e2028;border:1px solid var(--edge);border-radius:14px;margin:10px 0;padding:14px;gap:14px}
.game-row:last-child{border-bottom:1px solid var(--edge)}
.game-row .actions{margin:0}
.game-row .game-details{min-width:0;flex:1}
.game-status{font-size:11px;font-weight:800;letter-spacing:.06em;color:#8de4d2;text-transform:uppercase}
.badge{background:#214942;border:1px solid #376e62;border-radius:100px;padding:4px 9px;font-size:11px;font-weight:700;color:#cefff2}
.item{border:1px solid var(--edge);border-radius:13px;padding:14px;margin:10px 0;background:#0c1b24}
.status{display:inline-flex;align-items:center;gap:7px;border-radius:100px;padding:7px 13px;font-size:12px;font-weight:730;background:#26363d;color:#e6f0f2;white-space:nowrap}
.status:before{content:"";width:7px;height:7px;border-radius:50%;background:#a4b4b8}
.status.good{background:#194c42;color:#caffeb}.status.good:before{background:#73eac3}
.status.warn{background:#523821;color:#ffe0af}.status.warn:before{background:#ffba63}
.muted{color:var(--muted)}.small{font-size:12px}
.filterbar{display:flex;flex-wrap:wrap;gap:8px;margin:15px 0}
.filterbar button{min-height:39px;padding:7px 13px;font-size:13px}
.team-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(101px,1fr));gap:8px;margin:16px 0}
.team-choice{display:flex;align-items:center;justify-content:center;gap:8px;border-color:#3b5864;background:#10222b;min-height:46px}
.team-choice[aria-pressed="true"]{background:#2d6960;border-color:#8de0d0;color:#fff}
.team-dot{height:12px;width:12px;display:inline-block;border-radius:50%;flex:none;border:1px solid #ffffff55}
.star-button{min-width:44px;font-size:22px;padding:6px 10px;color:#bac5c8}
.star-button[aria-pressed="true"]{color:#ffd479;background:#493c29;border-color:#79633c}
#notice{position:fixed;bottom:max(16px,env(safe-area-inset-bottom));right:16px;left:16px;max-width:520px;margin:auto;z-index:11;background:#1b4245;color:white;border:1px solid #65b2a5;border-radius:13px;padding:14px 17px;box-shadow:0 9px 32px #0009;display:none}
code{color:#a0ecdd;overflow-wrap:anywhere}
.hint{border-left:3px solid var(--primary);padding:5px 0 5px 13px;margin:13px 0;color:var(--muted)}
.twocol{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:15px}
.twocol .card{margin-bottom:0}
.rail-note{font-size:12px;color:var(--muted);padding:8px 12px}
.kicker{color:#88ddcb;font-size:12px;font-weight:800;text-transform:uppercase;letter-spacing:.1em}
[hidden]{display:none!important}
@media(max-width:850px){
main{display:block;padding:0 14px 94px}
.tabs{top:0;z-index:5;display:flex;flex-direction:row;overflow-x:auto;gap:7px;border:none;border-bottom:1px solid var(--edge);border-radius:0;padding:10px 0 12px;background:#071117f7;box-shadow:none;scrollbar-width:thin;scroll-snap-type:x proximity}
.tab{width:auto;min-width:max-content;padding:10px 14px;scroll-snap-align:start}
.tab.selected{box-shadow:inset 0 -3px var(--active)}
.rail-note{display:none}
.section-intro{padding:22px 3px 2px}
}
@media(max-width:650px){
header{padding:19px 17px}
.card{padding:16px}
.twocol{display:block}.twocol .card{margin-bottom:15px}
.stats{grid-template-columns:repeat(2,minmax(0,1fr))}
.game-row{flex-wrap:wrap}
.game-row .actions{width:100%;justify-content:flex-start}
.row{grid-template-columns:1fr}
}
@media(prefers-reduced-motion:reduce){html{scroll-behavior:auto}button{transition:none}}
</style>
</head>
<body>
<header>
<div class="top">
<div><div class="eyebrow">GLOWDECK · GAME CENTER</div><h1>GameDay</h1><p>Every game. Your favorite moments. Your lights.</p></div>
<div id="connection" class="status">Connecting</div>
</div>
</header>
<main>
<nav class="tabs" aria-label="GameDay navigation">
<div class="rail-note"><span class="kicker">CONTROL CENTER</span><p class="small">This device · your local network</p></div>
<button class="tab selected" data-tab="dashboard" aria-current="page">Overview</button>
<button class="tab" data-tab="games">Live games</button>
<button class="tab" data-tab="display">Favorites &amp; alerts</button>
<button class="tab" data-tab="devices">WLED celebrations</button>
<button class="tab" data-tab="rear">Rear RGB LED</button>
<button class="tab" data-tab="events">Activity</button>
<button class="tab" data-tab="settings">Device settings</button>
<button class="tab" data-tab="firmware">Firmware update</button>
</nav>
<section id="dashboard" class="panel active">
<div class="section-intro"><div class="eyebrow">LIVE CONTROL CENTER</div><h2>Your game day at a glance</h2><p>Scores, favorites and celebrations—without changing what your lights are following.</p></div>
<div class="card hero"><div class="kicker">Today's NHL</div><h2>You're connected to the action</h2>
<div class="stats">
<div class="stat"><span>Games live</span><b id="dashLiveCount">—</b></div>
<div class="stat"><span>Favorite teams</span><b id="dashFavoriteCount">—</b></div>
<div class="stat"><span>Games followed</span><b id="dashFollowCount">—</b></div>
<div class="stat"><span>On-screen alerts</span><b id="dashDisplayMode">—</b></div>
</div>
<div class="actions"><button class="primary" data-tab="games">Browse games</button><button data-tab="display">Manage favorite teams</button><button data-tab="devices">Control WLED</button></div>
</div>
<div class="card"><h2>Device &amp; network health</h2>
<div class="stats">
<div class="stat"><span>Selected game</span><b id="currentGame">None</b></div>
<div class="stat"><span>Goal celebrations</span><b id="currentMode">Both teams</b></div>
<div class="stat"><span>Device IP</span><b id="deviceIp">—</b></div>
<div class="stat"><span>Free memory</span><b id="heap">—</b></div>
<div class="stat"><span>Stack headroom (minimum)</span><b id="stackWatermark">—</b></div>
<div class="stat"><span>Currently on screen</span><b id="showingGame">—</b></div>
<div class="stat"><span>Rotation</span><b id="rotationStatus">—</b></div>
<div class="stat"><span>NHL score check</span><b id="scorePollFrequency">—</b></div>
<div class="stat"><span>NHL connection</span><b id="nhlFeedState">—</b></div>
<div class="stat"><span>Last successful NHL update</span><b id="nhlLastSuccess">—</b></div>
</div>
<div class="actions"><button data-tab="settings">Network settings</button><button data-tab="firmware">Firmware update</button></div>
<div class="small muted" id="lastError"></div></div>
<div class="card"><h2>Latest league events</h2>
<p class="small muted">What the NHL feed reported and which lights GameDay commanded.</p>
<div id="recentGameEvents"><p class="muted">No events recorded yet.</p></div>
<button data-tab="events">View complete event log</button></div>
<div class="card"><h2>Armed game · WLED goal triggers</h2>
<p>Choose which side of the <strong>armed matchup</strong> triggers WLED. Starring a different live game follows it for rear RGB alerts and screen celebrations, without re-arming WLED.</p>
<div class="actions" id="modeButtons">
<button data-mode="0">Off</button><button data-mode="1">Home</button><button data-mode="2">Away</button><button data-mode="3">Both</button>
</div>
<div class="actions"><button id="testHome">Test home color</button><button id="testAway">Test away color</button></div>
<p class="small">A test saves the current WLED state, shows the celebration color, then restores the previous state.</p></div>
</section>
<section id="events" class="panel">
<div class="section-intro"><div class="eyebrow">ACTIVITY LOG</div><h2>What happened, and what fired</h2><p>League observations and the exact commands sent to your physical lights.</p></div>
<div class="card"><h2>Game events & light activity</h2>
<p>See NHL goals, power plays, puck drop and finals, plus whether the <b>CYD's built-in LED</b> and each selected <b>WLED LED strip/controller</b> received a celebration command.</p>
<p class="small muted">WLED “Command accepted” means the controller acknowledged the HTTP request. It does not independently confirm visible light output. The latest 64 events are retained in memory <b>until the device reboots</b>. No historical events are backfilled.</p>
<div class="row">
<div><label for="eventCategory">Filter by event</label><select id="eventCategory">
<option value="all">All events</option><option value="goals">Goals</option>
<option value="powerplays">Power plays</option><option value="other">Game start & final</option>
<option value="previews">Manual tests</option>
</select></div>
</div>
<div class="actions">
<button class="primary" id="refreshEventLog">Refresh log</button>
<button id="clearEventLog">Clear session log</button>
</div>
<div class="small muted" id="eventCount">No events yet</div>
<div id="eventList"><p class="muted">No events recorded yet.</p></div>
<div class="actions"><button id="eventNewer">Newer</button><button id="eventOlder">Older</button></div>
</div>
</section>
<section id="games" class="panel">
<div class="section-intro"><div class="eyebrow">NHL LIVE BOARD</div><h2>Every matchup, one place</h2><p>Tap the star to follow a game for rear RGB and screen celebrations. Use Arm to choose the one matchup that also controls WLED.</p></div>
<div class="card"><div class="row">
<div><h2>Today's games</h2><p class="small">Favorite-team games rise to the top of the live carousel.</p></div>
<div><label for="gameSearch">Search teams</label><input id="gameSearch" type="search" placeholder="Search DAL, Utah, Boston…" autocomplete="off"></div>
</div>
<div class="filterbar" role="group" aria-label="Filter today's games">
<button class="selected" data-game-filter="all" aria-pressed="true">All</button>
<button data-game-filter="live" aria-pressed="false">Live</button>
<button data-game-filter="favorites" aria-pressed="false">Favorite teams</button>
<button data-game-filter="following" aria-pressed="false">Following</button>
</div>
<div id="gameList"><p>No games loaded yet.</p></div>
<div id="gameDetail" class="item" hidden></div>
<button id="refreshGames">Refresh list</button>
<p class="small muted">Score checks run on the CYD every 20 seconds when a game is selected. NHL reporting can be delayed.</p></div>
</section>
<section id="display" class="panel">
<div class="section-intro"><div class="eyebrow">PERSONALIZE YOUR FEED</div><h2>Favorite teams &amp; screen alerts</h2><p>Follow the whole NHL, while giving the games you care about priority. Screen preferences do not change physical lighting targets.</p></div>
<div class="card"><h2>Permanent favorite teams</h2>
<p>Select any teams. Their live games appear first in the carousel, and their events get priority over other NHL alerts. These favorites persist across dates and do not automatically arm WLED or rear LEDs.</p>
<div id="favoriteTeamGrid" class="team-grid" role="group" aria-label="Choose favorite NHL teams"><p class="muted">Loading teams…</p></div>
<p class="small muted" id="favoriteTeamCount">No favorite teams selected.</p>
<div class="actions"><button class="primary" id="saveFavorites">Save favorite teams</button></div></div>
<div class="card"><h2>Today's game carousel</h2>
<p>During live NHL games, the CYD automatically focuses on full-screen live scoreboards and groups other upcoming or finished matches into readable four-game summary slides. When no games are live, it returns to the complete game-and-standings carousel. Swipe to browse or tap a summary row for its matchup details.</p>
<div class="stats">
<div class="stat"><span>Current screen</span><b id="displayGame">—</b></div>
<div class="stat"><span>Carousel slide</span><b id="displaySlide">—</b></div>
<div class="stat"><span>Rotation mode</span><b id="displayRotationMode">—</b></div>
<div class="stat"><span>League standings</span><b id="displayStandingsState">—</b></div>
<div class="stat"><span>Scoreboard date</span><b id="displayDate">—</b></div>
<div class="stat"><span>Local game date</span><b id="displayToday">—</b></div>
<div class="stat"><span>Clock synchronization</span><b id="displayClock">—</b></div>
</div>
<div class="toggle" style="margin-top:18px"><input type="checkbox" id="cycleGames"><label for="cycleGames">Automatically cycle through carousel slides</label></div>
<div class="toggle" style="margin-top:13px"><input type="checkbox" id="showStandings"><label for="showStandings">Include league-wide conference & division rankings</label></div>
<p class="small muted">When NHL games are LIVE, standings are temporarily removed from rotation to keep the focus on live scores. When no games are live, the eight conference and division slides return, if enabled. Armed goal alerts continue normally.</p>
<label for="cycleSeconds">Show each slide for (seconds)</label>
<input id="cycleSeconds" type="number" min="5" max="30" step="1" value="9">
<label for="displayTimezone">Game start time zone</label>
<select id="displayTimezone">
<option value="0">Eastern (ET)</option><option value="1">Central (CT)</option>
<option value="2">Mountain (MT)</option><option value="3">Pacific (PT)</option>
<option value="4">Arizona (AZ)</option>
</select>
</div><div class="card">
<h2>On-screen NHL event notifications</h2>
<p>Choose how widely the CYD reports live action. The screen can show events for every game while the rear LED and WLED continue following their independently selected matchups.</p>
<label for="displayAllLiveEvents">Show screen alerts for</label>
<select id="displayAllLiveEvents">
<option value="true">All live NHL games · recommended</option>
<option value="false">Armed and followed games only</option>
</select>
<p class="small muted">Priority: favorite-team alerts first, followed games second, all other live games third. Other-game notifications are short, low-priority events are throttled, and stale queued alerts are discarded.</p>
<div class="stats">
<div class="stat"><span>Background alerts throttled</span><b id="backgroundSkipped">0</b></div>
<div class="stat"><span>Overflow / duplicate suppression</span><b id="overlayDropped">0</b></div>
</div>
<div class="stats">
<div class="stat"><span>On-screen event</span><b id="overlayStatus">Idle</b></div>
<div class="stat"><span>Queued events</span><b id="overlayQueued">0</b></div>
</div>
<div class="toggle"><input type="checkbox" id="overlayEnabled"><label for="overlayEnabled">Enable event screen takeovers</label></div>
<div class="row" style="margin-top:12px">
<div class="toggle"><input id="overlayGoals" type="checkbox"><label for="overlayGoals">Goals</label></div>
<div class="toggle"><input id="overlayPowerPlays" type="checkbox"><label for="overlayPowerPlays">Power-play start/end</label></div>
<div class="toggle"><input id="overlayStart" type="checkbox"><label for="overlayStart">Game start</label></div>
<div class="toggle"><input id="overlayFinal" type="checkbox"><label for="overlayFinal">Final win/loss</label></div>
</div>
<div class="row" style="margin-top:12px">
<div class="toggle"><input id="overlayFlash" type="checkbox"><label for="overlayFlash">Brief goal border flashing</label></div>
<div class="toggle"><input id="overlayLogos" type="checkbox"><label for="overlayLogos">Display team logos</label></div>
</div>
<div class="row">
<div><label for="overlayGoalSeconds">Goal screen duration (3–12 seconds)</label>
<input id="overlayGoalSeconds" type="number" min="3" max="12" step="1" value="8"></div>
<div><label for="overlayFinalSeconds">Final screen duration (3–12 seconds)</label>
<input id="overlayFinalSeconds" type="number" min="3" max="12" step="1" value="7"></div>
</div>
<p class="small muted">A goal has priority over other overlays. The previous carousel, stats or standings page returns automatically. NHL polling and light effects continue independently. Flashing is limited to the colored screen border; the whole screen does not strobe.</p>
<div class="actions"><button id="overlayTestGoal">Preview goal</button>
<button id="overlayTestPP">Preview power play</button>
<button id="overlayTestPPEnd">Preview PP end</button>
<button id="overlayTestStart">Preview puck drop</button>
<button id="overlayTestWin">Preview final win</button>
<button id="overlayTestLoss">Preview final loss</button>
<button id="overlayStop">Dismiss overlay</button></div>
<p class="small muted">These previews affect the CYD screen only. Test them with your WLED devices disconnected if desired.</p>
<div class="actions"><button id="saveDisplay" class="primary">Save carousel &amp; alert settings</button></div>
</div>
<div class="hint">Carousel browsing does not arm WLED. Use the stars to follow rear RGB alerts, or Arm in Live games for WLED triggers.</div>
</section>
<section id="devices" class="panel">
<div class="section-intro"><div class="eyebrow">PHYSICAL CELEBRATIONS</div><h2>Your WLED light show</h2><p>Choose which controllers participate, test their effects, and restore their previous lighting afterward.</p></div>
<div class="card"><h2>WLED celebration engine</h2>
<p>Run matching event effects on the WLED controllers you choose. Each controller can participate in goals, power plays, game starts and final results independently.</p>
<div class="stats">
<div class="stat"><span>Playing now</span><b id="wledEventStatus">Idle</b></div>
<div class="stat"><span>Selected controllers</span><b id="wledSelectedCount">—</b></div>
</div>
<div class="row">
<div><label for="wledGoalStyle">Goal animation</label><select id="wledGoalStyle">
<option value="0">Strobe → Chase</option>
<option value="1">Strobe only</option>
<option value="2">Chase only</option>
<option value="3">Solid team color</option></select></div>
<div><label for="favoriteSide">Team to track for game-start and final result</label>
<select id="favoriteSide"><option value="0">Home team</option><option value="1">Away team</option></select></div>
</div>
<div class="toggle" style="margin-top:14px"><input id="wledPP" type="checkbox"><label for="wledPP">Pulse team color during an active power play</label></div>
<div class="toggle" style="margin-top:13px"><input id="wledGameStart" type="checkbox"><label for="wledGameStart">Game-start team-color sweep</label></div>
<div class="toggle" style="margin-top:13px"><input id="wledFinal" type="checkbox"><label for="wledFinal">Final win/loss effects</label></div>
<p class="small muted">Goal priority overrides power-play effects, then the pulse resumes if the advantage is still active. When an effect ends, each controller returns to its previous WLED state. WLED's own UDP broadcasting is suppressed for these commands so non-participating devices are not accidentally included.</p>
</div>
<div class="card"><h2>Select participating WLED devices</h2>
<p>Enter up to four local IP addresses. Switch on only the controllers you want involved, then choose which NHL events each one receives.</p>
<div id="deviceList"></div>
<div class="actions"><button class="primary" id="saveDevices">Save celebration settings</button></div>
<p class="small muted">All active segments on each selected WLED controller participate. You can use different WLED LED counts and segment layouts across devices.</p>
</div>
<div class="card"><h2>Preview celebrations</h2>
<p>Test all opted-in controllers at once, without waiting for a game. The selected game's team colors are used, or Utah's color when no game is armed.</p>
<div class="actions">
<button id="wledTestGoalHome">Home goal</button>
<button id="wledTestGoalAway">Away goal</button>
<button id="wledTestPP">Power play (9s)</button>
<button id="wledTestStart">Game start</button>
<button id="wledTestWin">Final win</button>
<button id="wledTestLoss">Final loss</button>
<button id="wledStop">Stop and restore</button>
</div>
<p class="small muted" id="wledDeviceFeedback">Controller status will appear here after a test.</p>
</div>
</section>
<section id="rear" class="panel">
<div class="section-intro"><div class="eyebrow">CYD LIGHTING</div><h2>Rear RGB &amp; followed games</h2><p>These onboard LED effects operate independently of your external WLED devices.</p></div>
<div class="card"><h2>Rear RGB LED</h2>
<p>Make the CYD's own rear light react to NHL events. No WLED controller or extra wiring is required.</p>
<div class="stats">
<div class="stat"><span>Current LED effect</span><b id="rearEvent">Idle</b></div>
<div class="stat"><span>Followed games</span><b id="rearFollowCount">—</b></div>
</div>
<div class="toggle" style="margin-top:18px"><input id="rearEnabled" type="checkbox"><label for="rearEnabled">Enable rear RGB LED</label></div>
<div class="toggle" style="margin-top:13px"><input id="rearGoal" type="checkbox"><label for="rearGoal">Flash scoring team's color for goals</label></div>
<div class="toggle" style="margin-top:13px"><input id="rearPowerPlay" type="checkbox"><label for="rearPowerPlay">Pulse attacking team's color during power plays</label></div>
<div class="row">
<div><label for="rearBrightness">Maximum brightness</label>
<input id="rearBrightness" type="range" min="1" max="255" value="128">
<div id="rearBrightValue" class="small muted">128 / 255</div></div>
<div><label for="rearGoalSeconds">Goal flash duration (seconds)</label>
<input id="rearGoalSeconds" type="number" min="2" max="12" step="1" value="6"></div>
</div>
<div class="actions"><button id="saveRear" class="primary">Save rear LED settings</button></div>
<div class="hint">The armed matchup always drives the rear LED while enabled. Follow up to four more games below. The rear LED is completely independent from WLED segment control.</div>
</div>
<div class="card"><h2>Follow games for rear LED</h2>
<p>Check games to monitor even while the screen carousel displays a different matchup. This selection lasts for today's schedule.</p>
<div id="rearFollowedList"></div>
</div>
<div class="card"><h2>Verify matchup colors</h2>
<p id="rearMatchup" class="small muted">Loading matchup…</p>
<div class="actions">
<button id="rearTestHome">Flash home goal</button>
<button id="rearTestAway">Flash away goal</button>
<button id="rearTestPP">Pulse home power play</button>
<button id="rearTestOff">Stop rear LED</button>
</div>
<p class="small muted">Home and away buttons use the armed matchup (or the featured game). They are disabled if no NHL game is loaded, rather than silently using the same fallback color.</p>
</div>
<div class="card"><h2>Choose any NHL team's primary color</h2>
<p class="small muted">Works without an armed or live game. All colors below come directly from the firmware team palette.</p>
<label for="rearTeam">NHL team</label>
<select id="rearTeam"><option value="">Loading team colors…</option></select>
<div class="actions" style="align-items:center">
<span id="rearTeamSwatch" style="width:24px;height:24px;display:inline-block;border-radius:6px;border:1px solid #86929b"></span>
<span id="rearSelectedHex" class="small">Team and hex color loading…</span>
</div>
<div class="actions">
<button id="rearTestTeam">Flash chosen team</button>
<button id="rearTestTeamPP">Pulse chosen team PP</button>
</div>
</div>
<div class="card"><h2>Hardware RGB channel diagnostics</h2>
<p class="small muted">Each button lights only one physical channel at solid brightness for three seconds. Use these to detect swapped, stuck or missing channels. No game or WLED device required.</p>
<div class="actions">
<button id="rearDiagRed">Red only</button>
<button id="rearDiagGreen">Green only</button>
<button id="rearDiagBlue">Blue only</button>
</div>
</div>
<div class="card"><h2>Full celebration preview</h2>
<p class="small muted">Run the rear CYD RGB LED, opted-in WLED devices and the animated 320×240 overlay using the same NHL team color. Each enabled output runs independently.</p>
<div class="actions">
<button id="rearFullHome">Celebrate home goal</button>
<button id="rearFullAway">Celebrate away goal</button>
<button id="rearFullTeam" class="primary">Celebrate chosen team</button>
<button id="rearFullStop">Stop / restore all</button>
</div>
<p id="rearFullResult" class="small muted">No full celebration test has run yet.</p>
</div></section>
<section id="settings" class="panel">
<div class="section-intro"><div class="eyebrow">DEVICE SETTINGS</div><h2>Network &amp; preferences</h2><p>Connectivity, local device options, horn sounds and security settings.</p></div>
<div class="twocol">
<div class="card"><h2>Wi-Fi</h2><p>Connect the CYD to the network used by your WLED controllers.</p>
<label for="ssid">Network name (SSID)</label><input id="ssid" autocomplete="off" maxlength="32" placeholder="Home Wi-Fi">
<div class="actions"><button id="scanWifi">Find nearby networks</button></div>
<select id="networks" aria-label="Found Wi-Fi networks"><option value="">Select a scanned network</option></select>
<label for="wifiPassword">Wi-Fi password</label><input id="wifiPassword" type="password" autocomplete="new-password" placeholder="Leave blank to keep existing">
<p class="small muted">The GlowDeck setup hotspot remains available at 192.168.4.1.</p></div>
<div class="card"><h2>Celebration preferences</h2>
<label for="brightness">WLED brightness</label><input id="brightness" type="range" min="1" max="255"><div class="small" id="brightValue"></div>
<label for="seconds">Celebration duration (seconds)</label><input id="seconds" type="number" min="2" max="30" step="1" value="8">
<div class="toggle" style="margin-top:18px"><input id="horn" type="checkbox"><label for="horn">Enable built-in horn (GPIO26 speaker)</label></div>
<p class="small muted">The onboard horn is a simple tone sequence. Custom WAV playback is not yet included.</p>
<div class="toggle" style="margin-top:18px"><input id="screenInverted" type="checkbox"><label for="screenInverted">Correct inverted CYD display colors</label></div>
<p class="small muted">Enable if the screen is pale blue or buttons appear pink instead of dark teal. It can also be toggled under SETUP directly on the touchscreen.</p>
<label for="adminPassword">Change dashboard password</label><input id="adminPassword" type="password" autocomplete="new-password" placeholder="Leave blank to keep existing">
<p class="small muted">Web username is always admin. Enter at least 8 characters to change its password.</p>
</div></div>
<div class="card"><button class="primary" id="saveSettings">Save setup</button>
<div class="hint">If you change Wi-Fi, reconnect your phone to GlowDeck's setup hotspot or the new home network to reopen the page.</div></div>
<div class="card"><h2>Connection information</h2>
<p>Setup hotspot: <code id="apName">—</code> at <code id="apIp">192.168.4.1</code></p>
<p>Local hostname: <code id="mdns">—</code></p>
<p>Uptime: <span id="uptime">—</span></p>
<p>Local calendar date: <span id="localToday">—</span></p>
<p>Time synchronization: <span id="clockState">—</span></p>
<p class="small muted">GameDay automatically changes to the next day's NHL schedule without restarting. If the clock cannot synchronize, verify internet/NTP access.</p></div>
</section>
<section id="firmware" class="panel">
<div class="section-intro"><div class="eyebrow">FIRMWARE</div><h2>Keep GameDay current</h2><p>Install a verified firmware.bin without losing saved Wi-Fi, favorites or event settings.</p></div>
<div class="card"><h2>Firmware update</h2>
<p>Upload a compiled PlatformIO <code>firmware.bin</code> from this project. Keep the CYD powered until it reboots.</p>
<form id="uploadForm"><label for="bin">Compiled firmware (.bin)</label><input id="bin" name="firmware" type="file" accept=".bin" required>
<div class="actions"><button class="primary" type="submit">Upload and reboot</button></div></form>
<p class="small muted">Initial installation still requires USB. This update control works only when you use the project's OTA-capable partition layout.</p></div>
<div class="card"><h2>Troubleshooting</h2><p>For boot loops, first flash the basic diagnostic firmware in the documentation. Touch calibration can be started from the CYD's SETUP screen. Watch serial output at 115200 baud.</p>
<div class="actions"><button id="reboot">Restart device</button></div></div>
</section>
</main>
<div id="notice" role="status" aria-live="polite"></div>
<script>
"use strict";
const $ = id => document.getElementById(id);
let currentConfig = null, currentStatus = null, currentGames = [], noticeTimeout;
let rearPalette = [];
let gameFilter="all", favoriteTeamDraftMask=0;
let detailPreviewToken = 0;
const modes = ["Off","Home only","Away only","Both teams"];
function message(text){
  const box = $("notice");box.textContent=text;box.style.display="block";
  clearTimeout(noticeTimeout);noticeTimeout=setTimeout(()=>box.style.display="none",4500);
}
async function api(url, options={}){
  const res = await fetch(url,{cache:"no-store",...options});
  const type = res.headers.get("content-type") || "";
  const body = type.includes("application/json") ? await res.json() : await res.text();
  if(!res.ok)throw new Error(body.error || (typeof body==="string" ? body : res.statusText));
  return body;
}
const post = (url,body) => api(url,{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify(body)});
async function perform(fn){try{await fn()}catch(err){message("Error: "+err.message)}}
function tab(id){
  document.querySelectorAll(".panel").forEach(x=>x.classList.toggle("active",x.id===id));
  document.querySelectorAll(".tab").forEach(x=>{
    const selected=x.dataset.tab===id;
    x.classList.toggle("selected",selected);
    if(selected)x.setAttribute("aria-current","page");
    else x.removeAttribute("aria-current");
  });
  scrollTo({top:0,behavior:"smooth"});
}
document.querySelectorAll("[data-tab]").forEach(x=>x.addEventListener("click",()=>tab(x.dataset.tab)));
let eventOffset=0, eventPageTotal=0;
function eventTimestamp(row){
  if(row.timestamp)
    return new Date(row.timestamp*1000).toLocaleString([],{
      month:"short",day:"numeric",hour:"numeric",minute:"2-digit",second:"2-digit"
    });
  return "Device uptime "+Math.floor((row.uptime||0)/60)+" min";
}
function createEventElement(row){
  const card=document.createElement("div");card.className="item";
  const heading=document.createElement("div");heading.className="game";
  const h=document.createElement("strong");
  h.textContent=(row.preview?"TEST · ":"")+(row.type||"Event")+
    (row.team?" — "+row.team:"");
  const when=document.createElement("small");
  when.className="muted";when.textContent=eventTimestamp(row);
  heading.append(h,when);card.append(heading);
  const score=document.createElement("div");score.className="small";
  const matchup=(row.away&&row.home)
    ?row.away+" "+row.awayScore+" – "+row.homeScore+" "+row.home
    :"Manual preview";
  score.textContent=matchup+(row.period?" · Period "+row.period:"")+
    ((row.goalsDelta||0)>1?" · "+row.goalsDelta+" score increase":"");
  score.style.margin="8px 0";
  card.append(score);
  const builtIn=document.createElement("div");builtIn.className="small";
  builtIn.textContent="CYD internal LED: "+(row.rear||"Not applicable");
  card.append(builtIn);
  const devices=document.createElement("div");devices.className="small muted";
  devices.style.marginTop="5px";
  const configured=currentConfig?.wled||[];
  const selected=(row.devices||[]).filter(d=>{
    const w=configured[d.slot-1]; return !!w?.enabled && !!w?.ip;
  });
  devices.textContent=selected.length
    ?selected.map(d=>
      "WLED "+d.slot+": "+(d.result||"Not triggered")+
      (d.accepted&&d.result!=="Command accepted"?" · command accepted":"")
    ).join("  |  ")
    :"WLED: No controllers enabled for celebrations";
  card.append(devices);
  return card;
}
function renderEventRows(targetId, rows){
  const target=$(targetId);target.replaceChildren();
  if(!rows.length){
    const note=document.createElement("p");note.className="muted";
    note.textContent="No matching events recorded in this session.";
    target.append(note);
    return;
  }
  rows.forEach(row=>target.append(createEventElement(row)));
}
async function refreshEventSummary(){
  const data=await api("/api/events?category=all&offset=0&limit=5");
  renderEventRows("recentGameEvents",data.events||[]);
  if($("events").classList.contains("active") && eventOffset===0)
    await loadEventLog();
}
async function loadEventLog(){
  const filter=$("eventCategory").value;
  const data=await api("/api/events?category="+encodeURIComponent(filter)+
    "&offset="+eventOffset+"&limit=12");
  eventPageTotal=data.total||0;
  renderEventRows("eventList",data.events||[]);
  $("eventCount").textContent=eventPageTotal+" matching event(s) · "+
    (data.sessionOnly?"Current device session · ":"")+
    "last "+data.capacity+" retained maximum";
  $("eventNewer").disabled=eventOffset===0;
  $("eventOlder").disabled=eventOffset+12>=eventPageTotal;
}
function renderStatus(){
  if(!currentStatus)return;
  const s=currentStatus, game=currentGames.find(g=>g.id===s.selectedId);
  $("connection").className="status "+(s.wifiConnected?"good":"warn");
  $("connection").textContent=s.wifiConnected?"Online":"Setup hotspot";
  $("deviceIp").textContent=s.ip;
  $("currentGame").textContent=game?game.away+" @ "+game.home:"Not selected";
  $("currentMode").textContent=modes[s.mode]||"Unknown";
  $("wledEventStatus").textContent=s.wledEvent||"IDLE";
  $("wledSelectedCount").textContent=(currentConfig?.wled||[])
    .filter(d=>d.enabled&&d.ip).length+" / 4";
  renderWledControllerResults();
  $("wledDeviceFeedback").textContent=(s.wledResults||[])
    .map((v,i)=>v?"WLED "+(i+1)+": "+v:"").filter(Boolean).join(" · ")
    ||"No recent WLED celebration activity";
  $("rearEvent").textContent=(s.rearEvent||"IDLE").replaceAll("_"," ");
  $("rearFollowCount").textContent=(s.followed||[]).length+
    (s.selectedId?" additional + armed":" additional");
  $("dashLiveCount").textContent=String(s.liveGameCount??0);
  $("dashFavoriteCount").textContent=String(popcount(s.favoriteTeams??0));
  $("dashFollowCount").textContent=String((s.followed||[]).length+(s.selectedId?1:0));
  $("dashDisplayMode").textContent=s.displayAllLiveEvents?"League-wide":"Followed only";
  $("backgroundSkipped").textContent=String(s.backgroundEventsSkipped??0);
  $("overlayDropped").textContent=String(s.overlayEventsDropped??0);
  renderRearTestControls();
  const showing=currentGames[s.featuredIndex]||null;
  $("showingGame").textContent=showing?showing.away+" @ "+showing.home:"No games";
  $("rotationStatus").textContent=s.cycleGames?"Every "+s.cycleSeconds+" sec":"Paused";
  $("scorePollFrequency").textContent=(s.scorePollSeconds||60)+" sec";
  $("nhlFeedState").textContent=s.nhlFailures
    ?"Retrying ("+s.nhlFailures+"), HTTP "+s.nhlLastHttp
    :s.scoreboardLoaded?"Connected":"Connecting";
  $("nhlLastSuccess").textContent=s.nhlLastSuccessAgeSeconds>=0
    ?s.nhlLastSuccessAgeSeconds+" sec ago":"Not yet received";
  $("displayGame").textContent=s.slideStandings
    ?s.standingsPage||"League standings"
    :s.slideSummary
      ?"Other games · page "+s.summaryPageNumber+"/"+s.summaryPageCount
      :showing?showing.away+" @ "+showing.home:"No games";
  $("displaySlide").textContent=((s.carouselSlide??0)+1)+" / "+(s.carouselTotal||1);
  $("displayRotationMode").textContent=s.liveMode
    ?s.liveGameCount+" live + "+s.summaryPageCount+
      " summary page"+(s.summaryPageCount===1?"":"s")
    :"Full games & standings";
  $("overlayStatus").textContent=s.overlayActive?
    (s.overlayEvent||"Event")+" "+(s.overlayTeam||"")+
    " · "+Math.ceil((s.overlayRemainingMs||0)/1000)+"s":"Idle";
  $("overlayQueued").textContent=String(s.overlayQueue??0);
  $("displayStandingsState").textContent=s.standingsReady?"Current cache loaded":"Loading or unavailable";
  $("displayDate").textContent=s.scoreDate||"Not loaded";
  $("displayToday").textContent=s.today||"Synchronizing";
  $("displayClock").textContent=s.clockSynced?"Synced":"Pending NTP";
  $("localToday").textContent=s.today||"Pending time sync";
  $("clockState").textContent=s.clockSynced?"Synchronized":"Waiting for network time";
  $("heap").textContent=Math.round(s.freeHeap/1024)+" KB";
  $("stackWatermark").textContent=(s.stackLowWater===undefined?"Unavailable":s.stackLowWater+" bytes");
  $("lastError").textContent=s.error||"";
  $("apName").textContent=s.apSsid;
  $("apIp").textContent=s.apIp;
  $("mdns").textContent=s.mdns||"Not yet connected";
  $("uptime").textContent=Math.floor(s.uptimeSeconds/60)+" minutes";
  document.querySelectorAll("[data-mode]").forEach(b=>b.classList.toggle("selected",Number(b.dataset.mode)===s.mode));
}
function popcount(mask){
  mask=Number(mask)>>>0;
  let count=0;
  while(mask){mask&=mask-1;++count;}
  return count;
}
function isFavoriteGame(g){
  return !!(currentStatus && rearPalette.some((t,i)=>
    ((Number(currentStatus.favoriteTeams)>>>i)&1) &&
    (t.abbr===g.away || t.abbr===g.home)));
}
function showGame(g){
  const live=g.state==="LIVE"||g.state==="CRIT";
  const following=g.followed || g.id===currentStatus?.selectedId;
  const query=$("gameSearch").value.toLowerCase().trim();
  const text=[g.away,g.home,g.awayName,g.homeName].join(" ").toLowerCase();
  return (!query||text.includes(query))&&
    (gameFilter==="all"||gameFilter==="live"&&live||
     gameFilter==="favorites"&&isFavoriteGame(g)||
     gameFilter==="following"&&following);
}
function renderGames(){
  const list=$("gameList");list.replaceChildren();
  if(!currentGames.length){
    const p=document.createElement("p");
    p.textContent=currentStatus&&currentStatus.scoreboardLoaded
      ? "No NHL games scheduled for "+(currentStatus.scoreDate||"today")+"."
      : "Loading today's NHL games. "+(currentStatus&&currentStatus.error||"Check Wi-Fi and time synchronization.");
    list.append(p);return;
  }
  const visible=currentGames.filter(showGame).sort((a,b)=>{
    const rank=g=>((g.state==="LIVE"||g.state==="CRIT")?4:0)+
      (isFavoriteGame(g)?2:0)+(g.followed?1:0);
    return rank(b)-rank(a);
  });
  if(!visible.length){
    const p=document.createElement("p");p.className="muted";
    p.textContent="No games match this filter.";
    list.append(p);return;
  }
  visible.forEach(g=>{
    const line=document.createElement("div");line.className="game game-row";
    const detail=document.createElement("div");detail.className="game-details";
    const matchup=document.createElement("div");
    matchup.style.display="flex";matchup.style.alignItems="center";matchup.style.gap="6px";
    function teamLogo(code){
      const pic=document.createElement("img");
      if(/^[A-Z]{3}$/.test(code)){
        pic.src="https://assets.nhle.com/logos/nhl/svg/"+code+"_light.svg";
      }
      pic.alt=code+" logo";pic.width=30;pic.height=30;
      pic.style.objectFit="contain";
      pic.onerror=()=>pic.style.display="none";
      return pic;
    }
    const score=document.createElement("div");score.className="score";
    score.textContent=(g.state==="FUT"||g.state==="PRE")?
      g.away+" @ "+g.home:
      g.away+" "+g.awayScore+" - "+g.homeScore+" "+g.home;
    matchup.append(teamLogo(g.away),score,teamLogo(g.home));
    const subtitle=document.createElement("div");
    subtitle.className="muted small";
    const flags=[];
    if(isFavoriteGame(g))flags.push("FAVORITE TEAM");
    if(g.followed)flags.push("FOLLOWED");
    if(g.id===currentStatus?.selectedId)flags.push("ARMED");
    subtitle.textContent=(g.status||g.state)+
      (flags.length?" · "+flags.join(" · "):"");
    detail.append(matchup,subtitle);
    const actions=document.createElement("div");actions.className="actions";
    actions.style.justifyContent="flex-end";
    const selected=!!currentStatus&&currentStatus.selectedId===g.id;
    const arm=document.createElement("button");
    arm.textContent=selected?"Armed":"Arm";if(selected)arm.className="selected";
    arm.title="Choose the game for goal-triggered lights";
    arm.onclick=()=>perform(async()=>{
      await post("/api/select",{id:g.id});
      message("Goal alerts armed: "+g.away+" @ "+g.home);
      await refresh();
    });
    const star=document.createElement("button");
    const isFollowing=!!g.followed||selected;
    star.className="star-button";
    star.textContent=isFollowing?"★":"☆";
    star.setAttribute("aria-label",(isFollowing?"Unfollow ":"Follow ")+g.away+" at "+g.home);
    star.setAttribute("aria-pressed",String(isFollowing));
    star.title=selected?"Armed game automatically follows rear LED and screen alerts":
      "Follow this game for screen celebrations and rear RGB LED";
    star.disabled=selected;
    star.onclick=()=>perform(async()=>{
      star.disabled=true;
      await post("/api/rear/follow",{id:g.id,follow:!isFollowing});
      await refresh();
      message((isFollowing?"Unfollowed ":"Following ")+g.away+" @ "+g.home+
        " · screen and rear RGB");
    });
    const stats=document.createElement("button");
    const isFuture=g.state==="FUT"||g.state==="PRE";
    stats.textContent=isFuture?"Preview":g.state==="LIVE"||g.state==="CRIT"?"Live stats":"Stats";
    stats.className="ghost";
    stats.title=isFuture
      ?"Season W-L-OT, vs-conference W-L-OT, conference rank and points"
      :"Shots, faceoffs, power plays and other game stats";
    stats.onclick=()=>perform(async()=>{
      const box=$("gameDetail");
      box.hidden=false;
      const selectedId=g.id;
      const token=++detailPreviewToken;
      box.replaceChildren(document.createTextNode(
        isFuture?"Loading season and conference records for "+g.away+" @ "+g.home+"..."
                :"Loading game stats for "+g.away+" @ "+g.home+"..."));
      // Conference game records take two low-priority NHL schedule requests.
      // Recheck the device until both are ready; never invent a zero record.
      for(let attempt=0;attempt<7;attempt++){
        const data=await api("/api/stats?id="+encodeURIComponent(selectedId));
        if(token!==detailPreviewToken)return;
        box.replaceChildren();
        const head=document.createElement("h3");
        head.textContent=g.away+" @ "+g.home+" — "+
          (data.pregame?"Pregame records":g.status||g.state);
        box.append(head);
        if(!data.stats||!data.stats.length){
          const p=document.createElement("p");p.className="muted";
          p.textContent=data.error||"Game data not yet published";
          box.append(p);
        }else{
          const table=document.createElement("table");table.style.width="100%";
          data.stats.forEach(row=>{
            const tr=document.createElement("tr");
            [row.away,row.label,row.home].forEach((v,j)=>{
              const td=document.createElement("td");td.textContent=String(v||"—");
              td.style.padding="7px";
              td.style.textAlign=j===0?"left":j===1?"center":"right";
              if(j===1)td.className="muted";
              tr.append(td);
            });
            table.append(tr);
          });
          box.append(table);
        }
        if(!data.pregame||!data.conferenceLoading)break;
        const status=document.createElement("p");status.className="small muted";
        status.textContent="Calculating records against conference opponents...";
        box.append(status);
        await new Promise(resolve=>setTimeout(resolve,2600));
      }
      if(token===detailPreviewToken)
        box.scrollIntoView({behavior:"smooth",block:"nearest"});
    });
    actions.append(star,stats,arm);
    line.append(detail,actions);list.append(line);
  });
}
document.querySelectorAll("[data-game-filter]").forEach(button=>{
  button.onclick=()=>{
    gameFilter=button.dataset.gameFilter;
    document.querySelectorAll("[data-game-filter]").forEach(el=>{
      const selected=el.dataset.gameFilter===gameFilter;
      el.classList.toggle("selected",selected);
      el.setAttribute("aria-pressed",String(selected));
    });
    renderGames();
  };
});
$("gameSearch").oninput=renderGames;
async function refresh(){
  const [s,g]=await Promise.all([api("/api/status"),api("/api/games")]);
  currentStatus=s;currentGames=g.games||[];
  renderStatus();renderGames();renderRearFollowed();
  await refreshEventSummary().catch(()=>{});
}
function renderDevices(){
  const list=$("deviceList");list.replaceChildren();
  (currentConfig.wled||[]).forEach((device,i)=>{
    const card=document.createElement("div");card.className="item";
    const title=document.createElement("h3");title.textContent="WLED "+(i+1);
    const label=document.createElement("label");label.htmlFor="ip"+i;label.textContent="Local IP address";
    const ip=document.createElement("input");ip.id="ip"+i;ip.placeholder="192.168.1.71";ip.value=device.ip||"";ip.inputMode="decimal";
    const row=document.createElement("div");row.className="toggle";row.style.marginTop="10px";
    const enable=document.createElement("input");enable.id="enabled"+i;enable.type="checkbox";enable.checked=!!device.enabled;
    const lbl=document.createElement("label");lbl.htmlFor=enable.id;lbl.textContent="Include this WLED device in celebrations";
    row.append(enable,lbl);card.append(title,label,ip,row);
    const flags=document.createElement("div");flags.className="row";flags.style.marginTop="10px";
    const events=[
      ["goal",1,"Goals"],["pp",2,"Power plays"],
      ["start",4,"Game start"],["final",8,"Final results"]
    ];
    const mask=Number(device.events??15);
    events.forEach(([name,bit,title])=>{
      const option=document.createElement("div");
      option.className="toggle";
      const cb=document.createElement("input");cb.id="event"+i+"_"+name;
      cb.type="checkbox";cb.checked=!!(mask&bit);
      const lab=document.createElement("label");lab.htmlFor=cb.id;lab.textContent=title;
      option.append(cb,lab);flags.append(option);
    });
    card.append(flags);
    const result=document.createElement("p");result.className="small muted";
    result.id="wledDeviceResult"+i;
    result.textContent="Status: not tested";
    card.append(result);list.append(card);
  });
  renderWledControllerResults();
}
function renderWledControllerResults(){
  if(!currentStatus)return;
  const statuses=currentStatus.wledResults||[];
  statuses.forEach((txt,i)=>{
    const el=$("wledDeviceResult"+i);
    if(el)el.textContent="Status: "+(txt||"Ready");
  });
}
function renderRearFollowed(){
  const list=$("rearFollowedList");
  list.replaceChildren();
  if(!currentGames.length){
    const p=document.createElement("p");p.className="muted";
    p.textContent="No NHL games listed for today.";list.append(p);return;
  }
  currentGames.forEach(game=>{
    const row=document.createElement("div");
    row.className="game";row.style.alignItems="center";
    const label=document.createElement("label");
    label.style.margin="0";label.style.flex="1";
    label.textContent=game.away+" @ "+game.home+" · "+(game.status||game.state);
    const checkbox=document.createElement("input");
    checkbox.type="checkbox";
    checkbox.style.width="22px";checkbox.style.minHeight="22px";
    checkbox.style.accentColor="#31837a";
    const isArmed=currentStatus&&currentStatus.selectedId===game.id;
    checkbox.checked=!!game.followed||!!isArmed;
    checkbox.disabled=!!isArmed;
    checkbox.setAttribute("aria-label","Follow "+game.away+" at "+game.home);
    checkbox.onchange=()=>perform(async()=>{
      checkbox.disabled=true;
      try{
        await post("/api/rear/follow",{id:game.id,follow:checkbox.checked});
        await refresh();
        message((checkbox.checked?"Following ":"Stopped following ")+game.away+" @ "+game.home);
      }catch(err){
        checkbox.disabled=false;
        await refresh();
        throw err;
      }
    });
    row.append(label,checkbox);
    if(isArmed){
      const note=document.createElement("span");
      note.className="badge";note.textContent="Armed";row.append(note);
    }
    list.append(row);
  });
}
function rearColor(code){
  const match=rearPalette.find(t=>t.abbr===code);
  return match?match.hex:"Unknown";
}
function rearMatchupGame(){
  if(!currentStatus)return null;
  return currentGames.find(g=>g.id===currentStatus.selectedId)
    ||currentGames[currentStatus.featuredIndex||0]||null;
}
function renderRearSelected(){
  const code=$("rearTeam").value, hex=rearColor(code);
  $("rearSelectedHex").textContent=code+" · "+hex;
  $("rearTeamSwatch").style.backgroundColor=hex.startsWith("#")?hex:"transparent";
}
function renderRearTestControls(){
  const g=rearMatchupGame(), armed=g&&g.id===currentStatus.selectedId;
  const fmt=code=>code+" ("+rearColor(code)+")";
  $("rearMatchup").textContent=g
    ?(armed?"Armed matchup: ":"Featured matchup: ")+
      "Away "+fmt(g.away)+" @ Home "+fmt(g.home)
    :"No game available. Choose a team below for color tests.";
  for(const id of ["rearTestHome","rearTestAway","rearTestPP","rearFullHome","rearFullAway"])
    $(id).disabled=!g;
  $("rearTestHome").textContent=g?"Flash home "+fmt(g.home):"No home team";
  $("rearTestAway").textContent=g?"Flash away "+fmt(g.away):"No away team";
  $("rearFullHome").textContent=g?"Celebrate home "+fmt(g.home):"No home team";
  $("rearFullAway").textContent=g?"Celebrate away "+fmt(g.away):"No away team";
}
async function loadRearPalette(){
  const response=await api("/api/rear/palette");
  rearPalette=response.teams||[];
  const select=$("rearTeam"), old=select.value||"UTA";
  select.replaceChildren();
  rearPalette.forEach(item=>select.add(new Option(item.abbr+" · "+item.hex,item.abbr)));
  select.value=rearPalette.some(item=>item.abbr===old)?old:"UTA";
  renderRearSelected();
  renderRearTestControls();
  renderFavoriteTeams();
}
function renderFavoriteTeams(){
  const grid=$("favoriteTeamGrid");grid.replaceChildren();
  rearPalette.forEach((team,index)=>{
    const checked=!!((favoriteTeamDraftMask>>>index)&1);
    const button=document.createElement("button");
    button.type="button";button.className="team-choice";
    button.setAttribute("aria-pressed",String(checked));
    button.setAttribute("aria-label",(checked?"Remove ":"Add ")+team.abbr+" as favorite team");
    const dot=document.createElement("span");
    dot.className="team-dot";dot.style.backgroundColor=team.hex;
    const title=document.createElement("span");title.textContent=team.abbr;
    button.append(dot,title);
    button.onclick=()=>{
      favoriteTeamDraftMask=(favoriteTeamDraftMask^(1<<index))>>>0;
      renderFavoriteTeams();
    };
    grid.append(button);
  });
  const count=popcount(favoriteTeamDraftMask);
  $("favoriteTeamCount").textContent=count?count+
    " favorite team"+(count===1?"":"s")+" selected · Save to apply across game days."
    :"No favorites selected. Every live game will have equal screen priority.";
}
$("saveFavorites").onclick=()=>perform(async()=>{
  const data=await post("/api/display/favorites",{mask:favoriteTeamDraftMask});
  currentConfig.favoriteTeams=data.favoriteTeams;
  await refresh();
  message(popcount(data.favoriteTeams)+" favorite teams saved");
});
function deviceSettings(){
  return Array.from({length:4},(_,i)=>{
    const choices=[["goal",1],["pp",2],["start",4],["final",8]];
    const events=choices.reduce((mask,[name,bit])=>
      mask+($("event"+i+"_"+name).checked?bit:0),0);
    return {ip:$("ip"+i).value.trim(),enabled:$("enabled"+i).checked,events};
  });
}
function formConfig(){
  return {ssid:$("ssid").value.trim(),password:$("wifiPassword").value,
    adminPassword:$("adminPassword").value,
    mode:currentStatus?currentStatus.mode:currentConfig.mode,
    brightness:Number($("brightness").value),seconds:Number($("seconds").value),
    horn:$("horn").checked,screenInverted:$("screenInverted").checked,
    wledPP:$("wledPP").checked,wledGameStart:$("wledGameStart").checked,
    wledFinal:$("wledFinal").checked,
    wledGoalStyle:Number($("wledGoalStyle").value),
    favoriteSide:Number($("favoriteSide").value),
    rearEnabled:$("rearEnabled").checked,rearGoal:$("rearGoal").checked,
    rearPowerPlay:$("rearPowerPlay").checked,
    rearBrightness:Number($("rearBrightness").value),
    rearGoalSeconds:Number($("rearGoalSeconds").value),
    overlayEnabled:$("overlayEnabled").checked,
    overlayGoals:$("overlayGoals").checked,
    overlayPowerPlays:$("overlayPowerPlays").checked,
    overlayStart:$("overlayStart").checked,
    overlayFinal:$("overlayFinal").checked,
    overlayFlash:$("overlayFlash").checked,
    overlayLogos:$("overlayLogos").checked,
    displayAllLiveEvents:$("displayAllLiveEvents").value==="true",
    overlayGoalSeconds:Number($("overlayGoalSeconds").value),
    overlayFinalSeconds:Number($("overlayFinalSeconds").value),
    cycleGames:$("cycleGames").checked,showStandings:$("showStandings").checked,
    cycleSeconds:Number($("cycleSeconds").value),
    timezone:Number($("displayTimezone").value),wled:deviceSettings()};
}
async function saveConfig(){
  const data=formConfig();
  await post("/api/config",data);
  $("wifiPassword").value="";$("adminPassword").value="";
  currentConfig=await api("/api/config");
  await refresh();
  message("Settings saved on the CYD");
}
async function loadConfig(){
  currentConfig=await api("/api/config");
  $("ssid").value=currentConfig.ssid||"";
  $("brightness").value=currentConfig.brightness;
  $("brightValue").textContent=currentConfig.brightness+" / 255";
  $("seconds").value=currentConfig.seconds;
  $("horn").checked=!!currentConfig.horn;
  $("wledPP").checked=currentConfig.wledPP!==false;
  $("wledGameStart").checked=currentConfig.wledGameStart!==false;
  $("wledFinal").checked=currentConfig.wledFinal!==false;
  $("wledGoalStyle").value=String(currentConfig.wledGoalStyle??0);
  $("favoriteSide").value=String(currentConfig.favoriteSide??0);
  $("rearEnabled").checked=!!currentConfig.rearEnabled;
  $("rearGoal").checked=!!currentConfig.rearGoal;
  $("rearPowerPlay").checked=!!currentConfig.rearPowerPlay;
  $("rearBrightness").value=currentConfig.rearBrightness||128;
  $("rearBrightValue").textContent=$("rearBrightness").value+" / 255";
  $("rearGoalSeconds").value=currentConfig.rearGoalSeconds||6;
  $("screenInverted").checked=!!currentConfig.screenInverted;
  $("overlayEnabled").checked=currentConfig.overlayEnabled!==false;
  $("overlayGoals").checked=currentConfig.overlayGoals!==false;
  $("overlayPowerPlays").checked=currentConfig.overlayPowerPlays!==false;
  $("overlayStart").checked=currentConfig.overlayStart!==false;
  $("overlayFinal").checked=currentConfig.overlayFinal!==false;
  $("overlayFlash").checked=currentConfig.overlayFlash!==false;
  $("overlayLogos").checked=currentConfig.overlayLogos!==false;
  $("displayAllLiveEvents").value=currentConfig.displayAllLiveEvents===false?"false":"true";
  favoriteTeamDraftMask=Number(currentConfig.favoriteTeams??0)>>>0;
  $("overlayGoalSeconds").value=currentConfig.overlayGoalSeconds||8;
  $("overlayFinalSeconds").value=currentConfig.overlayFinalSeconds||7;
  $("cycleGames").checked=!!currentConfig.cycleGames;
  $("showStandings").checked=currentConfig.showStandings!==false;
  $("cycleSeconds").value=currentConfig.cycleSeconds||9;
  $("displayTimezone").value=String(currentConfig.timezone ?? 1);
  renderDevices();
  await loadRearPalette();
}
document.querySelectorAll("[data-mode]").forEach(b=>b.addEventListener("click",()=>perform(async()=>{
  const mode=Number(b.dataset.mode);
  await post("/api/config",{mode});
  await refresh();message("Celebrations: "+modes[mode]);
})));
$("saveSettings").onclick=()=>perform(saveConfig);
$("saveDisplay").onclick=()=>perform(saveConfig);
function previewOverlay(kind,side="home"){
  perform(async()=>{
    await post("/api/overlay/preview",{kind,side});
    await refresh();
    message(kind==="stop"?"On-screen overlay dismissed":
      "CYD "+kind+" overlay preview started");
  });
}
$("overlayTestGoal").onclick=()=>previewOverlay("goal");
$("overlayTestPP").onclick=()=>previewOverlay("pp");
$("overlayTestPPEnd").onclick=()=>previewOverlay("ppend");
$("overlayTestStart").onclick=()=>previewOverlay("start");
$("overlayTestWin").onclick=()=>previewOverlay("win");
$("overlayTestLoss").onclick=()=>previewOverlay("loss");
$("overlayStop").onclick=()=>previewOverlay("stop");
$("eventCategory").onchange=()=>{eventOffset=0;perform(loadEventLog)};
$("refreshEventLog").onclick=()=>perform(loadEventLog);
$("eventOlder").onclick=()=>{eventOffset+=12;perform(loadEventLog)};
$("eventNewer").onclick=()=>{eventOffset=Math.max(0,eventOffset-12);perform(loadEventLog)};
$("clearEventLog").onclick=()=>perform(async()=>{
  if(!confirm("Clear the current device session's event history?"))return;
  await post("/api/events/clear",{});
  eventOffset=0;
  await loadEventLog();
  await refreshEventSummary();
  message("Session event log cleared");
});
$("saveDevices").onclick=()=>perform(saveConfig);
function wledPreview(event,side="home"){
  perform(async()=>{
    const result=await post("/api/wled/preview",{event,side});
    await refresh();
    if(!result.ok)throw Error("No opted-in WLED controller accepted the effect. Check IP addresses, selected events and device status.");
    message(event==="stop"?"Restore requested on selected WLED devices":
      event.toUpperCase()+" preview started on "+result.devices+" selected controller(s)");
  });
}
$("wledTestGoalHome").onclick=()=>wledPreview("goal","home");
$("wledTestGoalAway").onclick=()=>wledPreview("goal","away");
$("wledTestPP").onclick=()=>wledPreview("pp");
$("wledTestStart").onclick=()=>wledPreview("start");
$("wledTestWin").onclick=()=>wledPreview("win");
$("wledTestLoss").onclick=()=>wledPreview("loss");
$("wledStop").onclick=()=>wledPreview("stop");
$("saveRear").onclick=()=>perform(saveConfig);
$("rearBrightness").oninput=()=>$("rearBrightValue").textContent=$("rearBrightness").value+" / 255";
function rearPreview(mode,side="home",team="",channel=""){
  perform(async()=>{
    const result=await post("/api/rear/test",{mode,side,team,channel});
    await refresh();
    message(mode==="off"?"Rear LED stopped":
      (mode==="channel"?"Hardware channel ":"Rear "+mode+" test: ")+
      result.team+" "+result.hex);
  });
}
function selectedRearTeam(){return $("rearTeam").value}
$("rearTeam").onchange=renderRearSelected;
$("rearTestHome").onclick=()=>rearPreview("goal","home");
$("rearTestAway").onclick=()=>rearPreview("goal","away");
$("rearTestPP").onclick=()=>rearPreview("powerplay","home");
$("rearTestOff").onclick=()=>rearPreview("off");
$("rearTestTeam").onclick=()=>rearPreview("goal","home",selectedRearTeam());
$("rearTestTeamPP").onclick=()=>rearPreview("powerplay","home",selectedRearTeam());
$("rearDiagRed").onclick=()=>rearPreview("channel","home","","red");
$("rearDiagGreen").onclick=()=>rearPreview("channel","home","","green");
$("rearDiagBlue").onclick=()=>rearPreview("channel","home","","blue");
function fullPreview(event,side="home",team=""){
  perform(async()=>{
    const result=await post("/api/wled/preview",{event,side,team,combined:true});
    await refresh();
    const parts=[
      "Rear LED "+(result.rearStarted?"started":"not enabled"),
      "WLED "+(result.wledStarted?"started":"no controller accepted"),
      "Screen "+(result.overlayStarted?"started":"not enabled")
    ];
    const description=event==="stop"?"Stop / restore requested":
      result.team+" "+result.hex+" · "+parts.join(" · ");
    $("rearFullResult").textContent=description;
    message(description);
  });
}
$("rearFullHome").onclick=()=>fullPreview("goal","home");
$("rearFullAway").onclick=()=>fullPreview("goal","away");
$("rearFullTeam").onclick=()=>fullPreview("goal","home",selectedRearTeam());
$("rearFullStop").onclick=()=>fullPreview("stop");
$("testHome").onclick=()=>perform(async()=>{await post("/api/test",{side:"home"});message("Home color test started")});
$("testAway").onclick=()=>perform(async()=>{await post("/api/test",{side:"away"});message("Away color test started")});

$("refreshGames").onclick=()=>perform(async()=>{
  await post("/api/refresh",{});
  message("Fresh NHL schedule requested; the device will update shortly.");
  await refresh();
});
$("brightness").oninput=()=>{$("brightValue").textContent=$("brightness").value+" / 255"};
$("scanWifi").onclick=()=>perform(async()=>{
  message("Scanning nearby Wi-Fi networks...");
  const data=await api("/api/wifi/scan");
  const el=$("networks");el.replaceChildren(new Option("Choose a scanned network",""));
  (data.networks||[]).forEach(n=>{if(n.ssid)el.add(new Option(n.ssid+" ("+n.rssi+" dBm)",n.ssid))});
  message("Wi-Fi scan complete");
});
$("networks").onchange=e=>{if(e.target.value)$("ssid").value=e.target.value};
$("uploadForm").onsubmit=e=>{e.preventDefault();perform(async()=>{
  const file=$("bin").files[0];if(!file)return;
  if(!confirm("Flash "+file.name+" and restart the CYD?"))return;
  const data=new FormData();data.append("firmware",file);
  const res=await fetch("/api/update",{method:"POST",body:data});
  const text=await res.text();if(!res.ok)throw new Error(text);
  message(text);
})};
$("reboot").onclick=()=>perform(async()=>{
  if(!confirm("Restart the CYD now?"))return;
  await api("/api/reboot",{method:"POST"});message("Restart requested");
});
(async()=>{
  try{await loadConfig();await refresh()}
  catch(err){message("Cannot load device: "+err.message)}
  setInterval(()=>refresh().catch(()=>{}),15000);
})();
</script>
</body>
</html>
)GLOWDECK";
