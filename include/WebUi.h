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
:root{color-scheme:dark;--bg:#0a141c;--panel:#111f29;--edge:#29414b;--primary:#31837a;--text:#f4f9fa;--muted:#9ab0b6;--amber:#f4a64e}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:15px/1.5 system-ui,-apple-system,"Segoe UI",sans-serif}
header{padding:22px max(20px,calc((100vw - 940px)/2));background:linear-gradient(155deg,#112a32,#0a141c);border-bottom:1px solid var(--edge)}
.eyebrow{color:#89e0d0;font-weight:800;letter-spacing:.16em;font-size:11px}.top{display:flex;justify-content:space-between;gap:12px;align-items:center}h1{font-size:clamp(23px,4vw,32px);line-height:1.15;margin:4px 0}p{color:var(--muted);margin:4px 0}
main{max-width:940px;margin:auto;padding:16px 16px 90px}.tabs{position:sticky;top:0;display:flex;gap:6px;overflow:auto;padding:10px 0 16px;background:var(--bg);z-index:5}
.tab{white-space:nowrap;flex:1;min-width:80px}.panel{display:none}.panel.active{display:block}.card{background:var(--panel);border:1px solid var(--edge);border-radius:16px;padding:18px;margin-bottom:14px}
h2{font-size:18px;margin:0 0 12px}h3{font-size:15px;margin:0 0 8px}.muted{color:var(--muted)}.small{font-size:12px}
.stats{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:10px}.stat{background:#0d1b25;border-radius:11px;padding:12px}.stat b{display:block;font-size:16px;overflow-wrap:anywhere}.stat span{font-size:12px;color:var(--muted)}
button{font:inherit;color:var(--text);background:#1d343c;border:1px solid #315059;border-radius:10px;padding:10px 13px;cursor:pointer;min-height:42px;font-weight:650}
button:hover{border-color:#68b5a8}button.primary,button.selected{background:var(--primary);border-color:var(--primary);color:white}
button:disabled{opacity:.45;cursor:not-allowed}button.ghost{background:transparent}.actions{display:flex;flex-wrap:wrap;gap:8px;margin:12px 0}
label{display:block;font-size:13px;color:#c4d5d8;margin:11px 0 5px;font-weight:650}input,select{color:var(--text);background:#08141b;border:1px solid #36505a;border-radius:10px;padding:11px 12px;font:inherit;width:100%;min-height:43px}
input:focus,select:focus{outline:2px solid var(--primary);outline-offset:1px}.row{display:grid;grid-template-columns:repeat(auto-fit,minmax(190px,1fr));gap:11px}.toggle{display:flex;align-items:center;gap:8px}
.toggle input{width:20px;height:20px;min-height:auto;accent-color:#31837a}.toggle label{margin:0}
.game{display:flex;justify-content:space-between;align-items:center;gap:10px;padding:12px 0;border-bottom:1px solid var(--edge)}
.game:last-child{border-bottom:0}.game .score{font-size:19px;font-weight:800}.badge{background:#1a4847;border-radius:100px;padding:3px 8px;font-size:11px;color:#c0f1e8}
.item{border:1px solid var(--edge);border-radius:12px;padding:13px;margin:10px 0;background:#0d1b25}
.status{display:inline-flex;border-radius:20px;padding:5px 10px;font-size:12px;background:#26363d;color:#d3e1e4}
.status.good{background:#17483f;color:#b5f1df}.status.warn{background:#523821;color:#ffe0af}
#notice{position:fixed;bottom:18px;right:16px;left:16px;max-width:440px;margin:auto;z-index:11;background:#1e4246;color:white;border:1px solid #55948e;border-radius:12px;padding:13px;box-shadow:0 9px 28px #0009;display:none}
code{color:#93e2d5;word-break:break-word}.hint{border-left:3px solid var(--primary);padding-left:12px;margin:10px 0;color:var(--muted)}
@media(min-width:700px){.twocol{display:grid;grid-template-columns:1fr 1fr;gap:15px}.twocol .card{margin-bottom:0}}
</style>
</head>
<body>
<header>
<div class="top">
<div><div class="eyebrow">GLOWDECK / ESP32 CYD</div><h1>GameDay Control</h1><p>Your standalone NHL-to-WLED game companion</p></div>
<div id="connection" class="status">Connecting</div>
</div>
</header>
<main>
<nav class="tabs" aria-label="Sections">
<button class="tab selected" data-tab="dashboard">Dashboard</button>
<button class="tab" data-tab="games">Games</button>
<button class="tab" data-tab="display">Display</button>
<button class="tab" data-tab="devices">Lights</button>
<button class="tab" data-tab="settings">Setup</button>
<button class="tab" data-tab="firmware">Update</button>
</nav>
<section id="dashboard" class="panel active">
<div class="card"><h2>Game-day status</h2>
<div class="stats">
<div class="stat"><span>Selected game</span><b id="currentGame">None</b></div>
<div class="stat"><span>Goal celebrations</span><b id="currentMode">Both teams</b></div>
<div class="stat"><span>Device IP</span><b id="deviceIp">—</b></div>
<div class="stat"><span>Free memory</span><b id="heap">—</b></div>
<div class="stat"><span>Currently on screen</span><b id="showingGame">—</b></div>
<div class="stat"><span>Rotation</span><b id="rotationStatus">—</b></div>
</div>
<div class="actions"><button class="primary" data-tab="games">Choose game</button><button data-tab="devices">Manage lights</button></div>
<div class="small muted" id="lastError"></div></div>
<div class="card"><h2>Celebration controls</h2>
<p>Choose which team can trigger lights when the NHL score changes. The device saves this automatically.</p>
<div class="actions" id="modeButtons">
<button data-mode="0">Off</button><button data-mode="1">Home</button><button data-mode="2">Away</button><button data-mode="3">Both</button>
</div>
<div class="actions"><button id="testHome">Test home color</button><button id="testAway">Test away color</button></div>
<p class="small">A test saves the current WLED state, shows the celebration color, then restores the previous state.</p></div>
</section>
<section id="games" class="panel">
<div class="card"><h2>Today's NHL games</h2><p>Tap a matchup to arm it on your CYD. The first poll records current scores without replaying earlier goals.</p>
<div id="gameList"><p>No games loaded yet.</p></div>
<div id="gameDetail" class="item" hidden></div>
<button id="refreshGames">Refresh list</button>
<p class="small muted">Score checks run on the CYD every 20 seconds when a game is selected. NHL reporting can be delayed.</p></div>
</section>
<section id="display" class="panel">
<div class="card"><h2>Today's game carousel</h2>
<p>Cycle through today's NHL schedule on the CYD, with official team logos, scheduled starts, live period/clock, scores, shots and final results. The CYD now uses a full-screen broadcast layout without persistent TODAY, GAMES or SETUP buttons. Swipe across the screen for next/previous, tap the main scoreboard to view stats, or tap the tiny top-right menu icon to reach the game list and setup.</p>
<div class="stats">
<div class="stat"><span>Current screen</span><b id="displayGame">—</b></div>
<div class="stat"><span>Scoreboard date</span><b id="displayDate">—</b></div>
<div class="stat"><span>Local game date</span><b id="displayToday">—</b></div>
<div class="stat"><span>Clock synchronization</span><b id="displayClock">—</b></div>
</div>
<div class="toggle" style="margin-top:18px"><input type="checkbox" id="cycleGames"><label for="cycleGames">Automatically cycle through games</label></div>
<label for="cycleSeconds">Show each game for (seconds)</label>
<input id="cycleSeconds" type="number" min="5" max="30" step="1" value="9">
<label for="displayTimezone">Game start time zone</label>
<select id="displayTimezone">
<option value="0">Eastern (ET)</option><option value="1">Central (CT)</option>
<option value="2">Mountain (MT)</option><option value="3">Pacific (PT)</option>
<option value="4">Arizona (AZ)</option>
</select>
<div class="actions"><button id="saveDisplay" class="primary">Save display settings</button></div>
<div class="hint">Browsing rotating games does not change which matchup triggers goal celebrations. Use ARM GOALS on the CYD or ARM in the Games tab.</div>
</div></section>
<section id="devices" class="panel">
<div class="card"><h2>WLED devices</h2>
<p>Add up to four WLED controllers on the same Wi-Fi network as your CYD. Each enabled controller will receive a goal celebration.</p>
<div id="deviceList"></div>
<div class="actions"><button class="primary" id="saveDevices">Save light settings</button><button id="testLights">Test all enabled lights</button></div>
<p class="small muted">Enter IPv4 addresses such as 192.168.1.71. For stable operation, reserve those IPs in your router. This release controls segment 0; per-segment routing is planned.</p></div>
</section>
<section id="settings" class="panel">
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
  document.querySelectorAll(".tab").forEach(x=>x.classList.toggle("selected",x.dataset.tab===id));
  scrollTo({top:0,behavior:"smooth"});
}
document.querySelectorAll("[data-tab]").forEach(x=>x.addEventListener("click",()=>tab(x.dataset.tab)));
function renderStatus(){
  if(!currentStatus)return;
  const s=currentStatus, game=currentGames.find(g=>g.id===s.selectedId);
  $("connection").className="status "+(s.wifiConnected?"good":"warn");
  $("connection").textContent=s.wifiConnected?"Online":"Setup hotspot";
  $("deviceIp").textContent=s.ip;
  $("currentGame").textContent=game?game.away+" @ "+game.home:"Not selected";
  $("currentMode").textContent=modes[s.mode]||"Unknown";
  const showing=currentGames[s.featuredIndex]||null;
  $("showingGame").textContent=showing?showing.away+" @ "+showing.home:"No games";
  $("rotationStatus").textContent=s.cycleGames?"Every "+s.cycleSeconds+" sec":"Paused";
  $("displayGame").textContent=showing?showing.away+" @ "+showing.home:"No games";
  $("displayDate").textContent=s.scoreDate||"Not loaded";
  $("displayToday").textContent=s.today||"Synchronizing";
  $("displayClock").textContent=s.clockSynced?"Synced":"Pending NTP";
  $("localToday").textContent=s.today||"Pending time sync";
  $("clockState").textContent=s.clockSynced?"Synchronized":"Waiting for network time";
  $("heap").textContent=Math.round(s.freeHeap/1024)+" KB";
  $("lastError").textContent=s.error||"";
  $("apName").textContent=s.apSsid;
  $("apIp").textContent=s.apIp;
  $("mdns").textContent=s.mdns||"Not yet connected";
  $("uptime").textContent=Math.floor(s.uptimeSeconds/60)+" minutes";
  document.querySelectorAll("[data-mode]").forEach(b=>b.classList.toggle("selected",Number(b.dataset.mode)===s.mode));
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
  currentGames.forEach(g=>{
    const line=document.createElement("div");line.className="game";
    const detail=document.createElement("div");detail.style.flex="1";
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
    subtitle.className="muted small";subtitle.textContent=g.status||g.state;
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
    const stats=document.createElement("button");
    stats.textContent="Stats";stats.className="ghost";
    stats.title="View team comparisons for this matchup";
    stats.onclick=()=>perform(async()=>{
      $("gameDetail").hidden=false;
      $("gameDetail").replaceChildren(document.createTextNode("Loading stats for "+g.away+" @ "+g.home+"..."));
      const data=await api("/api/stats?id="+encodeURIComponent(g.id));
      const box=$("gameDetail");box.replaceChildren();
      const head=document.createElement("h3");
      head.textContent=g.away+" @ "+g.home+" — "+(g.status||g.state);
      box.append(head);
      if(!data.stats||!data.stats.length){
        const p=document.createElement("p");p.className="muted";
        p.textContent=data.error||"Detailed game stats not yet published";
        box.append(p);
      }else{
        const table=document.createElement("table");table.style.width="100%";
        data.stats.forEach(row=>{
          const tr=document.createElement("tr");
          [row.away,row.label,row.home].forEach((v,j)=>{
            const td=document.createElement("td");td.textContent=String(v||"—");
            td.style.padding="7px";td.style.textAlign=j===0?"left":j===1?"center":"right";
            if(j===1)td.className="muted";
            tr.append(td);
          });
          table.append(tr);
        });
        box.append(table);
      }
      box.scrollIntoView({behavior:"smooth",block:"nearest"});
    });
    actions.append(stats,arm);
    line.append(detail,actions);list.append(line);
  });
}
async function refresh(){
  const [s,g]=await Promise.all([api("/api/status"),api("/api/games")]);
  currentStatus=s;currentGames=g.games||[];
  renderStatus();renderGames();
}
function renderDevices(){
  const list=$("deviceList");list.replaceChildren();
  (currentConfig.wled||[]).forEach((device,i)=>{
    const card=document.createElement("div");card.className="item";
    const title=document.createElement("h3");title.textContent="WLED "+(i+1);
    const label=document.createElement("label");label.htmlFor="ip"+i;label.textContent="IP address";
    const ip=document.createElement("input");ip.id="ip"+i;ip.placeholder="192.168.1.71";ip.value=device.ip||"";ip.inputMode="decimal";
    const row=document.createElement("div");row.className="toggle";row.style.marginTop="10px";
    const enable=document.createElement("input");enable.id="enabled"+i;enable.type="checkbox";enable.checked=!!device.enabled;
    const lbl=document.createElement("label");lbl.htmlFor=enable.id;lbl.textContent="Use for celebrations";
    row.append(enable,lbl);card.append(title,label,ip,row);list.append(card);
  });
}
function deviceSettings(){
  return Array.from({length:4},(_,i)=>({ip:$("ip"+i).value.trim(),enabled:$("enabled"+i).checked}));
}
function formConfig(){
  return {ssid:$("ssid").value.trim(),password:$("wifiPassword").value,
    adminPassword:$("adminPassword").value,
    mode:currentStatus?currentStatus.mode:currentConfig.mode,
    brightness:Number($("brightness").value),seconds:Number($("seconds").value),
    horn:$("horn").checked,screenInverted:$("screenInverted").checked,
    cycleGames:$("cycleGames").checked,cycleSeconds:Number($("cycleSeconds").value),
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
  $("screenInverted").checked=!!currentConfig.screenInverted;
  $("cycleGames").checked=!!currentConfig.cycleGames;
  $("cycleSeconds").value=currentConfig.cycleSeconds||9;
  $("displayTimezone").value=String(currentConfig.timezone ?? 1);
  renderDevices();
}
document.querySelectorAll("[data-mode]").forEach(b=>b.addEventListener("click",()=>perform(async()=>{
  const mode=Number(b.dataset.mode);
  await post("/api/config",{mode});
  await refresh();message("Celebrations: "+modes[mode]);
})));
$("saveSettings").onclick=()=>perform(saveConfig);
$("saveDisplay").onclick=()=>perform(saveConfig);
$("saveDevices").onclick=()=>perform(saveConfig);
$("testHome").onclick=()=>perform(async()=>{await post("/api/test",{side:"home"});message("Home color test started")});
$("testAway").onclick=()=>perform(async()=>{await post("/api/test",{side:"away"});message("Away color test started")});
$("testLights").onclick=()=>perform(async()=>{await post("/api/test",{side:"home"});message("Test sent to enabled lights")});
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
