#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>
#include "S3WavPolicy.h"

using namespace S3WavPolicy;
struct Reader {
  std::vector<uint8_t> b;
  size_t pos=0;
  uint32_t reads=0;
  uint32_t size()const{return static_cast<uint32_t>(b.size());}
  bool seek(uint32_t p){
    if(p>b.size())return false;
    pos=p;return true;
  }
  size_t read(uint8_t *out,size_t n){
    ++reads;
    if(n>b.size()-pos)return 0;
    std::memcpy(out,b.data()+pos,n);
    pos+=n;return n;
  }
};
static void w16(std::vector<uint8_t> &b,uint16_t v) {
  b.push_back(uint8_t(v));b.push_back(uint8_t(v>>8));
}
static void w32(std::vector<uint8_t> &b,uint32_t v) {
  for(int i=0;i<4;++i)b.push_back(uint8_t(v>>(i*8)));
}
static void str(std::vector<uint8_t> &b,const char *s){
  for(int i=0;i<4;++i)b.push_back(uint8_t(s[i]));
}
static Reader wav(uint32_t rate=48000,uint16_t channels=1,
                  uint16_t format=1,bool junk=false){
  Reader f;
  str(f.b,"RIFF");w32(f.b,0);str(f.b,"WAVE");
  if(junk){
    str(f.b,"JUNK");w32(f.b,3);
    f.b.push_back(3);f.b.push_back(2);f.b.push_back(1);f.b.push_back(0);
  }
  str(f.b,"fmt ");w32(f.b,16);w16(f.b,format);
  w16(f.b,channels);w32(f.b,rate);
  w32(f.b,rate*channels*2);w16(f.b,channels*2);w16(f.b,16);
  str(f.b,"data");w32(f.b,channels*8);
  const int16_t test[4]={-32768,32767,-32768,32767};
  for(int i=0;i<4;++i){
    w16(f.b,static_cast<uint16_t>(test[i]));
    if(channels==2)w16(f.b,static_cast<uint16_t>(test[i]));
  }
  const uint32_t total=static_cast<uint32_t>(f.b.size())-8;
  for(int i=0;i<4;++i)f.b[4+i]=uint8_t(total>>(8*i));
  return f;
}
static void valid(uint32_t rate,uint16_t channels,bool junk){
  Reader f=wav(rate,channels,1,junk);
  Info meta;
  assert(parse(f,meta));
  assert(meta.valid && meta.channels==channels && meta.bits==16);
  assert(meta.sampleRate==rate && meta.frames==4);
  assert(meta.dataBytes==4u*channels*2u);
  assert(meta.dataOffset==(junk?56u:44u));
  assert(meta.outputFrames==(4ULL*48000+rate-1)/rate);
  Stream<Reader> stream;
  assert(stream.begin(f,meta));
  for(uint32_t i=0;i<meta.outputFrames;++i){
    int16_t value=0;
    assert(stream.next(value));
    assert(value>=-32768 && value<=32767);
  }
  int16_t extra=1;
  assert(!stream.next(extra));
}
int main(){
  assert(validSlot(1)&&validSlot(2)&&validSlot(3)&&validSlot(4));
  assert(!validSlot(0)&&!validSlot(5));
  valid(48000,1,false);valid(44100,2,true);
  valid(22050,1,true);valid(8000,2,false);
  valid(16000,1,false);valid(32000,2,false);
  Reader file=wav();
  Info info;
  file.b[0]='X';assert(!parse(file,info));
  file=wav();file.b[8]='X';assert(!parse(file,info));
  file=wav(48000,1,3);assert(!parse(file,info)); // float unsupported
  file=wav(48000,3);assert(!parse(file,info));
  file=wav(96000);assert(!parse(file,info));
  file=wav(4000);assert(!parse(file,info));
  file=wav();file.b[34]=8;file.b[35]=0;assert(!parse(file,info));
  file=wav();file.b[32]=3;assert(!parse(file,info));
  file=wav();file.b[28]^=1;assert(!parse(file,info));
  file=wav();file.b[40]=255;assert(!parse(file,info));
  file=wav();file.b.resize(46);assert(!parse(file,info));
  file=wav();file.b[4]=0xFF;file.b[5]=0xFF;assert(!parse(file,info));
  file=wav();file.b.resize(kMaxBytes+1);assert(!parse(file,info));
  file=wav();
  // The resampler should keep a nonzero first-section output (the
  // 8ms fade naturally zeroes very short clips; use a long realistic
  // fixture to test decoding and interpolation in the middle).
  Reader large=wav(44100,2);
  const size_t dataOffset=44;
  large.b.resize(dataOffset+44100*4);
  for(size_t p=dataOffset;p<large.b.size();p+=4){
    const uint16_t x=((p/4)%13)<6?0x7fff:0x8000;
    large.b[p]=uint8_t(x);large.b[p+1]=uint8_t(x>>8);
    large.b[p+2]=uint8_t(x);large.b[p+3]=uint8_t(x>>8);
  }
  const uint32_t bytes=uint32_t(large.b.size()-dataOffset);
  for(int i=0;i<4;++i){
    large.b[40+i]=uint8_t(bytes>>(8*i));
    large.b[4+i]=uint8_t((large.b.size()-8)>>(8*i));
  }
  assert(parse(large,info)&&info.outputFrames==48000);
  Stream<Reader> stream;
  const uint32_t beforeReads=large.reads;
  assert(stream.begin(large,info));
  int16_t sample=0;
  int32_t maximum=0;
  for(uint32_t i=0;i<info.outputFrames;++i){
    assert(stream.next(sample));
    const int32_t absolute=sample<0?-int32_t(sample):sample;
    if(absolute>maximum)maximum=absolute;
  }
  assert(maximum>30000);assert(!stream.next(sample));
  // 44.1k stereo 1-second audio must read in ~345 buffered chunks,
  // NOT 44,100 per-sample LittleFS operations while I2S is active.
  assert(large.reads-beforeReads<500);
  std::cout<<"PASS S3 7B PCM RIFF parser, 8/16/22.05/32/44.1/48kHz, mono/stereo, "
              "bounded resampler, corrupt headers and truncated files\n";
}
