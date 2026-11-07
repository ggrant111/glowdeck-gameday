#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include "WledDdpPolicy.h"
#include "WledScenePolicy.h"
using namespace WledDdpPolicy;
static Layout layout(){
  Layout l;
  l.pixels=120;l.count=2;
  l.ranges[0]={0,60,0,WledZonePolicy::HOME};
  l.ranges[1]={60,120,1,WledZonePolicy::AWAY};
  return l;
}
int main(){
  static_assert(kPort==4048,"Official DDP UDP port");
  static_assert(kHeaderBytes==10,"DDP v1 10-byte header");
  static_assert(kPacketBytes<=1472,"Ethernet MTU-safe UDP payload");
  static_assert(kMaxPixels==4096 && kMaxConcurrent==2,"Bounded CYD budget");
  static_assert(kChunkPixels==480 && kMaxPackets==9,"WLED DDP chunks");
  assert(validFps(10)&&validFps(15)&&validFps(20));
  assert(!validFps(9)&&!validFps(21));
  Layout l=layout();
  assert(validate(l,3,0)==OK); // start: both teams share dedicated strip
  assert(validate(l,4,0)==OK); // final
  assert(validate(l,1,1)==NOT_TARGETED); // home goal MUST NOT erase away
  assert(validate(l,1,2)==NOT_TARGETED);
  assert(validate(l,2,1)==NOT_TARGETED); // live PP still uses native JSON
  assert(std::strcmp(reason(NOT_TARGETED),"Opposite team zone: native JSON fallback")==0);
  l.ranges[1].role=WledZonePolicy::UNASSIGNED;
  assert(validate(l,3,0)==UNASSIGNED);
  l=layout();l.ranges[1].start=61;
  assert(validate(l,3,0)==GAP_OR_OVERLAP);
  l=layout();l.ranges[1].start=59;
  assert(validate(l,3,0)==GAP_OR_OVERLAP);
  l=layout();l.ranges[1].stop=121;
  assert(validate(l,3,0)==BAD_RANGE);
  l=layout();l.ranges[1].id=0;
  assert(validate(l,3,0)==DUPLICATE_ID);
  l=layout();l.pixels=4097;
  assert(validate(l,3,0)==TOO_MANY_PIXELS);
  l=layout();l.pixels=0;
  assert(validate(l,3,0)==NO_PIXELS);
  // A dedicated controller can assign all pixels to a single HOME zone:
  // only eligible HOME goals can use DDP, AWAY goals must use JSON.
  l=layout();l.ranges[1].role=WledZonePolicy::HOME;
  assert(validate(l,1,1)==OK);
  assert(validate(l,1,2)==NOT_TARGETED);
  l=layout();
  Style styles[kMaxRanges];
  styles[0]={0xFF0000,0,128,128,255};
  styles[1]={0x0000FF,0,128,128,255};
  uint8_t packet[kPacketBytes+8];
  std::memset(packet,0xC3,sizeof(packet));
  size_t used=0;
  assert(frameChunk(l,styles,0,19,0,packet,sizeof(packet)-8,used));
  assert(used==kHeaderBytes+120*3);
  assert(packet[0]==0x41&&packet[1]==5); // 1..15, never zero
  assert(packet[2]==0x0B&&packet[3]==0x01);
  assert(packet[4]==0&&packet[5]==0&&packet[6]==0&&packet[7]==0);
  assert(packet[8]==1&&packet[9]==104); // 360 RGB channel bytes
  assert(packet[10]==255&&packet[11]==0&&packet[12]==0);
  assert(packet[10+3*59]==255);
  assert(packet[10+3*60]==0&&packet[10+3*60+1]==0&&
         packet[10+3*60+2]==255);
  assert(packet[used]==0xC3&&packet[used+7]==0xC3);
  assert(!frameChunk(l,styles,0,0,0,packet,used-1,used));
  assert(used==0);
  assert(!frameChunk(l,styles,0,0,1,packet,sizeof(packet),used));
  assert(!frameChunk(l,styles,0,0,480,packet,sizeof(packet),used));
  const uint16_t sizes[]={1,300,479,480,481,960,961,1000,2048,4096};
  for(uint16_t leds:sizes){
    Layout full;
    full.count=1;full.pixels=leds;
    full.ranges[0]={0,leds,0,WledZonePolicy::HOME};
    assert(validate(full,1,1)==OK);
    assert(packetCount(leds)==uint16_t((uint32_t(leds)+479)/480));
    uint16_t chunks=0;
    for(uint16_t first=0;first<leds;first=uint16_t(first+kChunkPixels)){
      std::memset(packet,0xC3,sizeof(packet));
      assert(frameChunk(full,styles,0,uint8_t(chunks+14),first,
                        packet,kPacketBytes,used));
      const uint16_t n=uint16_t(leds-first)>kChunkPixels?
                       kChunkPixels:uint16_t(leds-first);
      const uint16_t dataBytes=uint16_t(n*3U);
      const uint32_t byteOffset=uint32_t(first)*3U;
      assert(used==size_t(kHeaderBytes)+dataBytes && used<=1450);
      assert(packet[0]==(first+n==leds?0x41:0x40));
      assert(packet[1]==uint8_t((chunks+14)%15+1));
      assert(packet[2]==0x0B&&packet[3]==1);
      assert(packet[4]==uint8_t(byteOffset>>24));
      assert(packet[5]==uint8_t(byteOffset>>16));
      assert(packet[6]==uint8_t(byteOffset>>8));
      assert(packet[7]==uint8_t(byteOffset));
      assert(packet[8]==uint8_t(dataBytes>>8));
      assert(packet[9]==uint8_t(dataBytes));
      assert(packet[10]==255&&packet[11]==0&&packet[12]==0);
      assert(packet[used]==0xC3&&packet[sizeof(packet)-1]==0xC3);
      ++chunks;
    }
    assert(chunks==packetCount(leds));
  }
  assert(packetCount(0)==0);
  // A WLED segment boundary can fall inside the middle 480-pixel chunk.
  Layout mixed;
  mixed.count=2;mixed.pixels=1000;
  mixed.ranges[0]={0,500,0,WledZonePolicy::HOME};
  mixed.ranges[1]={500,1000,1,WledZonePolicy::AWAY};
  assert(validate(mixed,3,0)==OK);
  assert(frameChunk(mixed,styles,0,0,480,packet,kPacketBytes,used));
  assert(packet[0]==0x40);
  assert(packet[10]==255&&packet[11]==0&&packet[12]==0);
  assert(packet[10+19*3]==255); // pixel 499 home
  assert(packet[10+20*3]==0&&packet[10+20*3+2]==255); // pixel 500 away
  assert(frameChunk(mixed,styles,0,0,960,packet,kPacketBytes,used));
  assert(packet[0]==0x41&&used==kHeaderBytes+40*3);
  assert(packet[10]==0&&packet[12]==255);
  mixed.ranges[1].start=501; // uncovered LED must fail, not send stale data
  assert(!frameChunk(mixed,styles,0,0,480,packet,kPacketBytes,used));
  assert(used==0);
  // Pixel brightness is proportional and never wraps.
  Style dim={0xFFFFFF,0,128,128,128};
  const uint32_t white=pixelColor(dim,0,10,0);
  assert((white&255)<=128 && (white&255)>=127);
  assert(scale(255,255)==255&&scale(255,128)==128);
  const Style fx[]={{0xEE5522,0,128,128,192},
                    {0xEE5522,2,128,128,192},
                    {0xEE5522,3,128,128,192},
                    {0xEE5522,12,128,128,192},
                    {0xEE5522,17,128,128,192},
                    {0xEE5522,23,128,128,192},
                    {0xEE5522,28,128,128,192}};
  for(const auto &style:fx)
    for(uint32_t time=0;time<60000;time+=53){
      const uint32_t col=pixelColor(style,4,30,time);
      assert((col>>24)==0);
      assert((col&255)<=0x22);
      assert(((col>>8)&255)<=0x55);
      assert(((col>>16)&255)<=0xEE);
    }
  std::cout<<"PASS WLED 8D: chunked DDP v1 RGB24, "+
    std::to_string(kMaxPixels)+" pixels, 480-pixel packets, whole-strip ownership, "+
    "home/away JSON fallback, bounded FPS and color envelopes\n";
}
