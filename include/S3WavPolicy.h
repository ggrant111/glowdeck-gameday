#pragma once
#include <stdint.h>
#include <stddef.h>

// Phase 7B: deliberately narrow, bounded RIFF/WAVE PCM decoder.
// Host-testable without Arduino, networking, heap, or filesystem globals.
// Accepts uncompressed PCM 16-bit mono/stereo at 8–48 kHz and performs
// bounded linear resampling to the existing 48kHz ES8311 I2S output.
namespace S3WavPolicy {
constexpr uint8_t kSlots=3;
constexpr uint32_t kMaxBytes=768UL*1024UL;
constexpr uint32_t kMaxDurationMs=10000UL;
constexpr uint32_t kMaxHeaderScan=4096UL;
constexpr uint32_t kOutputRate=48000UL;
constexpr size_t kInputBufferBytes=512; // 128 stereo PCM frames per flash read
struct Info {
  uint32_t dataOffset=0, dataBytes=0, sampleRate=0, frames=0, outputFrames=0;
  uint32_t durationMs=0;
  uint16_t channels=0, bits=0;
  bool valid=false;
};
inline uint16_t u16(const uint8_t *p) {
  return uint16_t(p[0]) | (uint16_t(p[1])<<8);
}
inline uint32_t u32(const uint8_t *p) {
  return uint32_t(p[0]) | (uint32_t(p[1])<<8) |
         (uint32_t(p[2])<<16) | (uint32_t(p[3])<<24);
}
inline bool fourcc(const uint8_t *p,const char *s){
  return p[0]==uint8_t(s[0])&&p[1]==uint8_t(s[1])&&
         p[2]==uint8_t(s[2])&&p[3]==uint8_t(s[3]);
}
inline bool validSlot(uint8_t slot){return slot>=1 && slot<=kSlots;}
// Reader implements size(), seek(uint32_t) and read(uint8_t*,size_t).
// All integer arithmetic used for file offsets is overflow-checked.
template<typename Reader>
bool parse(Reader &file,Info &info){
  info=Info{};
  const uint64_t size=file.size();
  if(size<44 || size>kMaxBytes)return false;
  uint8_t riff[12]={};
  if(!file.seek(0)||file.read(riff,12)!=12 ||
     !fourcc(riff,"RIFF")||!fourcc(riff+8,"WAVE"))return false;
  const uint64_t riffEnd=uint64_t(u32(riff+4))+8ULL;
  if(riffEnd>size || riffEnd<44)return false;
  bool fmtSeen=false,dataSeen=false;
  uint32_t pos=12;
  uint16_t blockAlign=0;
  while(uint64_t(pos)+8ULL<=riffEnd &&
        pos<kMaxHeaderScan){
    uint8_t chunk[8]={};
    if(!file.seek(pos)||file.read(chunk,8)!=8)return false;
    const uint32_t len=u32(chunk+4);
    const uint64_t start=uint64_t(pos)+8ULL;
    const uint64_t end=start+uint64_t(len);
    if(end>riffEnd)return false;
    if(fourcc(chunk,"fmt ") && !fmtSeen){
      if(len<16 || len>256)return false;
      uint8_t fmt[16]={};
      if(!file.seek(uint32_t(start))||file.read(fmt,16)!=16)return false;
      if(u16(fmt)!=1)return false; // no compressed/float/extensible WAV
      info.channels=u16(fmt+2);
      info.sampleRate=u32(fmt+4);
      blockAlign=u16(fmt+12);
      info.bits=u16(fmt+14);
      if((info.channels!=1 && info.channels!=2) ||
         info.bits!=16 ||
         info.sampleRate<8000 || info.sampleRate>kOutputRate ||
         blockAlign!=info.channels*2 ||
         u32(fmt+8)!=info.sampleRate*blockAlign)return false;
      fmtSeen=true;
    }else if(fourcc(chunk,"data") && !dataSeen){
      info.dataOffset=uint32_t(start);
      info.dataBytes=len;
      dataSeen=true;
    }
    const uint64_t next=end+(len&1U);
    if(next>riffEnd || next>UINT32_MAX)return false;
    pos=uint32_t(next);
    if(fmtSeen&&dataSeen)break;
  }
  if(!fmtSeen || !dataSeen || !info.dataBytes ||
     (info.dataBytes%blockAlign)!=0)return false;
  info.frames=info.dataBytes/blockAlign;
  const uint64_t ms=uint64_t(info.frames)*1000ULL/info.sampleRate;
  if(!info.frames || ms>kMaxDurationMs)return false;
  info.durationMs=uint32_t(ms);
  const uint64_t out=(uint64_t(info.frames)*kOutputRate+
                      info.sampleRate-1ULL)/info.sampleRate;
  if(!out || out>kOutputRate*kMaxDurationMs/1000UL)return false;
  info.outputFrames=uint32_t(out);
  info.valid=true;
  return true;
}
inline int16_t readPcm16(const uint8_t *p){
  return static_cast<int16_t>(u16(p));
}
// Filesystem-independent streaming resampler. No large PCM allocations,
// no repeated seeks per output sample, no unsupported audio codecs.
template<typename Reader>
class Stream {
 public:
  bool begin(Reader &file,const Info &meta){
    if(!meta.valid || !file.seek(meta.dataOffset))return false;
    f_=&file;meta_=meta;phase_=0;inputIndex_=0;outputIndex_=0;
    buffered_=0;bufferPos_=0;consumedBytes_=0;
    if(!nextInput(current_))return false;
    if(meta.frames>1) {
      if(!nextInput(next_))return false;
    }else next_=current_;
    return true;
  }
  bool next(int16_t &out){
    if(!f_ || outputIndex_>=meta_.outputFrames)return false;
    const int32_t value=int32_t(current_)+
      int32_t((int64_t(next_)-int64_t(current_))*int64_t(phase_)/
              int64_t(kOutputRate));
    int32_t v=value;
    // 8-ms ramp at both boundaries prevents clicks from arbitrary WAVs.
    constexpr uint32_t ramp=kOutputRate*8UL/1000UL;
    if(outputIndex_<ramp)v=v*int32_t(outputIndex_)/int32_t(ramp);
    if(meta_.outputFrames-outputIndex_<ramp)
      v=v*int32_t(meta_.outputFrames-outputIndex_)/int32_t(ramp);
    out=static_cast<int16_t>(v);
    ++outputIndex_;
    phase_+=meta_.sampleRate;
    if(phase_>=kOutputRate){
      phase_-=kOutputRate;
      current_=next_;
      if(inputIndex_<meta_.frames){
        if(!nextInput(next_))return false;
      }else next_=current_;
    }
    return true;
  }
 private:
  bool nextInput(int16_t &result){
    if(!f_ || inputIndex_>=meta_.frames)return false;
    const size_t n=meta_.channels*2U;
    if(bufferPos_+n>buffered_){
      const uint32_t remain=meta_.dataBytes-consumedBytes_;
      if(remain<n)return false;
      // 512 is divisible by 2/4; each flash read holds whole PCM
      // samples, avoiding 48,000 tiny LittleFS reads per second.
      const size_t count=remain<kInputBufferBytes?remain:kInputBufferBytes;
      if(f_->read(buffer_,count)!=count)return false;
      consumedBytes_+=count;
      buffered_=count;
      bufferPos_=0;
    }
    const uint8_t *pcm=buffer_+bufferPos_;
    if(meta_.channels==1)result=readPcm16(pcm);
    else result=static_cast<int16_t>(
        (int32_t(readPcm16(pcm))+int32_t(readPcm16(pcm+2)))/2);
    bufferPos_+=n;
    ++inputIndex_;
    return true;
  }
  Reader *f_=nullptr;
  Info meta_{};
  uint32_t inputIndex_=0,outputIndex_=0,phase_=0,consumedBytes_=0;
  size_t buffered_=0,bufferPos_=0;
  uint8_t buffer_[kInputBufferBytes]={};
  int16_t current_=0,next_=0;
};
} // namespace S3WavPolicy
