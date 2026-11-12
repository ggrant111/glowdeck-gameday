#pragma once
#include "Arduino.h"
#include <cstdint>
enum {WL_DISCONNECTED=6,WL_CONNECTED=3};
enum {WIFI_AUTH_OPEN=0,WIFI_AUTH_WPA2_PSK=3};
class IPAddress {
 public:
  String toString() const {return String("192.168.1.77");}
};
class FakeWiFi {
 public:
  int currentStatus=WL_DISCONNECTED;
  int status() const {return currentStatus;}
  IPAddress localIP() const {return IPAddress();}
  int scanNetworks(bool,bool){return 1;}
  String SSID(int) const {return String("MyWiFi");}
  int RSSI(int) const {return -40;}
  int encryptionType(int) const {return WIFI_AUTH_WPA2_PSK;}
  void scanDelete(){}
};
extern FakeWiFi WiFi;
