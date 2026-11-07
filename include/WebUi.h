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
<title>GlowDeck GameDay | Command Center</title>
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
/* Phase 7D – arena-inspired desktop/mobile command center.
   Shared team palette accent, broadcast scoreboard, clear setup/test separation. */
:root{--bg:#070e18;--panel:#101d2a;--panel2:#172b3b;--edge:#2b4052;--primary:#287a79;--active:#70d9e8;--text:#edf7fd;--muted:#a4b9c7;--amber:#f4c979;--shadow:0 12px 38px #020a14aa;--hud:#79d6eb}
body{background:radial-gradient(ellipse 840px 370px at 70% -100px,#173c4b 0%,transparent 73%),linear-gradient(180deg,#081420 0%,#070e18 50%,#091521 100%);font-family:system-ui,-apple-system,"Segoe UI",sans-serif}
header{padding:24px clamp(17px,4vw,48px);background:linear-gradient(103deg,#0b2633,#101825 65%,#0b2331);border-bottom:1px solid #365068;position:relative}
header:after{content:"";position:absolute;bottom:0;left:0;right:0;height:2px;background:linear-gradient(90deg,var(--active),transparent 55%,#f2b86d)}
.brandlock,.header-controls{display:flex;align-items:center;gap:15px}
.brandmark{width:54px;height:54px;display:flex;flex-direction:column;align-items:center;justify-content:center;line-height:.95;transform:skew(-8deg);border:2px solid #76d5e2;background:linear-gradient(145deg,#204357,#0b2331);border-radius:12px;box-shadow:0 0 22px #4db6bf22}
.brandmark span{font-size:19px;font-weight:950;letter-spacing:-.08em}
.brandmark span:last-child{color:#7bdbdf}
.brand-dot{font-size:9px;color:var(--amber);vertical-align:middle}
.header-copy{margin:1px 0;font-size:12px;color:#acc7d1}
.platform-tag{display:inline-flex;align-items:center;gap:7px;font-size:11px;font-weight:850;letter-spacing:.07em;border:1px solid #3a6572;background:#1b3946;padding:7px 11px;border-radius:99px;color:#beeef2;white-space:nowrap}
main{max-width:1360px;grid-template-columns:225px minmax(0,1fr)}
.tabs{background:linear-gradient(180deg,#10202d,#0b1b27);border:1px solid #314c5a;border-radius:16px}
.tab{font-weight:680;min-height:53px;gap:10px}
.tab.selected{background:linear-gradient(90deg,#234f61,#1c454e);border-color:#508b9a;box-shadow:inset 3px 0 #88e3ed}
.nav-glyph{font-size:19px;line-height:1;width:23px;color:#82bfc9;text-align:center;flex:none}
.tab.selected .nav-glyph{color:#fff}
.nav-hint{margin-left:auto;font-size:9px;font-weight:800;letter-spacing:.08em;opacity:.65}
.rail-footer{border-top:1px solid var(--edge);margin-top:7px;padding-top:15px}
.card{background:linear-gradient(156deg,#142a3a,#0d1d2b 75%);border-color:#2c4657;border-radius:15px;box-shadow:0 7px 24px #0004}
.card.hero{background:linear-gradient(117deg,#153449,#102836 53%,#0d1c29);border-color:#46768a;position:relative;overflow:hidden}
.card.hero:after{content:"";position:absolute;pointer-events:none;inset:0;background:repeating-linear-gradient(0deg,transparent 0px,transparent 31px,#83c9de09 32px),linear-gradient(135deg,#78d4e40c,transparent 55%);opacity:.9}
.card.hero>*{position:relative;z-index:1}
.card.hero .kicker{color:#8ad8ed}
.card h2{color:#f5faff}
.stat{background:#0b1c2c;border-color:#274353}
.stat b{color:#f2f9ff;font-variant-numeric:tabular-nums}
.stat span{color:#9bb5c3}
button{border-radius:9px;background:#1d3545;border-color:#416172}
button:hover{background:#28485d;border-color:#74cede}
button.primary,button.selected{background:linear-gradient(145deg,#246f75,#1c5a69);border-color:#64b5c1}
input,select{background:#0b1c2a;border-color:#385b6c;border-radius:9px}
:focus-visible{outline:3px solid #8ce9ef;outline-offset:3px}
.section-intro{padding-top:8px}
.section-intro h2{font-weight:850;letter-spacing:-.035em}
.section-intro p{max-width:810px}
.subnav{display:flex;flex-wrap:wrap;gap:8px;padding:11px;background:#0b1a28;border:1px solid var(--edge);border-radius:12px;margin:0 0 22px}
.subnav button{min-height:38px;padding:7px 12px;background:transparent;border:1px solid transparent;font-size:12px;color:#b8d5dd}
.subnav button:hover,.subnav button.active{background:#224151;border-color:#426778;color:#f1fdff}
.portal-zone{display:none;scroll-margin-top:20px;margin:27px 0 32px}
.portal-zone.active-zone{display:block}
.zone-heading{display:flex;align-items:center;gap:14px;padding:0 0 13px;border-bottom:1px solid #34566a;margin:0 0 16px}
.zone-heading h3{margin:0 0 2px;font-size:19px;letter-spacing:-.02em}
.zone-heading p{margin:0;font-size:13px}
.zone-number{flex:none;display:grid;place-items:center;width:39px;height:39px;border:1px solid #467d8b;background:#193649;color:#86dce7;border-radius:10px;font-weight:900;font-size:15px}
.zone-heading .compact-link{margin-left:auto;min-height:36px;font-size:12px;white-space:nowrap}
.lab-notice{display:flex;align-items:flex-start;gap:13px;padding:17px 20px;margin:0 0 19px;background:#122b3b;border:1px solid #437083;border-radius:13px}
.lab-notice p{margin:5px 0 0;font-size:13px}
.lab-status-dot{flex:none;width:12px;height:12px;background:#f5c56a;border:3px solid #624d2d;border-radius:50%;margin-top:6px}
.lab-card{min-height:88px}
.save-dock{position:sticky;bottom:12px;z-index:4;display:flex;align-items:center;justify-content:space-between;gap:15px;background:#142b3aed;backdrop-filter:blur(16px);border:1px solid #46788b;padding:12px 17px;border-radius:13px;box-shadow:0 12px 38px #0009}
.save-indicator{color:#bdd3df;font-size:12px}
.save-indicator.is-dirty{color:#fbd18b;font-weight:800}
.legacy-save{display:none!important}
.lab-card .actions{margin:7px 0}
.lab-card button{min-height:40px}
#audioLabProbe{white-space:pre-wrap}
#lab-hardware .portal-zone{margin:0}
#lab-hardware .card{margin-bottom:12px}
.portal-zone [data-platform="s3"][hidden]{display:none!important}
#customize .hint,#device .hint{border-left-color:#5c9caa}
@media(max-width:850px){
 .tabs{background:#091a27fa}
 .tab{min-height:46px}
 .tab .nav-hint{display:none}
 .nav-glyph{font-size:16px;width:18px}
 .portal-zone{scroll-margin-top:88px}
 .save-dock{bottom:9px}
}
@media(max-width:650px){
 header{padding:16px}
 .top{align-items:flex-start;gap:10px}
 .brandmark{width:39px;height:43px;min-width:39px}
 .brandmark span{font-size:15px}
 .brandlock{gap:9px}
 .header-copy{display:none}
 .header-controls{flex-direction:column;align-items:flex-end;gap:4px}
 .platform-tag{font-size:9px;padding:4px 7px}
 .status{font-size:10px;padding:5px 9px}
 .zone-heading{flex-wrap:wrap;gap:9px}
 .zone-heading .compact-link{margin-left:48px}
 .subnav{overflow-x:auto;flex-wrap:nowrap;scrollbar-width:thin}
 .subnav button{white-space:nowrap}
 .save-dock{padding:10px 12px}
 .save-dock button{white-space:nowrap}
}

/* Broadcast-scoreboard language: home/away hierarchy and live chips
   echo the 480x320 native S3 arena UI without loading remote images. */
.arena-scoreboard{margin:16px 0 18px;border:1px solid #45657a;border-radius:14px;background:radial-gradient(ellipse at 50% 45%,#204157 0%,#102334 62%,#0a1b2b);overflow:hidden}
.arena-topline,.arena-footer{display:flex;align-items:center;justify-content:space-between;gap:12px;padding:9px 15px;background:#0c1a2a9c;color:#9ebdcc;font-size:10px;font-weight:800;letter-spacing:.12em}
.arena-topline{border-bottom:1px solid #304d62}.arena-footer{border-top:1px solid #304d62}
.arena-state{padding:4px 9px;border:1px solid #477488;border-radius:5px;color:#d6ebf1}
.arena-state.is-live{color:#ffd5b6;background:#5b332555;border-color:#9a533d}
.arena-teams{display:grid;grid-template-columns:minmax(0,1fr) minmax(100px,auto) minmax(0,1fr);align-items:center;gap:12px;padding:19px 20px 23px}
.arena-team{display:flex;align-items:center;gap:12px;min-width:0}
.arena-team.home{flex-direction:row-reverse;text-align:right}
.arena-mark{--team-accent:#78cbd9;width:62px;height:62px;flex:none;display:grid;place-items:center;border-radius:11px;border:1px solid var(--team-accent);font-size:17px;font-weight:950;letter-spacing:-.035em;color:#fff;background:linear-gradient(145deg,var(--team-accent),#15293a 91%);box-shadow:0 0 18px #0004}
.arena-team-label{font-size:10px;font-weight:850;letter-spacing:.13em;color:#a2c0d0}
.arena-team-label strong{display:block;font-size:21px;font-weight:900;letter-spacing:.02em;color:#fff}
.arena-score{font-size:clamp(31px,5vw,50px);font-weight:950;letter-spacing:-.09em;text-align:center;white-space:nowrap;font-variant-numeric:tabular-nums;color:#f3fafc;text-shadow:0 0 20px #9edfe62d}
.tab.has-dirty:after{content:"";width:7px;height:7px;background:#fbd18b;border-radius:50%;margin-left:4px}
.arena-workflow{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:7px;margin-top:14px}
.arena-workflow>div{padding:9px 10px;background:#071925bb;border:1px solid #345367;border-radius:8px;display:flex;flex-direction:column;gap:2px}
.arena-workflow b{font-size:11px;letter-spacing:.06em;color:#83dddf}
.arena-workflow span{font-size:11px;color:#b2cad3}
@media(max-width:650px){.arena-workflow{grid-template-columns:1fr}.arena-workflow>div{flex-direction:row;gap:12px}}
@media(max-width:650px){
 .arena-teams{grid-template-columns:minmax(0,1fr) auto minmax(0,1fr);gap:5px;padding:17px 10px}
 .arena-team{flex-direction:column;gap:5px;text-align:center}
 .arena-team.home{flex-direction:column;text-align:center}
 .arena-mark{width:43px;height:43px;font-size:12px}
 .arena-team-label strong{font-size:17px}
 .arena-score{font-size:29px;letter-spacing:-.09em}
 .arena-topline,.arena-footer{font-size:9px;padding:8px 10px}
}

/* 8B segment maps: compact arena-tinted zone picker, no external scripts. */
.wled-zones{margin-top:10px}
.wled-zones>h4{margin:12px 0 5px;font-size:16px}
.wled-segment-row{display:grid;grid-template-columns:minmax(0,1fr) auto;gap:12px;align-items:center;
  padding:12px;margin:8px 0;border:1px solid #365669;border-radius:10px;background:#102838}
.wled-segment-row strong{font-size:13px}
.wled-segment-row p{margin:4px 0 7px}
.wled-segment-row select{min-width:126px}
.wled-segment-row[data-zone="1"]{border-left:4px solid #6cd1d8}
.wled-segment-row[data-zone="2"]{border-left:4px solid #edb571}
.wled-segment-row[data-zone="3"]{border-left:4px solid #c6d0e9}
.wled-range-bar{height:6px;border-radius:3px;background:#0a1924;border:1px solid #335063;overflow:hidden}
.wled-range-fill{height:100%;background:#668797}
.wled-segment-row[data-zone="1"] .wled-range-fill{background:#6cd1d8}
.wled-segment-row[data-zone="2"] .wled-range-fill{background:#edb571}
.wled-segment-row[data-zone="3"] .wled-range-fill{background:#c6d0e9}
@media(max-width:620px){.wled-segment-row{grid-template-columns:1fr}.wled-segment-row .actions{justify-content:space-between}}
/* Native WLED JSON scene composer — arena-blue surfaces and zone accents. */
.wled-scene-builder{margin-top:12px;padding:16px;border:1px solid #46768b;
  border-radius:13px;background:linear-gradient(150deg,#112d3b,#0b1d29)}
.wled-scene-title{display:flex;align-items:center;justify-content:space-between;gap:10px;flex-wrap:wrap}
.wled-scene-title h4{margin:0 0 4px;font-size:17px}
.wled-scene-title strong{font-size:15px}
.wled-scene-warning{padding:12px;border:1px solid #aa8455;
  background:#4f3d2755;border-radius:10px;color:#f2d6a2}
.wled-scene-toolbar{display:flex;align-items:flex-end;gap:12px;flex-wrap:wrap;
  padding:12px 0;border-bottom:1px solid #34586b}
.wled-scene-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(225px,1fr));
  gap:12px;margin:14px 0}
.wled-scene-zone{padding:13px;border:1px solid #426578;border-radius:12px;
  background:#102534}
.wled-scene-zone[data-zone="1"]{border-top:3px solid #62c9d6}
.wled-scene-zone[data-zone="2"]{border-top:3px solid #dfab64}
.wled-scene-zone[data-zone="3"]{border-top:3px solid #d5dced}
.wled-scene-zone .toggle{margin:10px 0}
.wled-scene-controls{display:flex;flex-direction:column;gap:10px}
.wled-scene-controls[hidden]{display:none}
.wled-scene-field{display:flex;flex-direction:column;gap:4px;
  min-width:155px;font-size:12px;font-weight:650;color:#bad4dd}
.wled-scene-field input[type=range]{width:100%;min-height:18px}
.wled-scene-field b{align-self:flex-end;font-variant-numeric:tabular-nums}
.wled-scene-field select{width:100%}
.wled-scene-field input[type=color]{width:100%;height:40px;padding:3px}
@media(max-width:620px){.wled-scene-builder{padding:10px}
  .wled-scene-grid{grid-template-columns:minmax(0,1fr)}}</style>
</head>
<body>
<header>
<div class="top">
<div class="brandlock"><div class="brandmark" aria-hidden="true"><span>G</span><span>D</span></div>
<div><div class="eyebrow">GLOWDECK <span class="brand-dot">◆</span> GAMEDAY</div>
<h1>Command Center</h1><p class="header-copy">Broadcast control for every puck drop, goal and celebration.</p></div></div>
<div class="header-controls"><span class="platform-tag" id="platformTag">DETECTING DEVICE</span><div id="connection" class="status">Connecting</div></div>
</div>
</header>
<main>
<nav class="tabs" aria-label="GameDay navigation">
<div class="rail-note"><span class="kicker">GAME DAY CONTROL</span><p class="small">Your local command station</p></div>
<button class="tab selected" data-tab="dashboard" aria-current="page"><span class="nav-glyph" aria-hidden="true">▦</span><span>Overview</span></button>
<button class="tab" data-tab="games"><span class="nav-glyph" aria-hidden="true">◉</span><span>Live games</span></button>
<button class="tab" data-tab="customize"><span class="nav-glyph" aria-hidden="true">◈</span><span>Customize</span><span class="nav-hint">SETUP</span></button>
<button class="tab" data-tab="lab"><span class="nav-glyph" aria-hidden="true">⚡</span><span>Test Lab</span><span class="nav-hint">PREVIEW</span></button>
<button class="tab" data-tab="device"><span class="nav-glyph" aria-hidden="true">⚙</span><span>Device</span></button>
<button class="tab" data-tab="events"><span class="nav-glyph" aria-hidden="true">≡</span><span>Activity</span></button>
<div class="rail-note rail-footer"><span class="kicker">LOCAL DEVICE</span><p class="small">Changes stay on this GameDay controller. No cloud account required.</p></div>
</nav>
<section id="dashboard" class="panel active">
<div class="section-intro"><div class="eyebrow">THE ARENA AT A GLANCE</div><h2>Your game day, under control</h2><p>See what’s happening, choose the game driving your lights and launch straight into configuration or testing.</p></div>
<div class="card hero"><div class="kicker">NHL BROADCAST CENTER</div>
<h2>Today's arena</h2>
<div class="arena-scoreboard" aria-label="Featured NHL game">
<div class="arena-topline"><span id="heroLiveTag" class="arena-state">LOADING</span><span id="heroGameMode">GLOWDECK · GAME CENTER</span></div>
<div class="arena-teams">
<div class="arena-team"><div class="arena-mark" id="heroAwayMark">AWY</div>
<div class="arena-team-label">AWAY <strong id="heroAway">—</strong></div></div>
<div class="arena-score" id="heroScore" aria-live="polite">— : —</div>
<div class="arena-team home"><div class="arena-mark" id="heroHomeMark">HME</div>
<div class="arena-team-label">HOME <strong id="heroHome">—</strong></div></div>
</div>
<div class="arena-footer"><span id="heroStatus">Connecting to the NHL feed…</span><span id="heroArmStatus">NO GAME ARMED</span></div>
</div>
<div class="stats">
<div class="stat"><span>Games live</span><b id="dashLiveCount">—</b></div>
<div class="stat"><span>Favorite teams</span><b id="dashFavoriteCount">—</b></div>
<div class="stat"><span>Games followed</span><b id="dashFollowCount">—</b></div>
<div class="stat"><span>On-screen alerts</span><b id="dashDisplayMode">—</b></div>
</div>
<div class="actions"><button class="primary" data-tab="games">Browse games</button>
<button data-tab="customize" data-anchor="zone-favorites">Customize</button>
<button data-tab="lab" data-anchor="lab-full">Run a test</button></div>
<div class="arena-workflow"><div><b>01 · FOLLOW</b><span>Star any NHL game to track it</span></div><div><b>02 · ARM</b><span>One game controls WLED scoring</span></div><div><b>03 · TEST</b><span>Preview safely in Test Lab</span></div></div>
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
<p>The armed matchup controls WLED. Following (★) another game adds its screen and eligible on-device LED alerts without changing the WLED target.</p>
<div class="actions" id="modeButtons">
<button data-mode="0">Off</button><button data-mode="1">Home</button><button data-mode="2">Away</button><button data-mode="3">Both</button>
</div>
<div class="actions"><button id="testHome">Test home color</button><button id="testAway">Test away color</button></div>
<p class="small">A test saves the current WLED state, shows the celebration color, then restores the previous state.</p></div>
</section>
<section id="events" class="panel">
<div class="section-intro"><div class="eyebrow">ACTIVITY LOG</div><h2>What happened, and what fired</h2><p>League observations and the exact commands sent to your physical lights.</p></div>
<div class="card"><h2>Game events & light activity</h2>
<p>Review NHL goals, power plays, puck drops and finals. Check the on-device LED (where supported) and each opted-in WLED controller. This is an event audit, not a Test Lab.</p>
<p class="small muted">WLED “Command accepted” means the controller acknowledged the HTTP request. It does not independently confirm visible light output. The latest 64 events remain in memory <b>until the device reboots</b>. No historical events are backfilled.</p>
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
<div class="section-intro"><div class="eyebrow">NHL LIVE BOARD</div><h2>Every matchup, one place</h2><p>Tap ☆ to follow any game for rear RGB and screen alerts, even before puck drop. Follow the entire daily schedule if you like. Arm chooses which single game controls WLED.</p></div>
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
<p>During live NHL games, the GameDay display automatically focuses on full-screen live scoreboards and groups other upcoming or finished matches into readable four-game summary slides. When no games are live, it returns to the complete game-and-standings carousel. Swipe to browse or tap a summary row for its matchup details.</p>
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
<p>Choose how widely the GameDay screen reports live action. The screen can show events for every game while the rear LED and WLED continue following their independently selected matchups.</p>
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
<p class="small muted">Screen previews run locally and do not trigger WLED or the speaker.</p>
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
<div class="card"><h2>Discover &amp; connect WLED devices</h2>
<p>Find WLED controllers on this Wi-Fi network, verify their identity and choose which ones participate. Up to eight saved devices. Existing configured devices are retained; discovered devices are never enabled automatically.</p>
<div class="actions">
<button type="button" id="wledScan">Scan Wi-Fi for WLED</button>
<input type="text" id="wledManualIp" inputmode="decimal" placeholder="Manual IPv4 · 192.168.1.71" aria-label="Manual WLED local IP address">
<button type="button" id="wledManualCheck">Verify IP</button>
</div>
<p class="small muted" id="wledScanStatus" role="status">No discovery run yet. WLED must be on the same Wi-Fi subnet; manual entry remains available.</p>
<div id="wledDiscovered" aria-live="polite"></div>
</div>
<div class="card"><h2>Saved WLED controllers</h2>
<p>Assign a friendly name, adjust the IP, enable the controller and choose which NHL events it receives. Discovered controllers are OFF until you opt in and save.</p>
<div id="deviceList"></div>
<div class="actions"><button class="primary" id="saveDevices">Save celebration settings</button></div>
<p class="small muted">A controller without a zone map keeps the existing whole-controller celebration behavior. Use <strong>Manage segments</strong> to identify its physical LED ranges and opt into Home, Away or Neutral zones. Unassigned segments are never changed by mapped celebrations. DDP is reserved for Phase 8D.</p>
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
<div class="hint">The armed game is automatically followed. You can also star every other game on today's NHL schedule (up to 20 total games). Follow settings reset when the NHL date changes. Starring does not arm WLED.</div>
</div>
<div class="card"><h2>Follow today's NHL games</h2>
<p>Tap ★ on the GameDay scoreboard, game details, or the device's game list—or select games below. Following enables eligible on-device LED and priority screen celebrations independently of the armed WLED game.</p>
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
<p class="small muted">Run the selected WLED controllers and screen overlay together. The CYD rear RGB participates when enabled; Waveshare S3 case LEDs use their separate hardware test. No manual preview creates a genuine NHL goal.</p>
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
<div class="card"><h2>Wi-Fi</h2><p>Connect this GameDay device to the Wi-Fi network used by your WLED controllers.</p>
<label for="ssid">Network name (SSID)</label><input id="ssid" autocomplete="off" maxlength="32" placeholder="Home Wi-Fi">
<div class="actions"><button id="scanWifi">Find nearby networks</button></div>
<select id="networks" aria-label="Found Wi-Fi networks"><option value="">Select a scanned network</option></select>
<label for="wifiPassword">Wi-Fi password</label><input id="wifiPassword" type="password" autocomplete="new-password" placeholder="Leave blank to keep existing">
<p class="small muted">The GlowDeck setup hotspot remains available at 192.168.4.1.</p></div>
<div class="card"><h2>Celebration preferences</h2>
<label for="brightness">WLED brightness</label><input id="brightness" type="range" min="1" max="255"><div class="small" id="brightValue"></div>
<label for="seconds">Celebration duration (seconds)</label><input id="seconds" type="number" min="2" max="30" step="1" value="8">
<div class="toggle" style="margin-top:18px"><input id="horn" type="checkbox"><label for="horn" id="hornLabel">Enable built-in horn (GPIO26 speaker)</label></div>
<p class="small muted" id="hornHelp">The onboard horn is a simple tone sequence. Custom WAV playback is not yet included.</p>
<div class="toggle" style="margin-top:18px"><input id="screenInverted" type="checkbox"><label for="screenInverted">Correct inverted CYD display colors</label></div>
<p class="small muted">Enable if the screen is pale blue or buttons appear pink instead of dark teal. It can also be toggled under SETUP directly on the touchscreen.</p>
<label for="adminPassword">Change home Wi-Fi dashboard password</label><input id="adminPassword" type="password" autocomplete="new-password" placeholder="Leave blank to keep existing">
<p class="small muted">Connected to GlowDeck's setup hotspot? The dashboard opens without an extra login. Over your home Wi-Fi, sign in as admin with the dashboard password. Enter at least 8 characters to change it. The hotspot itself remains Wi-Fi password protected.</p>
</div></div>
<div class="card" id="s3AudioCard" hidden style="display:none">
<h2>Onboard speaker · ESP32-S3</h2>
<p>Improved, harmonically richer goal horn for the small Waveshare speaker. Adjust ES8311 output level from 30% to 80%; start at 70%, then tune for clear sound without distortion. Test sounds never trigger WLED, game events or screen takeovers.</p>
<p id="s3AudioStatus" class="small muted">Checking speaker...</p>
<label for="s3AudioVolume">Speaker volume · <b id="s3AudioVolumeValue">70%</b></label>
<div class="actions"><input type="range" id="s3AudioVolume" min="30" max="80" step="5" value="70" style="flex:1;min-width:180px">
<button type="button" id="s3AudioVolumeApply">Apply volume</button></div>
<div class="actions"><button type="button" id="s3AudioPreview">Preview goal horn</button>
<button type="button" id="s3AudioTone">Test speaker · 1 kHz</button>
<button type="button" id="s3AudioProbe">Check audio diagnostics</button></div>
<p class="small muted">The new horn contains brighter upper harmonics and a stronger digital signal. Both previews use your saved volume, while the device amplifier is off when idle.</p>
<pre id="s3AudioDiagnostics" class="small muted" style="white-space:pre-wrap;word-break:break-word">No diagnostic check run yet.</pre>
<p class="small muted">The checkbox above still controls audio for real followed/armed NHL goals. Previews never trigger lights or game events.</p>
<div class="divider" style="margin-top:20px"></div>
<h3>Goal horn sound library</h3>
<p class="small muted">Keep the built-in horn or install up to three custom WAV sounds. Select one sound for all followed/armed goal celebrations. Team-specific assignments are planned for Phase 7B.3.</p>
<p id="s3SoundLibraryStatus" class="small muted">Loading audio library...</p>
<div class="actions"><button type="button" id="s3SoundBuiltInUse">Use built-in horn</button>
<button type="button" id="s3SoundBuiltInPreview">Preview built-in</button></div>
<div id="s3SoundLibrarySlots" style="margin:12px 0"></div>
<form id="s3SoundUploadForm">
<label for="s3SoundUploadSlot">Upload into library slot</label>
<select id="s3SoundUploadSlot" required>
<option value="1">Slot 1</option><option value="2">Slot 2</option><option value="3">Slot 3</option>
</select>
<label for="s3SoundUploadFile">Custom WAV file (max 768 KB)</label>
<input id="s3SoundUploadFile" type="file" name="sound" accept=".wav,audio/wav,audio/x-wav" required>
<p class="small muted">PCM 16-bit WAV · mono/stereo · 8–48 kHz · up to 10 seconds. Files remain on the device through normal firmware OTA updates. Uploading replaces only the chosen slot.</p>
<div class="actions"><button type="submit" class="primary" id="s3SoundUploadButton">Upload WAV</button></div>
</form>
</div>
<div class="card" id="s3CaseLedCard" hidden style="display:none">
<h2>Case lighting · Waveshare S3 · Phase 7C</h2>
<p><strong>Not wired or hardware-tested yet.</strong> Both GPIO outputs remain disabled until you explicitly confirm completed wiring and enable this feature. HOME is the left side (GPIO17, header 14); AWAY is the right side (GPIO18, header 16).</p>
<p class="small muted">Requires independent 5 V WS2812B/NeoPixel strips, a 74AHCT125 3.3 V-to-5 V level shifter, shared ground, signal resistors and a fused external 5 V supply. Do not power strips from S3 GPIO or its 3.3 V pins.</p>
<p id="s3CaseLedStatus" class="small muted">Case LED wiring not yet validated</p>
<div class="toggle"><input type="checkbox" id="s3CaseEnabled">
<label for="s3CaseEnabled">Enable LEFT/HOME and RIGHT/AWAY external LED data pins after wiring</label></div>
<label for="s3CasePixels">LED count per side (1–16)</label>
<input id="s3CasePixels" type="number" min="1" max="16" step="1" value="8">
<label for="s3CaseBrightness">LED brightness · <strong id="s3CaseBrightValue">96 / 128</strong></label>
<input id="s3CaseBrightness" type="range" min="1" max="128" step="1" value="96">
<label for="s3CaseSeconds">Goal celebration duration (seconds)</label>
<input id="s3CaseSeconds" type="number" min="2" max="12" step="1" value="6">
<div class="toggle"><input id="s3CaseGoal" type="checkbox" checked><label for="s3CaseGoal">Followed/armed goals · team color, strobe and chase</label></div>
<div class="toggle"><input id="s3CasePP" type="checkbox" checked><label for="s3CasePP">Verified power plays · team color pulse</label></div>
<div class="toggle"><input id="s3CaseStart" type="checkbox" checked><label for="s3CaseStart">Armed game start · both team colors</label></div>
<div class="toggle"><input id="s3CaseFinal" type="checkbox" checked><label for="s3CaseFinal">Armed game final · winning team color</label></div>
<div class="actions"><button type="button" id="s3CaseSave" class="primary">Save case lighting configuration</button></div>
<p class="small muted">Isolated diagnostic previews only change physical case LEDs, not the NHL event log, WLED, goal GIF or speaker. After wiring, use RED / GREEN / BLUE to verify channel order before testing live team colors.</p>
<div class="actions">
<button type="button" class="s3-case-test" data-case-mode="goal" data-case-side="home">Test HOME / left goal</button>
<button type="button" class="s3-case-test" data-case-mode="goal" data-case-side="away">Test AWAY / right goal</button>
<button type="button" class="s3-case-test" data-case-mode="pp" data-case-side="home">Test left power play</button>
<button type="button" class="s3-case-test" data-case-mode="pp" data-case-side="away">Test right power play</button>
<button type="button" class="s3-case-test" data-case-mode="red" data-case-side="home">Left RED</button>
<button type="button" class="s3-case-test" data-case-mode="green" data-case-side="home">Left GREEN</button>
<button type="button" class="s3-case-test" data-case-mode="blue" data-case-side="home">Left BLUE</button>
<button type="button" class="s3-case-test" data-case-mode="red" data-case-side="away">Right RED</button>
<button type="button" class="s3-case-test" data-case-mode="green" data-case-side="away">Right GREEN</button>
<button type="button" class="s3-case-test" data-case-mode="blue" data-case-side="away">Right BLUE</button>
<button type="button" id="s3CaseOff">All case LEDs OFF</button>
</div>
<p class="small muted">See <a href="https://github.com/ggrant111/glowdeck-gameday/blob/main/docs/S3_PHASE7C_CASE_LEDS.md" target="_blank" rel="noopener noreferrer">Phase 7C wiring and acceptance guide</a>.</p>
</div>
<div class="card" id="s3GoalGifCard" hidden style="display:none">
<h2>Hockey goal GIF · ESP32-S3</h2>
<p>Play your hockey goal-and-siren animation whenever a real NHL goal or GOAL preview appears on the touchscreen. Team-colored borders, score, and WLED celebrations stay synchronized. The GIF itself is silent; the separate ES8311 onboard horn can play during an armed game's real goal when enabled.</p>
<p id="goalGifStatus" class="small muted">Checking animation storage...</p>
<form id="goalGifUploadForm">
<label for="goalGifFile">Upload optimized 320×240 GIF (maximum 1 MB)</label>
<input type="file" name="goal" id="goalGifFile" accept=".gif,image/gif" required>
<div class="actions"><button class="primary" type="submit">Install goal GIF</button>
<button type="button" id="goalGifPreview">Preview GOAL on device</button>
<button type="button" id="goalGifRemove">Remove animation</button></div></form>
<p class="small muted">The animation is stored in device flash and survives firmware updates, unless you erase flash or reformat its filesystem. Applies to the S3 only; existing CYD overlays are unchanged.</p>
</div>
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
<p>Upload a compiled PlatformIO <code>firmware.bin</code> from this project. Keep the controller powered until it finishes and reboots.</p>
<form id="uploadForm"><label for="bin">Compiled firmware (.bin)</label><input id="bin" name="firmware" type="file" accept=".bin" required>
<div class="actions"><button class="primary" type="submit">Upload and reboot</button></div></form>
<p class="small muted">Initial installation still requires USB. This update control works only when you use the project's OTA-capable partition layout.</p></div>
<div class="card"><h2>Troubleshooting</h2><p>For boot loops, first flash the basic diagnostic firmware in the documentation. Use the device's setup diagnostics if touch stops responding. Watch serial output at 115200 baud.</p>
<div class="actions"><button id="reboot">Restart device</button></div></div>
</section>
<section id="customize" class="panel" aria-labelledby="customizeTitle">
<div class="section-intro"><div class="eyebrow">MAKE GAMEDAY YOURS</div>
<h2 id="customizeTitle">Customize the experience</h2>
<p>Change one subsystem at a time: favorites, screen, WLED, sound or local LEDs. Live diagnostics and celebration rehearsals are in Test Lab; custom sound auditioning stays beside the WAV library.</p></div>
<div class="subnav" aria-label="Customize sections">
<button type="button" data-jump="zone-favorites">Favorites</button>
<button type="button" data-jump="zone-screen">Screen</button>
<button type="button" data-jump="zone-wled">WLED</button>
<button type="button" data-jump="zone-audio">Sound</button>
<button type="button" data-jump="zone-hardware">On-device LEDs</button>
</div>
<div id="zone-favorites" class="portal-zone"><div class="zone-heading"><span class="zone-number">01</span><div><h3>Favorites &amp; following</h3><p>Choose priority teams and games without changing the armed WLED matchup.</p></div></div><div id="mount-favorites"></div></div>
<div id="zone-screen" class="portal-zone"><div class="zone-heading"><span class="zone-number">02</span><div><h3>Scoreboard &amp; alerts</h3><p>Rotation, team logos, goal screen and game-day display preferences.</p></div><button type="button" class="ghost compact-link" data-tab="lab" data-anchor="lab-screen">Screen tests</button></div><div id="mount-screen"></div></div>
<div id="zone-wled" class="portal-zone"><div class="zone-heading"><span class="zone-number">03</span><div><h3>WLED celebrations</h3><p>Configure controllers, effects and event permissions.</p></div><button type="button" class="ghost compact-link" data-tab="lab" data-anchor="lab-wled">Test WLED</button></div><div id="mount-wled"></div></div>
<div id="zone-audio" class="portal-zone"><div class="zone-heading"><span class="zone-number">04</span><div><h3>Sound &amp; horns</h3><p>Enable the onboard horn, manage custom WAV sounds and adjust output level.</p></div><button type="button" class="ghost compact-link" data-tab="lab" data-anchor="lab-audio">Audio tests</button></div><div id="mount-audio"></div></div>
<div id="zone-hardware" class="portal-zone"><div class="zone-heading"><span class="zone-number">05</span><div><h3>On-device LED hardware</h3><p>CYD rear RGB or Waveshare S3 left/home and right/away case lighting.</p></div><button type="button" class="ghost compact-link" data-tab="lab" data-anchor="lab-hardware">LED tests</button></div><div id="mount-hardware"></div></div>
<div class="save-dock" role="region" aria-label="Save shared settings"><span class="save-indicator" id="customizeSaveState">Display, WLED, horn and CYD options save together. Volume, WAV library, favorites and case LEDs have their own Apply controls.</span><button type="button" class="primary" id="portalSave">Save settings</button></div>
</section>
<section id="lab" class="panel" aria-labelledby="labTitle">
<div class="section-intro"><div class="eyebrow">SAFE PREVIEW &amp; DIAGNOSTICS</div>
<h2 id="labTitle">Test Lab</h2>
<p>One place to verify colors, sounds, screen events and connected lights. These are manual tests; they do not create an actual NHL score.</p></div>
<div class="lab-notice"><span class="lab-status-dot" aria-hidden="true"></span><div><strong>Know what you're testing</strong><p>Screen and speaker previews are local. WLED previews send real commands to selected devices and restore previous states when supported. External case lights only respond after you enable their wiring.</p></div></div>
<div class="subnav" aria-label="Test Lab sections">
<button type="button" data-jump="lab-screen">Screen</button>
<button type="button" data-jump="lab-audio">Audio</button>
<button type="button" data-jump="lab-wled">WLED</button>
<button type="button" data-jump="lab-hardware">LED hardware</button>
<button type="button" data-jump="lab-full">Full celebration</button>
</div>
<div id="lab-screen" class="portal-zone"><div class="zone-heading"><span class="zone-number">01</span><div><h3>Screen &amp; scoreboard</h3><p>Preview NHL goal, power-play and final overlays without switching games.</p></div></div><div class="card lab-card" id="mount-lab-screen"></div></div>
<div id="lab-audio" class="portal-zone" data-platform="s3"><div class="zone-heading"><span class="zone-number">02</span><div><h3>Speaker &amp; sound</h3><p>Hear the selected horn, verify the test tone, inspect ES8311 diagnostics.</p></div></div><div class="card lab-card" id="mount-lab-audio"></div></div>
<div id="lab-wled" class="portal-zone"><div class="zone-heading"><span class="zone-number">03</span><div><h3>WLED controller tests</h3><p>Real color/effect commands are sent to opted-in controllers on your local network.</p></div></div><div id="mount-lab-wled"></div></div>
<div id="lab-hardware" class="portal-zone"><div class="zone-heading"><span class="zone-number">04</span><div><h3>LED hardware channels</h3><p>Confirm individual outputs and team colors before live events.</p></div></div><div id="mount-lab-hardware"></div><div id="mount-lab-case" data-platform="s3"></div></div>
<div id="lab-full" class="portal-zone"><div class="zone-heading"><span class="zone-number">05</span><div><h3>Full celebration rehearsal</h3><p>Check the combined celebration, then restore devices to their prior state.</p></div></div><div id="mount-lab-full"></div></div>
</section>
<section id="device" class="panel" aria-labelledby="deviceTitle">
<div class="section-intro"><div class="eyebrow">SYSTEM &amp; MAINTENANCE</div>
<h2 id="deviceTitle">Device settings</h2>
<p>Wi-Fi, dashboard access, firmware, connection information and diagnostics. Your game-day effects live under Customize.</p></div>
<div class="subnav" aria-label="Device sections"><button type="button" data-jump="zone-network">Network &amp; access</button><button type="button" data-jump="zone-maintenance">Firmware &amp; maintenance</button><button type="button" data-jump="zone-connection">Device information</button></div>
<div id="zone-network" class="portal-zone"><div class="zone-heading"><span class="zone-number">01</span><div><h3>Network &amp; access</h3><p>Connect the device to your Wi-Fi and manage the local dashboard login.</p></div></div><div id="mount-network"></div></div>
<div id="zone-maintenance" class="portal-zone"><div class="zone-heading"><span class="zone-number">02</span><div><h3>Firmware &amp; maintenance</h3><p>Install verified S3 or CYD firmware for your specific board and restart safely.</p></div></div><div id="mount-maintenance"></div></div>
<div id="zone-connection" class="portal-zone"><div class="zone-heading"><span class="zone-number">03</span><div><h3>Connection information</h3><p>Useful identifiers and local network diagnostics.</p></div></div><div id="mount-connection"></div></div>
<div class="save-dock" role="region" aria-label="Save device settings"><span class="save-indicator" id="deviceSaveState">Network and access changes need to be saved</span><button type="button" class="primary" id="deviceSave">Save device settings</button></div>
</section>
</main>
<div id="notice" role="status" aria-live="polite"></div>
<script>
"use strict";
const $ = id => document.getElementById(id);

// Declarative portal layout. Reuse the original control nodes (and their IDs)
// instead of cloning buttons or duplicating API handlers. Future modules only
// need one mapping here and a destination in the accessible page shell.
function mountPortalModules(){
  const place=(id,destination)=>{
    const node=$(id), slot=$(destination);
    if(!node||!slot)throw new Error("Portal module missing: "+id+" → "+destination);
    slot.appendChild(node);
    return node;
  };
  const moveCard=(controlId,destination)=>{
    const source=$(controlId);
    if(!source)throw new Error("Missing existing control: "+controlId);
    const card=source.closest(".card");
    if(!card)throw new Error("Missing card for: "+controlId);
    return place(card.id || (card.id="portal-card-"+controlId),destination);
  };
  const newCard=(heading,lead,destination)=>{
    const card=document.createElement("div");
    card.className="card";
    const title=document.createElement("h2");title.textContent=heading;
    const intro=document.createElement("p");intro.textContent=lead;
    card.append(title,intro);
    $(destination).append(card);
    return card;
  };
  const transferRange=(first,last,dest)=>{
    let n=first;
    if(!n||!last)throw new Error("Portal setting group missing");
    while(n){
      const next=n.nextElementSibling;
      dest.appendChild(n);
      if(n===last)return;
      n=next;
    }
    throw new Error("Portal setting group boundary not found");
  };

  // GAME PLAN / VISUALS: move originals, keeping every API binding intact.
  moveCard("favoriteTeamGrid","mount-favorites");
  moveCard("rearFollowedList","mount-favorites");
  moveCard("cycleGames","mount-screen");
  moveCard("overlayEnabled","mount-screen");
  place("s3GoalGifCard","mount-screen");

  // CELEBRATIONS: preferences are not simulations.
  moveCard("wledGoalStyle","mount-wled");
  moveCard("deviceList","mount-wled");
  place("s3AudioCard","mount-audio");
  moveCard("rearEnabled","mount-hardware");
  place("s3CaseLedCard","mount-hardware");

  // SYSTEM: network, maintenance and connection details, nothing celebratory.
  moveCard("ssid","mount-network");
  moveCard("bin","mount-maintenance");
  moveCard("reboot","mount-maintenance");
  moveCard("apName","mount-connection");
  moveCard("deviceIp","mount-connection"); // detailed runtime health belongs in Device

  // The former "Celebration preferences" card mixed WLED gain, horn enable,
  // screen inversion and the admin password. Split it into purposeful cards.
  const original=$("brightness").closest(".card");
  const gainCard=newCard("Celebration timing & intensity",
    "Shared brightness and event duration for WLED celebrations.","mount-wled");
  transferRange(document.querySelector('label[for="brightness"]'),
                $("seconds"),gainCard);
  const hornCard=newCard("Enable goal audio",
    "Allow the onboard speaker to play the selected horn for eligible NHL goals.",
    "mount-audio");
  transferRange($("horn").closest(".toggle"),$("hornHelp"),hornCard);
  const displayCard=newCard("Display color correction",
    "Correct the panel's color inversion only if its colors are visibly wrong.",
    "mount-screen");
  const invert=$("screenInverted").closest(".toggle");
  transferRange(invert,invert.nextElementSibling,displayCard);
  const accessCard=newCard("Dashboard security",
    "Set a new local dashboard password without exposing Wi-Fi credentials.",
    "mount-network");
  const passwordLabel=document.querySelector('label[for="adminPassword"]');
  const finalHint=$("adminPassword").nextElementSibling;
  transferRange(passwordLabel,finalHint,accessCard);
  original.remove();

  // TEST LAB: move the *actual* controls, not a second set of lookalikes.
  // Screen tests are never mixed with live overlay configuration.
  const screenLab=$("mount-lab-screen");
  const screenActions=$("overlayTestGoal").closest(".actions");
  const visualNote=screenActions.nextElementSibling;
  screenLab.appendChild(screenActions);
  if(visualNote&&visualNote.matches("p"))screenLab.appendChild(visualNote);
  const gifPreview=$("goalGifPreview");
  const gifRow=document.createElement("div");gifRow.className="actions";
  const gifTitle=document.createElement("p");
  gifTitle.className="small muted";
  gifTitle.textContent="S3 goal GIF: preview the installed animation on the device.";
  gifRow.appendChild(gifPreview);
  screenLab.append(gifTitle,gifRow);

  // WLED tests are physical outputs; keep stop/restore beside the action.
  moveCard("wledTestGoalHome","mount-lab-wled");
  const quick=newCard("Quick home / away color",
    "Run a basic color test for the armed game without changing game selection.",
    "mount-lab-wled");
  const quickButtons=$("testHome").closest(".actions");
  const quickNote=quickButtons.nextElementSibling;
  quick.appendChild(quickButtons);
  if(quickNote&&quickNote.matches("p"))quick.appendChild(quickNote);

  // The CYD rear-RGB test set has three distinct old cards, now one area.
  moveCard("rearTestHome","mount-lab-hardware");
  moveCard("rearTeam","mount-lab-hardware");
  moveCard("rearDiagRed","mount-lab-hardware");
  moveCard("rearFullHome","mount-lab-full");
  // The S3 speaker library stays in Customize for contextual audition.
  // Dedicated sound tests and codec diagnostics belong here.
  const audioLab=$("mount-lab-audio");
  audioLab.appendChild($("s3AudioPreview").closest(".actions"));
  audioLab.appendChild($("s3AudioDiagnostics"));

  // S3 external case LEDs have their own, independent wiring diagnostics.
  const caseLab=newCard("Waveshare case output tests",
    "LEFT = HOME, RIGHT = AWAY. Only available after confirming LED wiring.",
    "mount-lab-case");
  const caseActions=document.querySelector(".s3-case-test").closest(".actions");
  caseLab.appendChild(caseActions);

  // Remove old duplicate Save buttons (retain IDs and JS handlers for
  // existing regression tests); the new shell offers one consistent save.
  ["saveDisplay","saveDevices","saveRear"].forEach(id=>{
    $(id).closest(".actions").classList.add("legacy-save");
  });
  $("saveSettings").closest(".card").classList.add("legacy-save");
}

const PORTAL_ROUTES={
  dashboard:{page:"dashboard"},games:{page:"games"},
  customize:{page:"customize"},lab:{page:"lab"},device:{page:"device"},
  events:{page:"events"},
  display:{page:"customize",anchor:"zone-screen"},
  devices:{page:"customize",anchor:"zone-wled"},
  rear:{page:"customize",anchor:"zone-hardware"},
  settings:{page:"device",anchor:"zone-network"},
  firmware:{page:"device",anchor:"zone-maintenance"}
};
let portalDirty=false;
function setPortalDirty(isDirty){
  portalDirty=!!isDirty;
  ["customizeSaveState","deviceSaveState"].forEach(id=>{
    const node=$(id);
    node.classList.toggle("is-dirty",portalDirty);
    node.textContent=portalDirty?"Unsaved changes · select Save settings":
      id==="customizeSaveState"?
        "Shared settings saved · audio volume, favorite teams, WAV and case LEDs have their own Apply buttons":
        "Network and dashboard settings saved on this device";
  });
  document.querySelector('[data-tab="customize"].tab')
    .classList.toggle("has-dirty",portalDirty);
}
function trackPortalChanges(){
  const stored=new Set([
    "ssid","wifiPassword","adminPassword","brightness","seconds","horn",
    "wledPP","wledGameStart","wledFinal","wledGoalStyle","favoriteSide",
    "rearEnabled","rearGoal","rearPowerPlay","rearBrightness",
    "rearGoalSeconds","screenInverted","overlayEnabled","overlayGoals",
    "overlayPowerPlays","overlayStart","overlayFinal","overlayFlash",
    "overlayLogos","displayAllLiveEvents","overlayGoalSeconds",
    "overlayFinalSeconds","cycleGames","showStandings","cycleSeconds",
    "displayTimezone"
  ]);
  ["customize","device"].forEach(panelId=>{
    $(panelId).addEventListener("change",event=>{
      const id=event.target.id||"";
      if(stored.has(id)||/^(ip[0-7]|wname[0-7]|enabled[0-7]|event[0-7]_(goal|pp|start|final))$/.test(id))
        setPortalDirty(true);
    });
    $(panelId).addEventListener("input",event=>{
      const id=event.target.id||"";
      if(stored.has(id)||/^(ip|wname)[0-7]$/.test(id))setPortalDirty(true);
    });
  });
}

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
const PORTAL_DEFAULT_ZONES={
  customize:"zone-favorites",lab:"lab-screen",device:"zone-network"
};
const portalLastZone={};
function jumpToZone(anchor){
  const section=$(anchor);
  if(!section)return;
  const panel=section.closest(".panel");
  if(!panel)return;
  // Progressive disclosure. Hidden sections retain their original controls,
  // so a global save always includes all loaded game-day settings.
  Array.from(panel.children).forEach(node=>{
    if(node.classList.contains("portal-zone"))
      node.classList.toggle("active-zone",node===section);
  });
  panel.querySelectorAll(".subnav button").forEach(button=>{
    const selected=button.dataset.jump===anchor;
    button.classList.toggle("active",selected);
    button.setAttribute("aria-pressed",String(selected));
  });
  portalLastZone[panel.id]=anchor;
  const reduced=matchMedia("(prefers-reduced-motion:reduce)").matches;
  panel.scrollIntoView({block:"start",behavior:reduced?"auto":"smooth"});
}
function tab(id,anchor){
  const route=PORTAL_ROUTES[id]||PORTAL_ROUTES.dashboard;
  const page=route.page;
  document.querySelectorAll(".panel").forEach(x=>
    x.classList.toggle("active",x.id===page));
  document.querySelectorAll(".tab").forEach(x=>{
    const selected=x.dataset.tab===page;
    x.classList.toggle("selected",selected);
    if(selected)x.setAttribute("aria-current","page");
    else x.removeAttribute("aria-current");
  });
  const chosenZone=anchor||route.anchor||portalLastZone[page]||
                   PORTAL_DEFAULT_ZONES[page];
  if(chosenZone){
    requestAnimationFrame(()=>jumpToZone(chosenZone));
  }else{
    const reduced=matchMedia("(prefers-reduced-motion:reduce)").matches;
    scrollTo({top:0,behavior:reduced?"auto":"smooth"});
  }
  // Bookmark navigational state without requesting extra firmware routes.
  history.replaceState(null,"","#"+page);
}
document.querySelectorAll("[data-tab]").forEach(x=>x.addEventListener("click",
  ()=>tab(x.dataset.tab,x.dataset.anchor)));
document.querySelectorAll("[data-jump]").forEach(x=>x.addEventListener("click",
  ()=>jumpToZone(x.dataset.jump)));
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
  builtIn.textContent=(currentStatus?.audioSupported?"On-device LED":"CYD rear LED")+
    ": "+(row.rear||"Not applicable");
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
function renderBroadcastHero(){
  const s=currentStatus;
  if(!s)return;
  const armed=currentGames.find(g=>g.id===s.selectedId);
  const shown=armed||
    currentGames.find(g=>g.state==="LIVE"||g.state==="CRIT")||
    currentGames[0];
  const isLive=shown&&(shown.state==="LIVE"||shown.state==="CRIT");
  const status=shown?(shown.status||shown.state||"NHL"):
    s.scoreboardLoaded?"NO GAMES TODAY":"CONNECTING";
  $("heroLiveTag").textContent=isLive?"● LIVE":status;
  $("heroLiveTag").classList.toggle("is-live",!!isLive);
  $("heroGameMode").textContent=armed?"ARMED MATCHUP":
    shown?"FEATURED MATCHUP":"NHL SCOREBOARD";
  $("heroAway").textContent=shown?shown.away:"—";
  $("heroHome").textContent=shown?shown.home:"—";
  $("heroAwayMark").textContent=shown?shown.away:"NHL";
  $("heroHomeMark").textContent=shown?shown.home:"NHL";
  const teamColor=team=>{
    const entry=rearPalette.find(t=>t.abbr===team);
    return /^#[0-9a-fA-F]{6}$/.test(entry?.hex||"")?entry.hex:"#77cbd9";
  };
  $("heroAwayMark").style.setProperty("--team-accent",teamColor(shown?.away));
  $("heroHomeMark").style.setProperty("--team-accent",teamColor(shown?.home));
  const upcoming=shown&&(shown.state==="FUT"||shown.state==="PRE");
  $("heroScore").textContent=shown&&!upcoming?
    shown.awayScore+" : "+shown.homeScore:"— : —";
  $("heroStatus").textContent=shown?status:
    "The scoreboard will appear when NHL games are available";
  $("heroArmStatus").textContent=armed?"ARMED FOR WLED":"DISPLAY ONLY";
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
  $("rearFollowCount").textContent=((s.followed||[]).filter(id=>id!==s.selectedId).length+
    (s.selectedId?1:0))+" / "+(s.followCapacity||20)+" daily games";
  $("dashLiveCount").textContent=String(s.liveGameCount??0);
  $("dashFavoriteCount").textContent=String(popcount(s.favoriteTeams??0));
  $("dashFollowCount").textContent=String(
    (s.followed||[]).filter(id=>id!==s.selectedId).length+(s.selectedId?1:0));
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
  const caseCard=$("s3CaseLedCard");
  caseCard.hidden=!s.audioSupported;
  caseCard.style.display=s.audioSupported?"":"none";
  if(s.audioSupported){
    $("s3CaseLedStatus").textContent=s.caseLedEnabled
      ? "GPIO17/18 output ENABLED · physical wiring still unverified"
      : "Safe / OFF · GPIO17 and GPIO18 not driven";
    document.querySelectorAll(".s3-case-test").forEach(b=>
      b.disabled=!s.caseLedEnabled);
    $("s3CaseOff").disabled=!s.caseLedEnabled;
  }
  const s3=!!s.audioSupported;
  $("platformTag").textContent=s3?"WAVESHARE S3 · 3.5″":"ESP32 CYD · 2.8″";
  $("rearEnabled").closest(".card").hidden=s3;
  $("lab-audio").hidden=!s3;
  const audioTab=document.querySelector('[data-jump="lab-audio"]');
  if(audioTab)audioTab.hidden=!s3;
  if(!s3 && portalLastZone.lab==="lab-audio")jumpToZone("lab-screen");
  $("mount-lab-hardware").hidden=s3;
  $("mount-lab-case").hidden=!s3;
  $("goalGifPreview").hidden=!s3;
  renderBroadcastHero();
  const audioCard=$("s3AudioCard");
  audioCard.hidden=!s.audioSupported;
  audioCard.style.display=s.audioSupported?"":"none";
  if(s.audioSupported){
    $("hornLabel").textContent="Enable onboard S3 goal horn (ES8311)";
    const gain=Number(s.audioVolume??70);
    $("hornHelp").textContent="Actual followed/armed goals use the saved S3 speaker volume; preview works even when the horn setting is off.";
    $("s3AudioStatus").textContent=s.audioReady?
      (s.audioPlaying?(s.audioLastClip||"Audio")+" playing · "+gain+"%":
       (s.audioHasPlayed?(s.audioLastStreamOk?"I2S stream finished":"Audio stream incomplete"):"Codec initialized · speaker not yet tested")+
       " · "+gain+"% volume"):
      "Speaker unavailable: "+(s.audioError||"Codec not initialized");
    $("s3AudioPreview").disabled=!s.audioReady||!!s.audioPlaying;
    $("s3AudioTone").disabled=!s.audioReady||!!s.audioPlaying;
    $("s3AudioVolumeApply").disabled=!s.audioReady||!!s.audioPlaying;
    $("s3SoundBuiltInPreview").disabled=!s.audioReady||!!s.audioPlaying;
    $("s3SoundUploadButton").disabled=!!s.audioPlaying;
  }else{
    $("hornLabel").textContent="Enable built-in horn (GPIO26 speaker)";
    $("hornHelp").textContent="The CYD plays a simple GPIO26 tone sequence; custom WAV playback is not included.";
  }
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
    const title=document.createElement("h3");title.textContent="Controller "+(i+1)+(device.name?" · "+device.name:"");
    const nameLabel=document.createElement("label");nameLabel.htmlFor="wname"+i;nameLabel.textContent="Friendly name";
    const name=document.createElement("input");name.id="wname"+i;name.placeholder="e.g. TV Backlights";name.maxLength=32;name.value=device.name||"";
    const label=document.createElement("label");label.htmlFor="ip"+i;label.textContent="Local IP address";
    const ip=document.createElement("input");ip.id="ip"+i;ip.placeholder="192.168.1.71";ip.value=device.ip||"";ip.inputMode="decimal";
    const row=document.createElement("div");row.className="toggle";row.style.marginTop="10px";
    const enable=document.createElement("input");enable.id="enabled"+i;enable.type="checkbox";enable.checked=!!device.enabled;
    const lbl=document.createElement("label");lbl.htmlFor=enable.id;lbl.textContent="Include this WLED device in celebrations";
    row.append(enable,lbl);card.append(title,nameLabel,name,label,ip,row);
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
    const zones=document.createElement("div");
    zones.className="wled-zones";
    const manage=document.createElement("button");
    manage.type="button";manage.className="ghost";
    manage.textContent=device.zoneEnabled?"Manage segments · Zones ON":"Manage segments · Legacy mode";
    manage.onclick=()=>perform(async()=>{
      if(currentConfig.wled[i].ip!==$("ip"+i).value.trim())
        throw Error("Save this controller IP before editing its segments");
      if(!currentConfig.wled[i].ip)
        throw Error("Save a WLED IP before discovering segments");
      await openWledSegments(i,zones);
    });
    const sceneButton=document.createElement("button");
    sceneButton.type="button";sceneButton.className="ghost";
    sceneButton.textContent="Celebration scene builder";
    const sceneHost=document.createElement("div");
    sceneHost.className="wled-scene-host";
    sceneButton.onclick=()=>perform(async()=>{
      if(currentConfig.wled[i].ip!==$("ip"+i).value.trim())
        throw Error("Save this controller IP before editing scenes");
      if(!currentConfig.wled[i].ip)
        throw Error("Save a WLED controller before creating celebration scenes");
      await openWledSceneBuilder(i,sceneHost);
    });
    card.append(result,manage,sceneButton,zones,sceneHost);list.append(card);
  });
  renderWledControllerResults();
}
function availableWledSlot(ip){
  const saved=currentConfig?.wled||[];
  const duplicate=saved.findIndex((d,i)=>$("ip"+i)?.value.trim()===ip);
  if(duplicate>=0)return {slot:duplicate,exists:true};
  const free=saved.findIndex((d,i)=>!$("ip"+i)?.value.trim());
  return {slot:free,exists:false};
}
function stageWledDevice(ip,name){
  const target=availableWledSlot(ip);
  if(target.slot<0)throw Error("All eight WLED slots are occupied. Clear one before adding.");
  const i=target.slot;
  $("ip"+i).value=ip;
  if(!target.exists)$("wname"+i).value=name||"WLED";
  // Auto-discovery MUST NOT enable participation or issue a light command.
  if(!target.exists)$("enabled"+i).checked=false;
  setPortalDirty(true);
  message(target.exists?"Already saved in controller "+(i+1):
    "Staged "+(name||ip)+" in controller "+(i+1)+". Choose events and Save settings.");
  tab("customize","zone-wled");
}
function renderDiscoveredWled(report){
  const host=$("wledDiscovered");host.replaceChildren();
  (report.devices||[]).forEach(device=>{
    const row=document.createElement("div");row.className="item";
    const header=document.createElement("div");header.className="row";
    const name=document.createElement("strong");
    name.textContent=device.name||"WLED";
    const state=document.createElement("span");state.className="small muted";
    state.textContent=device.online?"Verified WLED":"Offline / not verified";
    header.append(name,state);row.append(header);
    const detail=document.createElement("p");detail.className="small muted";
    detail.textContent=device.ip+" · "+
      (device.online?device.leds+" LEDs · "+device.version:"No JSON response");
    row.append(detail);
    if(device.online){
      const choose=document.createElement("button");choose.type="button";
      choose.textContent=device.savedSlot>=0?"Already saved · slot "+(device.savedSlot+1):
        "Add to GameDay";
      choose.disabled=device.savedSlot>=0;
      choose.onclick=()=>stageWledDevice(device.ip,device.name);
      row.append(choose);
    }
    host.append(row);
  });
  $("wledScanStatus").textContent=report.scanning?
    "Checking local WLEDs "+(report.checked||0)+" / "+(report.candidateCount||0)+"…":
    report.complete?
      (report.found||0)+" verified device(s) · "+(report.error||"Scan complete"):
      "Ready to discover. "+(report.error||"");
}
$("wledScan").onclick=()=>perform(async()=>{
  $("wledScan").disabled=true;
  try{
    await post("/api/wled/discover",{});
    for(let i=0;i<30;++i){
      const report=await api("/api/wled/discovery");
      renderDiscoveredWled(report);
      if(!report.scanning)return;
      await new Promise(resolve=>setTimeout(resolve,700));
    }
    message("Scan continues on your device. Refresh status to see the results.");
  }finally{$("wledScan").disabled=false;}
});
$("wledManualCheck").onclick=()=>perform(async()=>{
  const ip=$("wledManualIp").value.trim();
  if(!/^\d{1,3}(?:\.\d{1,3}){3}$/.test(ip))
    throw Error("Enter a WLED IPv4 address.");
  const info=await post("/api/wled/probe",{ip});
  stageWledDevice(info.ip,info.name);
});
async function openWledSegments(slot,host){
  host.replaceChildren();
  const busy=document.createElement("p");busy.className="small muted";
  busy.textContent="Reading live segment layout from WLED…";host.append(busy);
  const data=await api("/api/wled/segments?slot="+slot);
  host.replaceChildren();
  const heading=document.createElement("h4");
  heading.textContent="Controller "+(slot+1)+" · "+data.segments.length+" segments";
  host.append(heading);
  const hint=document.createElement("p");hint.className="small muted";
  hint.textContent="LED ranges come directly from WLED, start inclusive / stop exclusive. "+
    (data.controllerOn?
      "Identify briefly highlights only the chosen segment in white.":
      "This controller is OFF. Turn it on in WLED to identify segments.")+
    (data.truncated?" Only the first 16 segments can be mapped.":"");
  host.append(hint);
  if(!data.segments.length){
    const no=document.createElement("p");no.className="small muted";
    no.textContent="WLED reports no active segments. Create segments in WLED first.";
    host.append(no);return;
  }
  const enabledWrap=document.createElement("div");
  enabledWrap.className="toggle";
  const enabled=document.createElement("input");
  enabled.type="checkbox";enabled.checked=!!data.mappingEnabled;
  const enabledLabel=document.createElement("label");
  enabledLabel.textContent="Enable segment-based Home / Away / Neutral routing";
  enabledWrap.append(enabled,enabledLabel);host.append(enabledWrap);
  const picks=[];
  data.segments.forEach(seg=>{
    const row=document.createElement("div");row.className="wled-segment-row";
    const summary=document.createElement("div");
    const title=document.createElement("strong");
    title.textContent=(seg.name||("Segment "+seg.id))+" · #"+seg.id;
    const range=document.createElement("p");range.className="small muted";
    range.textContent="LED "+seg.start+"–"+(seg.stop-1)+" ("+
      (seg.stop-seg.start)+" pixels) · "+(seg.enabled?"On":"Off");
    const bar=document.createElement("div");bar.className="wled-range-bar";
    bar.setAttribute("aria-hidden","true");
    const fill=document.createElement("div");fill.className="wled-range-fill";
    bar.append(fill);summary.append(title,range,bar);
    const controls=document.createElement("div");controls.className="actions";
    const select=document.createElement("select");
    select.setAttribute("aria-label","Segment "+seg.id+" celebration zone");
    [["0","Unassigned"],["1","Home"],["2","Away"],["3","Neutral"]].forEach(
      ([v,name])=>{
        const option=document.createElement("option");
        option.value=v;option.textContent=name;select.append(option);
      });
    select.value=String(seg.role);
    const update=()=>{
      row.dataset.zone=select.value;
      fill.style.width="100%";
    };
    select.onchange=update;update();
    const identify=document.createElement("button");identify.type="button";
    identify.textContent="Identify";
    identify.disabled=!data.controllerOn;
    identify.onclick=()=>perform(async()=>{
      const result=await post("/api/wled/segment-test",{slot,id:seg.id});
      if(!result.ok)throw Error("Segment highlight failed; restore requested");
      message("Highlighting segment "+seg.id+" for 1.8 seconds; previous state will restore");
    });
    controls.append(select,identify);row.append(summary,controls);
    host.append(row);picks.push({id:seg.id,select});
  });
  const footer=document.createElement("div");footer.className="actions";
  const save=document.createElement("button");save.className="primary";
  save.textContent="Save zone assignments";
  save.onclick=()=>perform(async()=>{
    const payload={slot,enabled:enabled.checked,
      roles:picks.map(item=>({id:item.id,role:Number(item.select.value)}))};
    const result=await post("/api/wled/segment-config",payload);
    currentConfig.wled[slot].zoneEnabled=!!result.enabled;
    message(result.enabled?"Zone mapping enabled for controller "+(slot+1):
      "Legacy whole-controller mode restored for controller "+(slot+1));
    await openWledSegments(slot,host);
  });
  const close=document.createElement("button");close.textContent="Close";
  close.onclick=()=>host.replaceChildren();
  footer.append(save,close);host.append(footer);
  const alert=document.createElement("p");alert.className="small muted";
  alert.textContent="With zones enabled, Goal and PP affect only the matching team zone and Neutral. "+
    "Game start/final uses team-colored zones. Other segments remain untouched. "+
    "The WLED controller must already be on.";
  host.append(alert);
}
// 8C scene editor. Every change lives in one local draft until explicit
// Save scenes; switching event tabs never writes flash or WLED devices.
const WLED_SCENE_EVENTS=[
  {id:1,name:"Goal",defaultFx:23},
  {id:2,name:"Power play",defaultFx:2},
  {id:3,name:"Game start",defaultFx:3},
  {id:4,name:"Final · win",defaultFx:17},
  {id:5,name:"Final · loss",defaultFx:12}
];
const WLED_SCENE_ZONES=[
  {id:1,name:"Home",hint:"Home goals and power plays"},
  {id:2,name:"Away",hint:"Away goals and power plays"},
  {id:3,name:"Neutral",hint:"Either team's goals and power plays"}
];
const WLED_SCENE_EFFECTS=[
  [0,"Solid"],[2,"Breathe"],[3,"Wipe"],[12,"Fade"],
  [17,"Twinkle"],[23,"Strobe"],[28,"Chase"]
];
function sceneControl(label,element){
  const wrapper=document.createElement("label");
  wrapper.className="wled-scene-field";
  const title=document.createElement("span");title.textContent=label;
  wrapper.append(title,element);return wrapper;
}
async function openWledSceneBuilder(slot,host){
  host.replaceChildren();
  const info=await api("/api/wled/scenes?slot="+slot);
  const drafts=info.scenes.map(scene=>({...scene}));
  host.replaceChildren();
  const frame=document.createElement("div");frame.className="wled-scene-builder";
  const head=document.createElement("div");head.className="wled-scene-title";
  const title=document.createElement("h4");
  title.textContent="Celebration scenes · "+(info.name||"Controller "+(slot+1));
  const close=document.createElement("button");
  close.type="button";close.className="ghost";close.textContent="Close";
  close.onclick=()=>host.replaceChildren();
  head.append(title,close);frame.append(head);
  const instruction=document.createElement("p");instruction.className="small muted";
  instruction.textContent="Set an effect for each NHL event and lighting zone. "+
    "Leave a template on Inherit to retain the Phase 8B behavior. "+
    "Your scene edits apply to this controller only, and never change its LED ranges.";
  frame.append(instruction);
  if(!info.mappingEnabled){
    const warning=document.createElement("p");warning.className="wled-scene-warning";
    warning.textContent="Zone routing is OFF. Assign segments and enable Home/Away/Neutral "+
      "mapping under Manage segments before these scenes can play.";
    frame.append(warning);
  }
  let eventId=1;
  const toolbar=document.createElement("div");toolbar.className="wled-scene-toolbar";
  const eventSelect=document.createElement("select");
  eventSelect.setAttribute("aria-label","Scene event");
  WLED_SCENE_EVENTS.forEach(event=>{
    const option=document.createElement("option");option.value=String(event.id);
    option.textContent=event.name;eventSelect.append(option);
  });
  toolbar.append(sceneControl("NHL event",eventSelect));
  const defaultEvent=document.createElement("button");
  defaultEvent.type="button";defaultEvent.className="ghost";
  defaultEvent.textContent="Reset this event to inheritance";
  toolbar.append(defaultEvent);frame.append(toolbar);
  const sceneGrid=document.createElement("div");sceneGrid.className="wled-scene-grid";
  frame.append(sceneGrid);
  const foot=document.createElement("div");foot.className="actions";
  const save=document.createElement("button");save.type="button";
  save.className="primary";save.textContent="Save all scene templates";
  const discard=document.createElement("button");discard.type="button";
  discard.textContent="Discard changes";
  discard.onclick=()=>perform(()=>openWledSceneBuilder(slot,host));
  foot.append(save,discard);frame.append(foot);
  const tip=document.createElement("p");tip.className="small muted";
  tip.textContent="Preview runs the LAST SAVED scene for 3 seconds. Save edits first. "+
    "A preview will briefly change real WLED segments and restore their prior appearance. "+
    "Only the armed NHL game can trigger live WLED celebrations.";
  frame.append(tip);host.append(frame);
  function updateDraft(ev,zone,key,value){
    const scene=drafts.find(x=>x.event===ev&&x.zone===zone);
    if(scene)scene[key]=value;
    save.dataset.dirty="true";
    save.textContent="Save scene changes";
  }
  function draw(){
    sceneGrid.replaceChildren();
    const event=WLED_SCENE_EVENTS.find(x=>x.id===eventId);
    WLED_SCENE_ZONES.forEach(zone=>{
      const draft=drafts.find(x=>x.event===eventId&&x.zone===zone.id);
      const card=document.createElement("div");card.className="wled-scene-zone";
      card.dataset.zone=String(zone.id);
      const heading=document.createElement("div");heading.className="wled-scene-title";
      const name=document.createElement("strong");name.textContent=zone.name+" zone";
      const label=document.createElement("span");label.className="small muted";
      label.textContent=zone.hint;
      heading.append(name,label);card.append(heading);
      const toggleWrap=document.createElement("div");toggleWrap.className="toggle";
      const toggle=document.createElement("input");toggle.type="checkbox";
      toggle.checked=!!draft.custom;
      const toggleLabel=document.createElement("label");
      toggleLabel.textContent="Use custom scene (otherwise inherit)";
      toggleWrap.append(toggle,toggleLabel);card.append(toggleWrap);
      const controls=document.createElement("div");controls.className="wled-scene-controls";
      const effectSelect=document.createElement("select");
      WLED_SCENE_EFFECTS.forEach(([id,label])=>{
        const opt=document.createElement("option");opt.value=String(id);
        opt.textContent=label;effectSelect.append(opt);
      });
      effectSelect.value=String(draft.fx??event.defaultFx);
      const mkNumber=(label,key,min,max,step,initial)=>{
        const inp=document.createElement("input");inp.type="range";
        inp.min=String(min);inp.max=String(max);inp.step=String(step);
        inp.value=String(initial);
        const value=document.createElement("b");value.className="small";
        value.textContent=String(initial);
        inp.oninput=()=>{value.textContent=inp.value;updateDraft(eventId,zone.id,key,Number(inp.value));};
        const wrap=sceneControl(label,inp);wrap.append(value);controls.append(wrap);
      };
      const effectField=sceneControl("WLED effect",effectSelect);
      controls.append(effectField);
      effectSelect.onchange=()=>updateDraft(eventId,zone.id,"fx",Number(effectSelect.value));
      mkNumber("Speed","speed",0,255,1,draft.speed??128);
      mkNumber("Intensity","intensity",0,255,1,draft.intensity??128);
      mkNumber("Brightness","brightness",1,255,1,draft.brightness??190);
      const color=document.createElement("select");
      [["0","Automatic (8B defaults)"],["1","Team primary color"],
       ["2","White"],["3","Custom hex color"]].forEach(([id,label])=>{
        const opt=document.createElement("option");opt.value=id;opt.textContent=label;
        color.append(opt);
      });
      color.value=String(draft.colorMode??0);
      const colorField=sceneControl("Color behavior",color);
      controls.append(colorField);
      const colorHex=document.createElement("input");colorHex.type="color";
      colorHex.value=/^#[0-9a-f]{6}$/i.test(draft.hex||"")?
        draft.hex:"#ffffff";
      const hexField=sceneControl("Custom color",colorHex);
      hexField.hidden=color.value!=="3";controls.append(hexField);
      color.onchange=()=>{
        updateDraft(eventId,zone.id,"colorMode",Number(color.value));
        hexField.hidden=color.value!=="3";
      };
      colorHex.oninput=()=>updateDraft(eventId,zone.id,"hex",colorHex.value);
      const duration=document.createElement("input");duration.type="number";
      duration.min="0";duration.max="30";duration.step="1";
      duration.value=String(draft.duration??0);
      duration.disabled=eventId===2;
      duration.onchange=()=>updateDraft(eventId,zone.id,"duration",Number(duration.value));
      controls.append(sceneControl(eventId===2?
        "Duration · until official PP ends":"Duration · 0 = shared, 2–30 seconds",
        duration));
      toggle.onchange=()=>{
        updateDraft(eventId,zone.id,"custom",toggle.checked);
        controls.hidden=!toggle.checked;
      };
      controls.hidden=!toggle.checked;
      card.append(controls);
      const actions=document.createElement("div");actions.className="actions";
      const preview=document.createElement("button");preview.type="button";
      preview.textContent="Preview saved scene";
      preview.disabled=!info.mappingEnabled;
      preview.onclick=()=>perform(async()=>{
        if(save.dataset.dirty==="true")
          throw Error("Save or discard your scene edits before previewing");
        const result=await post("/api/wled/scene-preview",{
          slot,event:eventId,zone:zone.id
        });
        if(!result.ok)throw Error("WLED preview failed; restore scheduled");
        message(zone.name+" · "+event.name+" preview playing for 3 seconds");
      });
      actions.append(preview);card.append(actions);sceneGrid.append(card);
    });
  }
  eventSelect.onchange=()=>{eventId=Number(eventSelect.value);draw();};
  defaultEvent.onclick=()=>{
    WLED_SCENE_ZONES.forEach(zone=>updateDraft(eventId,zone.id,"custom",false));
    draw();
  };
  save.onclick=()=>perform(async()=>{
    const validated=drafts.map(scene=>{
      const v={...scene};
      if(!v.custom)return {event:v.event,zone:v.zone,custom:false};
      if(v.event===2)v.duration=0;
      if(v.duration!==0 && (v.duration<2||v.duration>30))
        throw Error("Custom duration must be 0 or 2–30 seconds");
      return v;
    });
    await post("/api/wled/scenes",{slot,scenes:validated});
    message("Saved all 15 scene templates on controller "+(slot+1));
    await openWledSceneBuilder(slot,host);
  });
  draw();
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
  return Array.from({length:(currentConfig?.wled?.length||8)},(_,i)=>{
    const choices=[["goal",1],["pp",2],["start",4],["final",8]];
    const events=choices.reduce((mask,[name,bit])=>
      mask+($("event"+i+"_"+name).checked?bit:0),0);
    return {ip:$("ip"+i).value.trim(),name:$("wname"+i).value.trim(),enabled:$("enabled"+i).checked,events};
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
  setPortalDirty(false);
  message("GameDay settings saved");
}
async function loadConfig(){
  currentConfig=await api("/api/config");
  $("ssid").value=currentConfig.ssid||"";
  $("brightness").value=currentConfig.brightness;
  $("brightValue").textContent=currentConfig.brightness+" / 255";
  $("seconds").value=currentConfig.seconds;
  $("horn").checked=!!currentConfig.horn;
  if(currentConfig.s3Volume!==undefined){
    $("s3AudioVolume").value=String(currentConfig.s3Volume);
    $("s3AudioVolumeValue").textContent=currentConfig.s3Volume+"%";
  }
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
$("portalSave").onclick=()=>perform(saveConfig);
$("deviceSave").onclick=()=>perform(saveConfig);
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
async function readS3AudioProbe(){
  const d=await api("/api/audio/diagnostics");
  const hex=v=>"0x"+Number(v||0).toString(16).toUpperCase().padStart(2,"0");
  $("s3AudioDiagnostics").textContent=[
    "ES8311 I2C: "+(d.codecRead?"OK":"READ FAILED")+" · id="+hex(d.codecId1)+"/"+hex(d.codecId2),
    "Codec volume reg: "+hex(d.codecVolume)+
      " (expected "+hex(Math.floor((d.volumePercent||70)*256/100)-1)+
      " for "+(d.volumePercent||70)+"%)",
    "DAC mute reg: "+hex(d.codecMute)+" (bits 5/6 should be clear)",
    "TCA9554: "+(d.expanderRead?"OK":"READ FAILED")+
      " · P7 direction="+((d.ampDirection&128)?"INPUT":"OUTPUT")+
      " · PA="+((d.ampOutput&128)?"HIGH":"LOW"),
    "Audio ready: "+!!d.ready+" · Playing: "+!!d.playing+
      " · Last clip: "+(d.lastClip||"none"),
    "I2S streamed: "+(d.framesWritten||0)+" frames"+
      " · complete="+!!d.streamOk+" · "+(d.streamDurationMs||0)+"ms",
    "Error: "+(d.error||"None")
  ].join("\n");
}
$("s3AudioVolume").oninput=()=>
  $("s3AudioVolumeValue").textContent=$("s3AudioVolume").value+"%";
$("s3AudioVolumeApply").onclick=()=>perform(async()=>{
  const requested=Number($("s3AudioVolume").value);
  const response=await post("/api/audio/volume",{percent:requested});
  currentConfig.s3Volume=response.volume;
  $("s3AudioVolumeValue").textContent=response.volume+"%";
  message("S3 speaker volume saved at "+response.volume+"% — preview to test");
  await refresh();
});
let s3AudioLibrary=null;
async function loadSoundLibrary(){
  const response=await fetch("/api/audio/library",{cache:"no-store"});
  if(!response.ok){
    $("s3SoundLibraryStatus").textContent="Sound library unavailable on this device.";
    return;
  }
  const lib=await response.json();
  s3AudioLibrary=lib;
  $("s3SoundLibraryStatus").textContent=!lib.storageReady?
    "Flash storage unavailable — built-in horn remains available.":
    (lib.selected===0?"Built-in horn selected":"Custom slot "+lib.selected+" selected")+
    " · "+(Math.round((lib.usedBytes||0)/1024))+" KB used of "+
    Math.round((lib.totalBytes||0)/1024)+" KB flash audio/animation space";
  $("s3SoundBuiltInUse").disabled=lib.selected===0;
  const host=$("s3SoundLibrarySlots");
  host.replaceChildren();
  (lib.clips||[]).forEach(c=>{
    const row=document.createElement("div");row.className="item";
    const title=document.createElement("strong");
    title.textContent="Slot "+c.slot+" · "+
      (c.installed?(c.name||"Custom WAV"):"Empty");
    row.append(title);
    const detail=document.createElement("p");
    detail.className="small muted";
    detail.textContent=c.installed?
      (c.durationMs/1000).toFixed(1)+" sec · "+c.sampleRate+" Hz · "+
       (c.channels===1?"mono":"stereo")+" · "+Math.round(c.bytes/1024)+" KB":
      (c.bytes?"Invalid WAV — replace or delete":"Upload a PCM WAV to this slot");
    row.append(detail);
    const actions=document.createElement("div");actions.className="actions";
    const preview=document.createElement("button");
    preview.textContent="Preview";
    preview.disabled=!c.installed;
    preview.onclick=()=>perform(async()=>{
      const result=await post("/api/audio/preview",{source:c.slot});
      message("Playing "+(c.name||"Custom WAV")+" at "+result.volume+"%");
      await refresh();
    });
    actions.append(preview);
    const select=document.createElement("button");
    select.textContent=lib.selected===c.slot?"Selected":"Use for goals";
    select.disabled=!c.installed||lib.selected===c.slot;
    select.onclick=()=>perform(async()=>{
      await post("/api/audio/select",{source:c.slot});
      currentConfig.s3Sound=c.slot;
      await loadSoundLibrary();
      message("Goal sound set to slot "+c.slot);
    });
    actions.append(select);
    const remove=document.createElement("button");
    remove.textContent="Delete";
    remove.disabled=!c.bytes;
    remove.onclick=()=>perform(async()=>{
      if(!confirm("Delete "+(c.name||("slot "+c.slot))+" from device flash?"))return;
      await post("/api/audio/delete",{slot:c.slot});
      await loadSoundLibrary();
      message("Custom WAV removed");
    });
    actions.append(remove);row.append(actions);host.append(row);
  });
}
$("s3SoundBuiltInPreview").onclick=()=>perform(async()=>{
  const result=await post("/api/audio/preview",{source:0});
  message("Built-in horn preview playing at "+result.volume+"%");
});
$("s3SoundBuiltInUse").onclick=()=>perform(async()=>{
  await post("/api/audio/select",{source:0});
  currentConfig.s3Sound=0;
  await loadSoundLibrary();
  message("Built-in goal horn selected");
});
$("s3SoundUploadForm").onsubmit=e=>{e.preventDefault();perform(async()=>{
  const file=$("s3SoundUploadFile").files[0];
  const slot=Number($("s3SoundUploadSlot").value);
  if(!file)throw Error("Choose a WAV file first");
  if(file.size>786432 || file.size<44)
    throw Error("WAV must be between 44 bytes and 768 KB");
  const header=new Uint8Array(await file.slice(0,12).arrayBuffer());
  const tag=Array.from(header.slice(0,4)).map(x=>String.fromCharCode(x)).join("");
  const wave=Array.from(header.slice(8,12)).map(x=>String.fromCharCode(x)).join("");
  if(tag!=="RIFF"||wave!=="WAVE")throw Error("Choose an uncompressed RIFF WAV file");
  const payload=new FormData();
  payload.append("sound",file,file.name);
  $("s3SoundUploadButton").disabled=true;
  try{
    const result=await api("/api/audio/upload?slot="+slot,
                           {method:"POST",body:payload});
    $("s3SoundUploadFile").value="";
    await loadSoundLibrary();
    message("Uploaded "+result.name+" ("+(result.durationMs/1000).toFixed(1)+" sec)");
  }finally{$("s3SoundUploadButton").disabled=false;}
})};
$("s3AudioPreview").onclick=()=>perform(async()=>{
  const result=await post("/api/audio/preview",{});
  $("s3AudioStatus").textContent="Goal sound playing · "+result.volume+"%";
  message("S3 "+(result.sound==="custom-wav"?"custom WAV":"built-in horn")+
          " preview started at "+result.volume+"%");
  await refresh();
});
$("s3AudioTone").onclick=()=>perform(async()=>{
  const result=await post("/api/audio/test-tone",{});
  $("s3AudioStatus").textContent="1 kHz speaker test playing · "+result.volume+"%";
  message("S3 1 kHz speaker test started at "+result.volume+"%");
  await refresh();
});
$("s3AudioProbe").onclick=()=>perform(async()=>{
  await readS3AudioProbe();
  message("S3 codec, amplifier and I2S diagnostics refreshed");
});
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
async function loadS3CaseLeds(){
  const res=await fetch("/api/case-led",{cache:"no-store"});
  if(!res.ok)return; // CYD has no S3 case hardware
  const state=await res.json();
  $("s3CaseEnabled").checked=!!state.enabled;
  $("s3CasePixels").value=String(state.pixelsPerSide||8);
  $("s3CaseBrightness").value=String(state.brightness||96);
  $("s3CaseBrightValue").textContent=$("s3CaseBrightness").value+" / 128";
  $("s3CaseSeconds").value=String(state.goalSeconds||6);
  $("s3CaseGoal").checked=!!state.goal;
  $("s3CasePP").checked=!!state.powerPlay;
  $("s3CaseStart").checked=!!state.gameStart;
  $("s3CaseFinal").checked=!!state.final;
  $("s3CaseLedStatus").textContent=state.enabled?
    "Case outputs ENABLED · GPIO17 LEFT and GPIO18 RIGHT · physical result unverified":
    "Case outputs OFF · safe until case wiring is completed";
}
$("s3CaseBrightness").oninput=()=>
  $("s3CaseBrightValue").textContent=$("s3CaseBrightness").value+" / 128";
$("s3CaseSave").onclick=()=>perform(async()=>{
  const enable=$("s3CaseEnabled").checked;
  const wasEnabled=!!currentStatus?.caseLedEnabled;
  let wiredConfirmed=false;
  if(enable&&!wasEnabled){
    wiredConfirmed=confirm(
      "Only enable after you have wired both 5V LED strips using a 74AHCT125 level shifter, " +
      "330-ohm signal resistors, external fused 5V supply and a common ground. " +
      "Are the case LEDs safely installed and ready for testing?");
    if(!wiredConfirmed){
      $("s3CaseEnabled").checked=false;
      return;
    }
  }
  const data={
    enabled:enable,wiredConfirmed,
    pixelsPerSide:Number($("s3CasePixels").value),
    brightness:Number($("s3CaseBrightness").value),
    goalSeconds:Number($("s3CaseSeconds").value),
    goal:$("s3CaseGoal").checked,powerPlay:$("s3CasePP").checked,
    gameStart:$("s3CaseStart").checked,final:$("s3CaseFinal").checked
  };
  await post("/api/case-led/config",data);
  await refresh();await loadS3CaseLeds();
  message(enable?"Case LED GPIO outputs enabled":"Case LED outputs disabled");
});
document.querySelectorAll(".s3-case-test").forEach(btn=>
  btn.onclick=()=>perform(async()=>{
    await post("/api/case-led/test",{
      mode:btn.dataset.caseMode,side:btn.dataset.caseSide
    });
    message("Case LED "+btn.dataset.caseSide+" "+btn.dataset.caseMode+
            " preview started (no WLED or NHL events)");
  })
);
$("s3CaseOff").onclick=()=>perform(async()=>{
  await post("/api/case-led/test",{mode:"off",side:"home"});
  message("All case LED effects stopped");
});
async function loadGoalGif(){
  const res=await fetch("/api/goal-gif");
  if(!res.ok)return; // The legacy CYD has no GIF hardware capability.
  const info=await res.json();
  $("s3GoalGifCard").hidden=!info.supported;
  $("s3GoalGifCard").style.display=info.supported?"block":"none";
  if(!info.supported)return;
  $("goalGifStatus").textContent=info.installed
    ? "Installed · "+Math.round(info.bytes/1024)+" KB · loops during each GOAL event"
    : "No animation installed · choose your GlowDeck_Goal_320x240.gif file";
}
$("goalGifUploadForm").onsubmit=e=>{e.preventDefault();perform(async()=>{
  const file=$("goalGifFile").files[0];if(!file)return;
  if(file.size>1048576)throw Error("GIF must be under 1 MB");
  const header=new Uint8Array(await file.slice(0,10).arrayBuffer());
  const signature=String.fromCharCode(...header.slice(0,6));
  const width=header[6]|(header[7]<<8),height=header[8]|(header[9]<<8);
  if(!["GIF87a","GIF89a"].includes(signature)||width!==320||height!==240)
    throw Error("Use the prepared 320×240 GlowDeck GIF");
  const data=new FormData();data.append("goal",file);
  const res=await fetch("/api/goal-gif/upload",{method:"POST",body:data});
  const result=await res.json();if(!res.ok)throw Error(result.error||"Upload failed");
  await loadGoalGif();
  message("Hockey goal animation installed");
})};
$("goalGifPreview").onclick=()=>perform(async()=>{
  await post("/api/overlay/preview",{kind:"goal",side:"home"});
  message("Hockey goal animation preview started on the S3");
});
$("goalGifRemove").onclick=()=>perform(async()=>{
  if(!confirm("Remove the installed hockey-goal animation?"))return;
  await post("/api/goal-gif/remove",{});
  await loadGoalGif();message("Goal animation removed");
});
$("uploadForm").onsubmit=e=>{e.preventDefault();perform(async()=>{
  const file=$("bin").files[0];if(!file)return;
  if(!confirm("Flash "+file.name+" and restart GameDay?"))return;
  const data=new FormData();data.append("firmware",file);
  const res=await fetch("/api/update",{method:"POST",body:data});
  const text=await res.text();if(!res.ok)throw new Error(text);
  message(text);
})};
$("reboot").onclick=()=>perform(async()=>{
  if(!confirm("Restart this GameDay device now?"))return;
  await api("/api/reboot",{method:"POST"});message("Restart requested");
});
(async()=>{
  try{
    mountPortalModules();
    trackPortalChanges();
    ["zone-favorites","lab-screen","zone-network"].forEach(jumpToZone);
    await loadConfig();
    await refresh();
    await loadGoalGif();
    await loadSoundLibrary();
    await loadS3CaseLeds();
    setPortalDirty(false);
    const initial=location.hash.slice(1);
    if(initial && PORTAL_ROUTES[initial] && initial!=="dashboard")
      tab(initial);
  }catch(err){message("Cannot load device: "+err.message)}
  setInterval(()=>refresh().catch(()=>{}),15000);
})();
</script>
</body>
</html>
)GLOWDECK";
