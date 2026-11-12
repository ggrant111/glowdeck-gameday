#include <cassert>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include "GameDayImprovSerial.h"

FakeWiFi WiFi;
class FakeStream : public Stream {
 public:
  std::vector<uint8_t> input,output;
  size_t pos=0;
  int available() override{return static_cast<int>(input.size()-pos);}
  int read() override{return pos<input.size()?input[pos++]:-1;}
  size_t write(uint8_t b) override{output.push_back(b);return 1;}
  size_t write(const uint8_t *b,size_t n) override{
    output.insert(output.end(),b,b+n);return n;
  }
  void feed(const std::vector<uint8_t> &bytes){
    input.insert(input.end(),bytes.begin(),bytes.end());
  }
  void clearOutput(){output.clear();}
};
std::vector<uint8_t> frame(uint8_t type,const std::vector<uint8_t> &data){
  std::vector<uint8_t> out={'I','M','P','R','O','V',1,type,
                            static_cast<uint8_t>(data.size())};
  out.insert(out.end(),data.begin(),data.end());
  uint8_t sum=0;
  for(uint8_t b:out)sum=static_cast<uint8_t>(sum+b);
  out.push_back(sum);
  return out;
}
std::vector<std::vector<uint8_t>> decode(const std::vector<uint8_t> &in){
  std::vector<std::vector<uint8_t>> out;
  size_t pos=0;
  while(pos<in.size()){
    assert(in.size()-pos>=10);
    assert(std::memcmp(in.data()+pos,"IMPROV",6)==0);
    assert(in[pos+6]==1);
    const auto len=in[pos+8];
    const size_t total=10+len;
    assert(pos+total<=in.size());
    uint8_t sum=0;
    for(size_t i=0;i<total-1;++i)sum=static_cast<uint8_t>(sum+in[pos+i]);
    assert(sum==in[pos+total-1]);
    out.emplace_back(in.begin()+pos,in.begin()+pos+total);
    pos+=total;
  }
  return out;
}
static int saved=0;
static std::string lastSsid,lastPassword;
bool tryConnect(const char *ssid,const char *pw){
  lastSsid=ssid;
  lastPassword=pw;
  if(lastPassword=="correct-pass"){WiFi.currentStatus=WL_CONNECTED;return true;}
  return false;
}
void connected(const char *ssid,const char *pw){
  assert(std::string(ssid)=="MyWiFi");
  assert(std::string(pw)=="correct-pass");
  ++saved;
}
void feedAndProcess(GameDayImprovSerial &service,FakeStream &io,
                    const std::vector<uint8_t> &data){
  io.feed(data);
  while(io.available())service.handleSerial();
}
int main(){
  FakeStream io;
  GameDayImprovSerial service(io);
  service.setDeviceInfo(GameDayImprovSerial::ESP32,
       "GlowDeck GameDay","v2026.11.12","CYD","http://{LOCAL_IPV4}/");
  service.setCustomConnectWiFi(tryConnect);
  service.onImprovConnected(connected);

  // An installer sends GET_CURRENT_STATE then GET_DEVICE_INFO in immediate
  // successive frames. The upstream library loses the second header.
  feedAndProcess(service,io,frame(3,{2,0}));
  auto packets=decode(io.output);
  assert(packets.size()>=2);
  assert(packets.back()[7]==1 && packets.back()[9]==2);
  io.clearOutput();
  feedAndProcess(service,io,frame(3,{3,0}));
  packets=decode(io.output);
  assert(packets.size()==2);
  assert(packets.back()[7]==4 && packets.back()[9]==3);
  io.clearOutput();

  // An RPC may be fragmented across short serial read cycles.
  auto scan=frame(3,{4,0});
  for(uint8_t byte:scan){
    io.feed({byte});
    service.handleSerial();
  }
  packets=decode(io.output);
  assert(packets.size()==3); // error reset, result with SSID, scan terminator
  assert(packets.back()[7]==4 && packets.back()[9]==4 && packets.back()[10]==0);
  io.clearOutput();

  const std::string ssid="MyWiFi",pw="correct-pass";
  std::vector<uint8_t> payload={1,static_cast<uint8_t>(ssid.size()+pw.size()+2),
     static_cast<uint8_t>(ssid.size())};
  payload.insert(payload.end(),ssid.begin(),ssid.end());
  payload.push_back(static_cast<uint8_t>(pw.size()));
  payload.insert(payload.end(),pw.begin(),pw.end());
  feedAndProcess(service,io,frame(3,payload));
  packets=decode(io.output);
  assert(packets.size()>=4);
  assert(packets.back()[7]==4 && packets.back()[9]==1);
  std::string raw(packets.back().begin(),packets.back().end());
  assert(raw.find("http://192.168.1.77/")!=std::string::npos);
  assert(saved==1 && lastSsid=="MyWiFi");
  io.clearOutput();

  // Provisioned state is discoverable after reconnect without reinstalling.
  feedAndProcess(service,io,frame(3,{2,0}));
  packets=decode(io.output);
  assert(packets.back()[7]==4 && packets.back()[9]==2);
  io.clearOutput();

  // Invalid checksum does not modify credentials and parser recovers.
  auto invalid=frame(3,{3,0});
  invalid.back()^=0xff;
  feedAndProcess(service,io,invalid);
  packets=decode(io.output);
  assert(packets.back()[7]==2 && packets.back()[9]==1);
  io.clearOutput();
  feedAndProcess(service,io,frame(3,{3,0}));
  packets=decode(io.output);
  assert(packets.back()[7]==4 && packets.back()[9]==3);
  assert(saved==1);
  std::cout<<"PASS: consecutive, fragmented, scan, Wi-Fi provision, reconnect, checksum recovery\n";
}
