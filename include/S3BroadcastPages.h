#pragma once
// Included INSIDE the S3-only render namespace, after the Phase 5 renderers.
// Phase 6C: new scene renderers only. Flag OFF retains every old renderer.
// No network calls, no independent framebuffer, no new state, no touch changes.
#include "S3BroadcastPagesPolicy.h"

constexpr uint16_t S3C_BG=S3BroadcastPages::kBackground;
constexpr uint16_t S3C_SURFACE=S3BroadcastPages::kSurface;
constexpr uint16_t S3C_ALT=S3BroadcastPages::kSurfaceAlt;
constexpr uint16_t S3C_RAIL=S3BroadcastPages::kRail;
constexpr uint16_t S3C_ACCENT=S3BroadcastPages::kAccent;
constexpr uint16_t S3C_MUTED=S3BroadcastPages::kMuted;

void drawS3CCanvas(){
  // Opaque in the ONE native TFT_eSprite; no PSRAM allocation or picture
  // decoding on pages that do not need the Live Game arena plinths.
  tft.fillScreen(S3C_BG);
  constexpr uint16_t bands[7]={
    0x0842,0x0842,0x0862,0x1062,0x0842,0x0822,0x0842};
  for(uint8_t i=0;i<7;++i)
    tft.fillRect(0,uint16_t(i)*40,480,40,bands[i]);
  tft.fillRect(0,280,480,40,S3C_BG);
  // The understated cross-arena light planes extend visually into the
  // live scoreboard without becoming borders around individual cards.
  tft.drawLine(0,52,168,0,0x10A3);
  tft.drawLine(479,52,316,0,0x10A3);
  tft.drawFastHLine(17,49,446,0x18A4);
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
}

void drawS3CText(String label,int16_t x,int16_t y,
                 int16_t maxWidth,uint8_t font,uint16_t color){
  if(maxWidth<=0)return;
  if(tft.textWidth(label,font)>maxWidth){
    const String ellipsis="...";
    while(label.length() && tft.textWidth(label+ellipsis,font)>maxWidth)
      label.remove(label.length()-1);
    if(!label.length())return;
    label+=ellipsis;
  }
  tft.setTextColor(color); // no opaque glyph boxes on a textured canvas
  tft.drawString(label,x,y,font);
}

void drawS3CStar(const Game &g,int16_t x,int16_t y){
  // Exactly the existing hit-map, follow state and armed semantics; only
  // the frameless visual treatment changes. Restore text datum for nav.
  const uint8_t previousDatum=tft.getTextDatum();
  const bool followed=rearFollows(g.id);
  const uint16_t color=followed?0xFE60:0xCE79;
  const int8_t px[10]={0,3,10,5,6,0,-6,-5,-10,-3};
  const int8_t py[10]={-11,-3,-3,2,10,6,10,2,-3,-3};
  for(uint8_t i=0;i<10;++i){
    const uint8_t j=(i+1)%10;
    if(followed)
      tft.fillTriangle(x,y,x+px[i],y+py[i],x+px[j],y+py[j],color);
    tft.drawLine(x+px[i],y+py[i],x+px[j],y+py[j],color);
  }
  if(g.id==selectedGameId){
    tft.setTextDatum(MC_DATUM);
    drawS3CText("A",x+15,y-8,12,1,S3C_ACCENT);
  }
  tft.setTextDatum(previousDatum);
}

void drawS3CHeader(const Game *g,const String &title){
  tft.setTextDatum(ML_DATUM);
  drawS3CText(title,17,24,234,2,WHITE);
  tft.fillRect(17,44,40,2,S3C_ACCENT);
  if(g){
    const uint16_t statusColor=gameIsLive(*g)?0xF9C3:
        gameIsFuture(*g)?S3C_ACCENT:S3C_MUTED;
    tft.setTextDatum(MC_DATUM);
    drawS3CText(gameIsLive(*g)?"LIVE":gameIsFuture(*g)?"NEXT":"FINAL",
                305,24,62,2,statusColor);
    drawS3CStar(*g,399,24);
  }else{
    tft.setTextDatum(MC_DATUM);
    drawS3CText("NHL",315,24,45,2,S3C_MUTED);
  }
  // The top-right menu retains its original 428,3,49,42 hit rectangle.
  const auto menu=S3NativeLayout::kMenu;
  for(uint8_t i=0;i<3;++i)
    tft.drawFastHLine(menu.x+14,menu.y+13+i*8,21,S3C_MUTED);
  tft.setTextDatum(TL_DATUM);
}

void drawS3CFooter(const char *left,const char *center,const char *right,
                   bool activeCenter=false){
  const auto a=S3NativeLayout::kPrev;
  const auto b=S3NativeLayout::kGames;
  const auto c=S3NativeLayout::kNext;
  tft.fillRect(0,280,480,40,S3C_BG);
  tft.drawFastHLine(18,278,444,S3C_RAIL);
  // Floating labels sit inside the UNCHANGED three capacitive touch targets.
  tft.setTextDatum(MC_DATUM);
  const auto drawAction=[&](const char* label,int16_t x,uint16_t color){
    if(!strcmp(label,"PREV"))
      GameDayIcons::draw(tft,GameDayIcons::Kind::PREVIOUS,x,296,19,color);
    else if(!strcmp(label,"NEXT"))
      GameDayIcons::draw(tft,GameDayIcons::Kind::NEXT,x,296,19,color);
    else if(!strcmp(label,"GAMES")||!strcmp(label,"ALL GAMES"))
      GameDayIcons::draw(tft,GameDayIcons::Kind::GAMES,x,296,19,color);
    else if(!strcmp(label,"SETUP"))
      GameDayIcons::draw(tft,GameDayIcons::Kind::SETUP,x,296,19,color);
    else {
      tft.setTextColor(color);
      tft.drawString(label,x,296,2);
    }
  };
  drawAction(left,a.x+a.w/2,S3C_MUTED);
  drawAction(right,c.x+c.w/2,S3C_MUTED);
  drawAction(center,b.x+b.w/2,activeCenter?0xFE60:S3C_ACCENT);
  tft.setTextDatum(TL_DATUM);
}

void drawS3CGameRow(const Game &g,uint8_t row){
  const auto r=S3NativeLayout::summaryRow(row);
  const uint16_t plate=(row&1)?S3C_SURFACE:S3C_ALT;
  tft.fillRect(r.x,r.y+1,r.w,r.h-2,plate);
  tft.fillRect(r.x,r.y+9,3,r.h-18,
               isArmedGame(g.id)?S3C_ACCENT:to565(teamRgb(g.away)));
  tft.fillRect(r.x+r.w-4,r.y+9,2,r.h-18,to565(teamRgb(g.home)));
  drawTeamLogo(tft,g.away,24,r.y+9,28);
  drawTeamLogo(tft,g.home,372,r.y+9,28);
  tft.setTextDatum(MC_DATUM);
  drawS3CText(g.away,100,r.y+16,82,4,WHITE);
  drawS3CText(g.home,342,r.y+16,80,4,WHITE);
  if(gameIsFuture(g)){
    drawS3CText("VS",220,r.y+17,65,4,S3C_ACCENT);
  }else{
    drawS3CText(String(g.awayScore),176,r.y+15,44,4,WHITE);
    drawS3CText("-",220,r.y+17,16,2,S3C_MUTED);
    drawS3CText(String(g.homeScore),264,r.y+15,44,4,WHITE);
  }
  const uint8_t rank=armPriority(g.id);
  const String status=(gameIsFuture(g)?formattedStart(g.startUTC):gameStatus(g));
  drawS3CText(status,205,r.y+38,170,1,S3C_MUTED);
  const uint16_t fill=rank?S3C_ACCENT:S3C_SURFACE;
  tft.fillRoundRect(302,r.y+28,56,20,4,fill);
  drawS3CText(rank?"A"+String(rank):
      armedGames.count()>=MultiGameArming::kMaxArmed?String("FULL"):String("ARM"),
      330,r.y+38,52,1,rank?S3C_BG:WHITE);
  drawS3CStar(g,444,r.y+24);
  tft.setTextDatum(TL_DATUM);
}

void drawS3CGameList(bool picker){
  const LiveCarousel::Page slide=currentCarouselPage();
  if(picker && gameCount && uint16_t(gamePage)*4>=gameCount)gamePage=0;
  drawS3CCanvas();
  const uint8_t pages=(gameCount+3)/4;
  drawS3CHeader(nullptr,picker?
      ("ALL GAMES  "+String(gamePage+1)+"/"+String(pages?pages:1)):
      String("OTHER GAMES"));
  for(uint8_t row=0;row<4;++row){
    const uint8_t index=picker?gamePage*4+row:
                         (row<slide.count?slide.rows[row]:255);
    if(index<gameCount)drawS3CGameRow(games[index],row);
  }
  if(!gameCount){
    tft.setTextDatum(MC_DATUM);
    drawS3CText("No games loaded yet",240,159,350,4,S3C_MUTED);
  }
  drawS3CFooter("PREV",picker?"TODAY":"ALL GAMES","NEXT");
}

void drawS3CStats(){
  drawS3CCanvas();
  const Game *g=detailGame();
  drawS3CHeader(g,"GAME STATS");
  if(!g){
    tft.setTextDatum(MC_DATUM);
    drawS3CText("Game unavailable",240,155,360,4,S3C_MUTED);
    drawS3CFooter("BACK","ARM GOALS","SETUP");
    return;
  }
  const uint16_t awayColor=to565(teamRgb(g->away));
  const uint16_t homeColor=to565(teamRgb(g->home));
  tft.fillRect(12,56,456,73,S3C_SURFACE);
  tft.fillRect(12,66,3,50,awayColor);
  tft.fillRect(465,66,3,50,homeColor);
  drawTeamLogo(tft,g->away,33,61,64);
  drawTeamLogo(tft,g->home,383,61,64);
  tft.setTextDatum(MC_DATUM);
  drawS3CText(g->away,142,86,76,4,WHITE);
  drawS3CText(g->home,337,86,76,4,WHITE);
  const String score=gameIsFuture(*g)?"VS":
      String(g->awayScore)+" - "+String(g->homeScore);
  drawS3CText(score,240,78,122,4,WHITE);
  drawS3CText(gameStatus(*g),240,113,226,2,S3C_ACCENT);
  const bool haveStats=detail.loaded && detail.gameId==g->id && detail.count>0;
  if(haveStats){
    for(uint8_t i=0;i<detail.count && i<5;++i){
      const auto r=S3BroadcastPages::statsRow(i);
      const uint16_t plate=(i&1)?S3C_SURFACE:S3C_ALT;
      tft.fillRect(r.x,r.y,r.w,r.h,plate);
      drawS3CText(detail.away[i],104,r.y+11,116,2,WHITE);
      drawS3CText(detail.label[i],240,r.y+10,140,2,S3C_MUTED);
      drawS3CText(detail.home[i],376,r.y+11,116,2,WHITE);
      if(!gameIsFuture(*g) &&
         s3ComparableRow(detail.label[i],detail.away[i],detail.home[i])){
        const float aw=detail.away[i].toFloat();
        const float hm=detail.home[i].toFloat();
        const float total=aw+hm;
        if(aw>=0 && hm>=0 && total>0){
          const uint16_t awPixels=uint16_t(80.0f*aw/total);
          const uint16_t hmPixels=uint16_t(80.0f*hm/total);
          tft.fillRect(54,r.y+22,80,2,S3C_RAIL);
          tft.fillRect(346,r.y+22,80,2,S3C_RAIL);
          if(awPixels)tft.fillRect(54,r.y+22,awPixels,2,awayColor);
          if(hmPixels)tft.fillRect(346,r.y+22,hmPixels,2,homeColor);
        }
      }
    }
  }else{
    tft.setTextDatum(MC_DATUM);
    drawS3CText(detail.loaded?
       (detail.error.length()?detail.error:String("No stats available")):
       String("Loading team stats..."),240,182,420,2,WHITE);
    drawS3CText(gameIsFuture(*g)?
       "Season comparison updates before puck drop":
       "NHL gamecenter data updates automatically",
       240,216,430,2,S3C_MUTED);
  }
  drawS3CFooter("BACK",isArmedGame(g->id)?"DISARM":
                armedGames.count()>=MultiGameArming::kMaxArmed?"MAX 5":"ARM",
                "SETUP",isArmedGame(g->id));
}

void drawS3CStandings(){
  drawS3CCanvas();
  const StandingsCarousel::Page page=StandingsCarousel::page(
      currentCarouselPage().index);
  drawS3CHeader(nullptr,"NHL STANDINGS");
  tft.setTextDatum(ML_DATUM);
  drawS3CText(page.title,17,65,310,2,WHITE);
  tft.setTextDatum(MR_DATUM);
  drawS3CText(page.range,464,65,120,2,S3C_ACCENT);
  tft.fillRect(12,83,456,20,S3C_SURFACE);
  tft.setTextDatum(MC_DATUM);
  drawS3CText("#",37,93,20,2,S3C_MUTED);
  drawS3CText("TEAM",109,93,76,2,S3C_MUTED);
  drawS3CText("GP",217,93,42,2,S3C_MUTED);
  drawS3CText("W-L-OT",331,93,112,2,S3C_MUTED);
  drawS3CText("PTS",438,93,42,2,S3C_MUTED);
  const String localDay=deviceLocalDate();
  const bool ready=standingsReady &&
                   (!localDay.length() || standingsForDate==localDay);
  if(!ready){
    drawS3CText(standingsLastAttempt?"Refreshing NHL standings...":
                 "Loading NHL standings...",240,164,420,2,WHITE);
    const String reason=standingsError.length()?standingsError:
                        String("Waiting for NHL standings data");
    drawS3CText(reason,240,201,410,2,S3C_MUTED);
  }else{
    uint8_t sorted[32]={};
    const uint8_t n=StandingsCarousel::collectAndSort(
       seasonStandings,32,page,sorted,32);
    const Game *armed=selectedGame();
    for(uint8_t row=0;row<S3BroadcastPages::kStandingsRowCount;++row){
      const uint8_t index=page.offset+row;
      if(index>=n)break;
      const auto &entry=seasonStandings[sorted[index]];
      const auto r=S3BroadcastPages::standingsRow(row);
      const uint16_t plate=(row&1)?S3C_SURFACE:S3C_ALT;
      tft.fillRect(r.x,r.y,r.w,r.h,plate);
      if(armed && (entry.team==armed->away || entry.team==armed->home))
        tft.fillRect(12,r.y,3,r.h,S3C_ACCENT);
      tft.fillRect(65,r.y+3,3,13,to565(teamRgb(entry.team)));
      drawTeamLogo(tft,entry.team,75,r.y+1,17);
      drawS3CText(String(StandingsCarousel::rank(
          page.group,entry.conferenceSequence,entry.divisionSequence)),
          37,r.y+10,25,2,WHITE);
      drawS3CText(entry.team,131,r.y+10,76,2,WHITE);
      drawS3CText(String(entry.gamesPlayed),217,r.y+10,40,2,S3C_MUTED);
      drawS3CText(entry.seasonRecord,331,r.y+10,106,2,S3C_MUTED);
      drawS3CText(entry.points.length()?entry.points:String("--"),
                  438,r.y+10,46,2,WHITE);
    }
    if(!n)drawS3CText("No ranked teams for this page",240,167,410,2,S3C_MUTED);
  }
  drawS3CFooter("PREV","ALL GAMES","NEXT");
}

void drawS3CSetup(){
  drawS3CCanvas();
  drawS3CHeader(nullptr,"DEVICE SETUP");
  tft.fillRect(12,56,456,145,S3C_SURFACE);
  tft.setTextDatum(ML_DATUM);
  if(WiFi.status()!=WL_CONNECTED){
    // Phase 6C is the default Waveshare S3 renderer. Unlike the old Phase 5
    // drawS3Setup(), this screen was missing its offline hotspot QR entirely.
    // Keep the same 3px modules, four-module quiet zone, SSID and password
    // used by the now hardware-verified CYD recovery screen.
    drawS3CText("HOME WI-FI OFFLINE",25,76,285,2,S3C_ACCENT);
    drawS3CText("Scan QR with your phone",25,105,295,2,WHITE);
    drawS3CText("AP: "+apSsid,25,132,295,2,WHITE);
    drawS3CText("Open 192.168.4.1",25,158,295,2,S3C_ACCENT);
    drawS3CText("KEY: "+apPassword,25,184,295,2,S3C_MUTED);
    // x=337 + (29 QR modules + eight-module quiet-zone) * 3 = 448.
    // The entire white 111x111 panel fits inside the 456x145 setup card.
    drawHotspotJoinQr(337,73,3);
  }else{
    drawS3CText("NETWORK",25,74,200,2,S3C_ACCENT);
    const String ssid=cfg.ssid.isEmpty()?String("Not configured"):cfg.ssid;
    drawS3CText("Wi-Fi: "+ssid,25,102,410,2,WHITE);
    drawS3CText("IP: "+localAddress(),25,124,420,2,WHITE);
    drawS3CText("GLOWDECK HOTSPOT",25,151,300,2,S3C_ACCENT);
    drawS3CText("AP: "+apSsid,25,175,235,2,WHITE);
    tft.setTextDatum(MR_DATUM);
    drawS3CText("KEY: "+apPassword,456,175,200,2,S3C_MUTED);
  }
  const S3NativeLayout::Rect b[3]={
    S3NativeLayout::kSetupTest,
    S3NativeLayout::kSetupTouchCheck,
    S3NativeLayout::kSetupColorMode};
  const char *labels[3]={"TEST LIGHTS","TOUCH CHECK","COLORS"};
  tft.setTextDatum(MC_DATUM);
  for(uint8_t i=0;i<3;++i){
    tft.fillRect(b[i].x,b[i].y,b[i].w,b[i].h,S3C_SURFACE);
    tft.fillRect(b[i].x+12,b[i].y+4,b[i].w-24,2,
                 i==2?S3C_ACCENT:S3C_RAIL);
    drawS3CText(labels[i],b[i].x+b[i].w/2,b[i].y+b[i].h/2,
                b[i].w-10,2,i==2?S3C_ACCENT:WHITE);
  }
  drawS3CFooter("TODAY","GAMES","BACK");
}

void drawS3CTouchCheck(){
  drawS3CCanvas();
  drawS3CHeader(nullptr,"TOUCH CHECK");
  tft.setTextDatum(MC_DATUM);
  drawS3CText("Tap all four targets to check alignment",240,57,420,2,S3C_MUTED);
  const auto r0=S3NativeLayout::kTouchCheckTopLeft;
  const auto r1=S3NativeLayout::kTouchCheckTopRight;
  const auto r2=S3NativeLayout::kTouchCheckBottomLeft;
  const auto r3=S3NativeLayout::kTouchCheckBottomRight;
  const S3NativeLayout::Rect r[4]={r0,r1,r2,r3};
  for(uint8_t i=0;i<4;++i){
    const bool detected=s3TouchCheckMask&(1U<<i);
    const uint16_t plate=detected?S3C_ACCENT:S3C_SURFACE;
    tft.fillRect(r[i].x,r[i].y,r[i].w,r[i].h,plate);
    drawS3CText(detected?"OK":"+",r[i].x+r[i].w/2,
                r[i].y+r[i].h/2,48,4,detected?S3C_BG:WHITE);
  }
  drawS3CText(s3TouchCheckMask==15?
      "All corners registered correctly":
      "Factory FT6336 mapping - no calibration required",
      240,142,450,2,s3TouchCheckMask==15?S3C_ACCENT:S3C_MUTED);
  if(s3TouchCheckX>=0)
    drawS3CText("Last touch: "+String(s3TouchCheckX)+", "+
                String(s3TouchCheckY),240,176,420,2,WHITE);
  drawS3CFooter("BACK","RESET","GAMES");
}

void drawS3CEmpty(){
  drawS3CCanvas();
  drawS3CHeader(nullptr,"NHL TODAY");
  tft.setTextDatum(MC_DATUM);
  drawS3CText("NHL GAMEDAY",240,116,400,4,WHITE);
  const String message=lastError.length()?lastError:
      scoreboardLoaded?String("No NHL games scheduled"):
                       String("Loading today's games...");
  drawS3CText(message,240,165,430,2,S3C_MUTED);
  drawS3CText(targetScoreboardDate.length()?
      targetScoreboardDate:scoreboardDate,240,200,300,2,S3C_ACCENT);
  drawS3CFooter("PREV","ALL GAMES","NEXT");
}

void drawS3CEventStatic(){
  if(!overlayController.active())return;
  const EventOverlay::Item &e=overlayController.current();
  drawS3CCanvas();
  const uint16_t club=TeamGradient::rgb565(
      e.color?e.color:0x31837AUL);
  tft.fillRect(14,56,4,200,club);
  tft.fillRect(462,56,4,200,club);
  tft.setTextDatum(ML_DATUM);
  drawS3CText("NHL / LIVE EVENT",18,24,250,2,S3C_MUTED);
  tft.fillRect(18,44,52,2,club);
  if(cfg.overlayLogos && e.team[0])
    drawTeamLogo(tft,String(e.team),37,89,78);
  else{
    tft.setTextDatum(MC_DATUM);
    drawS3CText(e.team[0]?String(e.team):String("NHL"),
                77,122,80,4,WHITE);
  }
  tft.setTextDatum(MC_DATUM);
  drawS3CText(EventOverlay::label(e.kind),280,95,275,4,WHITE);
  drawS3CText(s3OverlaySubtitle(e),280,135,315,2,S3C_MUTED);
  tft.drawFastHLine(39,164,402,S3C_RAIL);
  drawS3CText(s3OverlayScore(e),240,198,430,4,WHITE);
  String period=e.period?String("P")+String(e.period):String("");
  if(e.clock[0])period+=(period.length()?"  |  ":"")+String(e.clock);
  drawS3CText(period.length()?period:String("LIVE NHL"),
              240,238,300,2,S3C_ACCENT);
  // Event animation still owns the RGB565 perimeter and progress at 285.
  // The close hit remains 428,2,50,38.
  tft.drawLine(443,11,462,30,WHITE);
  tft.drawLine(462,11,443,30,WHITE);
  tft.setTextDatum(TL_DATUM);
}

void drawS3CGifBackdrop(const EventOverlay::Item &e){
  drawS3CCanvas();
  const uint16_t club=TeamGradient::rgb565(
      e.color?e.color:0x31837AUL);
  tft.fillRect(0,40,7,240,club);
  tft.fillRect(473,40,7,240,club);
  // Keep the original optional team-logo rail alongside the 1:1 GIF.
  if(cfg.overlayLogos && e.team[0])
    drawTeamLogo(tft,String(e.team),12,129,56);
  tft.setTextDatum(ML_DATUM);
  drawS3CText(String(e.team)+"  GOAL!",16,21,360,4,WHITE);
  // Exact 320x240 viewport preserved, including the four corner controls.
  const auto viewport=S3NativeLayout::kGifViewport;
  tft.fillRect(viewport.x,viewport.y,viewport.w,viewport.h,TFT_BLACK);
  tft.fillRect(0,281,480,35,S3C_BG);
  tft.drawLine(443,11,462,30,WHITE);
  tft.drawLine(462,11,443,30,WHITE);
  tft.setTextDatum(TL_DATUM);
}
