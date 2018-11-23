#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
extern "C" {
#include "user_interface.h"
}

#include "wifi.h" // #defines SSID and PASSWD
char ssid [ ] = SSID ; // your network SSID (name)
char *pass = PASSWD; // "password"

#define USE_TCP
#define FORCE_MODE 3

IPAddress srvip = IPAddress ( 192 , 168 , 0 , 2 ) ;
uint16_t  srvport = 12345 ;
WiFiUDP Udp ;
char chbuf [ 512 ] ;

struct {
  byte mac [ 6 ] ;
  byte mode ;
  byte chl ;
  IPAddress ip ;
  IPAddress gw ;
  IPAddress msk ;
  IPAddress dns ;
  uint32_t chk ;

} cfgbuf;

uint32_t StartTimeMs , SetupTime ;

// returns 0=got saved info, 1=data reset
uint32_t readcfg(void) {
  uint32_t x = 0 ;
  uint32_t *p = (uint32_t *)cfgbuf.mac ;
  ESP.rtcUserMemoryRead(0 ,p ,sizeof(cfgbuf));
  for (uint32_t i = 0; i < sizeof(cfgbuf)/4; i++) x += p[i];
  if (x) {
    for (uint32_t i = 0; i < 6; i++) cfgbuf.mac[i] = 0xff;
    cfgbuf.mode = 0; // chk err, reconfig
    cfgbuf.chl = 1;
    cfgbuf.ip = IPAddress(192, 168, 0, 106);
    cfgbuf.gw = IPAddress(192, 168, 0, 1);
    cfgbuf.msk = IPAddress(255, 255, 255, 0);
    cfgbuf.dns = IPAddress(192, 168, 0, 1);
    x = 1;
  }
  return x;
}

void writecfg(void) {
  int x = 0;
  static struct station_config conf;
  wifi_station_get_config(&conf);
  for (uint32_t i = 0; i < sizeof(conf.bssid); i++) cfgbuf.mac[i] = conf.bssid[i];
  cfgbuf.chl = wifi_get_channel();
  uint32_t *p = (uint32_t *)cfgbuf.mac;
  for (uint32_t i = 0; i < sizeof(cfgbuf)/4-1 ; i++) x += p[i];
  cfgbuf.chk =- x ;
  ESP.rtcUserMemoryWrite(0, p, sizeof(cfgbuf));
}

void setup ( ) {
  StartTimeMs = millis();
  digitalWrite(0, LOW);
  digitalWrite(5, LOW);
  pinMode(0, OUTPUT);
  pinMode(5, OUTPUT);
  Serial.begin(115200);
  readcfg();
#ifdef FORCE_MODE
  if (readcfg() == 0) cfgbuf.mode = FORCE_MODE; // can only force if we got saved info
#else
  readcfg();
#endif
  //printf("\n===== Deep sleep test starting mode=%d =====\n", cfgbuf.mode);
  if (WiFi.getMode() != WIFI_OFF) {
      WiFi.persistent(true);
      WiFi.mode(WIFI_OFF);
  }
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  int m = cfgbuf.mode;
  switch (cfgbuf.mode) {
    case 0:
      WiFi.begin(ssid, pass);
      break;
    case 1:
      WiFi.begin(ssid, pass, cfgbuf.chl, cfgbuf.mac);
      break;
    case 2: {
      bool ok = WiFi.config(cfgbuf.ip, cfgbuf.gw, cfgbuf.msk, cfgbuf.dns);
      if (!ok) printf("*** Wifi.config failed, mode=%d\n", m);
      WiFi.begin(ssid, pass);
      break; }
    default:
      WiFi.config(cfgbuf.ip, cfgbuf.gw, cfgbuf.msk, cfgbuf.dns);
      WiFi.begin(ssid, pass, cfgbuf.chl, cfgbuf.mac);
      cfgbuf.mode = -1;
      break;
  }
  while (WiFi.status() != WL_CONNECTED) delay(1);
  uint32_t total = millis();
  uint32_t connect = total - StartTimeMs;
  sprintf(chbuf, "Mode %d, Init %d ms, Connect %d ms, Total %d ms, SSID %s, SDK %s\n",
        m, StartTimeMs, connect, total, ssid, system_get_sdk_version());
  printf("\n%s",chbuf);
}

void loop(){
#ifdef USE_TCP
  WiFiClient cl;
  if (cl.connect(srvip, srvport)) {
      cl.write(chbuf);
      cl.stop();
  }
#else
  Udp.beginPacket(srvip, srvport);
  Udp.write(chbuf);
  Udp.endPacket();
  delay(20);
#endif
  yield();
  cfgbuf.mode++;
  writecfg();
  digitalWrite(0, HIGH);
  digitalWrite(5, HIGH);
  // WAKE_RF_DEFAULT, WAKE_RFCAL, WAKE_NO_RFCAL, WAKE_RF_DISABLED
  ESP.deepSleep(3000 * 1000, WAKE_NO_RFCAL); // Deep-Sleep time in microseconds
}
