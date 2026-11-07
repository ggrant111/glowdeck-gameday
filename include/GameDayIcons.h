#pragma once
#include <stdint.h>

// Small vector icons shared by CYD TFT_eSPI and S3 native display.
// Uses drawing primitives only: no bitmap RAM, glyph font, SVG or SPI asset.
namespace GameDayIcons {
enum class Kind : uint8_t { GAMES, SETUP, PREVIOUS, NEXT, PAUSE, PLAY };
template <typename Display>
void draw(Display &tft,Kind kind,int16_t x,int16_t y,
          int16_t size,uint16_t color){
  const int16_t h=size/2;
  switch(kind){
    case Kind::GAMES: {
      const int16_t s=size/3;
      for(int16_t row=0;row<2;++row)
        for(int16_t col=0;col<2;++col)
          tft.fillRoundRect(x-h+col*(s+3),y-h+row*(s+3),s,s,1,color);
      break;
    }
    case Kind::SETUP: {
      // Pixel ring instead of drawCircle/drawLine: the S3 framebuffer
      // deliberately shadows only sprite-safe primitives.
      int16_t dx=h-4,dy=0,err=1-dx;
      while(dx>=dy){
        tft.drawPixel(x+dx,y+dy,color);
        tft.drawPixel(x+dy,y+dx,color);
        tft.drawPixel(x-dy,y+dx,color);
        tft.drawPixel(x-dx,y+dy,color);
        tft.drawPixel(x-dx,y-dy,color);
        tft.drawPixel(x-dy,y-dx,color);
        tft.drawPixel(x+dy,y-dx,color);
        tft.drawPixel(x+dx,y-dy,color);
        ++dy;
        if(err<0)err+=2*dy+1;
        else{--dx;err+=2*(dy-dx)+1;}
      }
      tft.fillCircle(x,y,size/6,color);
      tft.drawFastHLine(x-h,y,3,color);
      tft.drawFastHLine(x+h-2,y,3,color);
      tft.drawFastVLine(x,y-h,3,color);
      tft.drawFastVLine(x,y+h-2,3,color);
      break;
    }
    case Kind::PREVIOUS:
      tft.fillTriangle(x-h/2,y,x+h/3,y-h,x+h/3,y+h,color);
      tft.drawFastHLine(x,y,h,color);
      break;
    case Kind::NEXT:
      tft.fillTriangle(x+h/2,y,x-h/3,y-h,x-h/3,y+h,color);
      tft.drawFastHLine(x-h,y,h,color);
      break;
    case Kind::PAUSE:
      tft.fillRoundRect(x-h/2-3,y-h,4,size,1,color);
      tft.fillRoundRect(x+h/2,y-h,4,size,1,color);
      break;
    case Kind::PLAY:
      tft.fillTriangle(x-h/2,y-h,x-h/2,y+h,x+h/2+3,y,color);
      break;
  }
}
} // namespace GameDayIcons
