// Esp32 deep-sleep experiments
// Originally posted by pvvx on a russian forum and modified by TvE
// for a blog post series https://blog.voneicken.com/projects/low-power/wifi
// Further adapted to esp32
//
// To run create a wifi.h file with:
//#define SSID "your ssid"
//#define PASSWD "your password"
// Then adjust the server IP address (srvip) and network parameters further down

#include <WiFi.h>
#include <WiFiUdp.h>

#include "wifi.h" // #defines SSID and PASSWD
char ssid [ ] = SSID; // your network SSID (name)
char *pass = PASSWD; // "password"

#define USE_TCP
#define FORCE_MODE 3

IPAddress srvip = IPAddress ( 192 , 168 , 0 , 2 );
uint16_t  srvport = 12345;
WiFiUDP Udp;
char chbuf [ 512 ];

static RTC_DATA_ATTR struct {
  byte mac [ 6 ];
  byte mode;
  byte chl;
  uint32_t ip;
  uint32_t gw;
  uint32_t msk;
  uint32_t dns;
  uint16_t localPort;
  uint32_t chk;
} cfgbuf;

uint32_t StartTimeMs, SetupTime;

bool checkCfg() {
    uint32_t x = 0;
    uint32_t *p = (uint32_t *)cfgbuf.mac;
    for (uint32_t i = 0; i < sizeof(cfgbuf)/4; i++) x += p[i];
    printf("RTC read: chk=%x x=%x ip=%08x mode=%d %s\n", cfgbuf.chk, x, cfgbuf.ip, cfgbuf.mode,
            x==0?"OK":"FAIL");
    if (x == 0 && cfgbuf.ip != 0) return true;
    p = (uint32_t *)cfgbuf.mac;
    for (uint32_t i = 0; i < sizeof(cfgbuf)/4; i++) printf(" %08x", p[i]);
    printf("\n");
    // bad checksum, init data
    for (uint32_t i = 0; i < 6; i++) cfgbuf.mac[i] = 0xff;
    cfgbuf.mode = 0; // chk err, reconfig
    cfgbuf.chl = 0;
    cfgbuf.ip = IPAddress(0, 0, 0, 0);
    cfgbuf.gw = IPAddress(0, 0, 0, 0);
    cfgbuf.msk = IPAddress(255, 255, 255, 0);
    cfgbuf.dns = IPAddress(0, 0, 0, 0);
    cfgbuf.localPort = 10000;
    return false;
}

void writecfg(void) {
    // save new info
    uint8_t *bssid = WiFi.BSSID();
    for (uint32_t i = 0; i < sizeof(cfgbuf.mac); i++) cfgbuf.mac[i] = bssid[i];
    cfgbuf.chl = WiFi.channel();
    cfgbuf.ip = WiFi.localIP();
    cfgbuf.gw = WiFi.gatewayIP();
    cfgbuf.msk = WiFi.subnetMask();
    cfgbuf.dns = WiFi.dnsIP();
    //printf("BSSID: %x:%x:%x:%x:%x:%x\n", cfgbuf.mac[0], cfgbuf.mac[1], cfgbuf.mac[2],
    //    cfgbuf.mac[3], cfgbuf.mac[4], cfgbuf.mac[5]);
    // recalculate checksum
    uint32_t x = 0;
    uint32_t *p = (uint32_t *)cfgbuf.mac;
    for (uint32_t i = 0; i < sizeof(cfgbuf)/4-1; i++) x += p[i];
    cfgbuf.chk = -x;
    printf("RTC write: chk=%x x=%x ip=%08x mode=%d\n", cfgbuf.chk, x, cfgbuf.ip, cfgbuf.mode);
    p = (uint32_t *)cfgbuf.mac;
    for (uint32_t i = 0; i < sizeof(cfgbuf)/4; i++) printf(" %08x", p[i]);
    printf("\n");
}

void setup ( ) {
  StartTimeMs = millis();
  digitalWrite(13, HIGH);
  pinMode(13, OUTPUT);
  Serial.begin(115200);
  printf("\n===== Deep sleep test starting =====\n");
#ifdef FORCE_MODE
  if (checkCfg()) cfgbuf.mode = FORCE_MODE; // can only force if we got saved info
#else
  checkCfg();
#endif
  if (WiFi.getMode() != WIFI_OFF) {
      printf("Wifi wasn't off!\n");
      WiFi.persistent(true);
      WiFi.mode(WIFI_OFF);
  }
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  int m = cfgbuf.mode;
  digitalWrite(13, LOW);
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
      //printf("BSSID: %x:%x:%x:%x:%x:%x\n", cfgbuf.mac[0], cfgbuf.mac[1], cfgbuf.mac[2],
      //    cfgbuf.mac[3], cfgbuf.mac[4], cfgbuf.mac[5]);
      cfgbuf.mode = -1;
      break;
  }
  while (WiFi.status() != WL_CONNECTED) delay(1);
  digitalWrite(13, HIGH);
  uint32_t total = millis();
  uint32_t connect = total - StartTimeMs;
  sprintf(chbuf, "Mode %d, Init %d ms, Connect %d ms, Total %d ms, SSID %s, IDF %s\n",
        m, StartTimeMs, connect, total, ssid, esp_get_idf_version());
  printf("%s",chbuf);
}

void loop(){
  uint32_t t0 = millis();
  uint32_t t1 = t0;
#ifdef USE_TCP
  WiFiClient cl;
  //WiFiClient::setLocalPortStart(cfgbuf.localPort++); 
  if (cl.connect(srvip, srvport)) {
      t1 = millis();
      cfgbuf.localPort = cl.localPort();
      cl.write(chbuf);
      cl.stop();
  } else {
      printf("TCP could not connect\n");
  }
  delay(10);
#else
  Udp.beginPacket(srvip, srvport);
  Udp.write(chbuf);
  Udp.endPacket();
  delay(20); // make sure packet leaves the house...
#endif
  printf("TCP connect: %d ms, total %d ms @ %d ms, port %d\n", t1-t0, millis()-t0, millis(),
      cfgbuf.localPort);
  cfgbuf.mode++;
  writecfg();
  digitalWrite(13, LOW);
  printf("Sleep at %d ms\n\n", millis());
  delay(20);
  esp_sleep_enable_timer_wakeup(5000 * 1000); // Deep-Sleep time in microseconds
  esp_deep_sleep_start();
  printf("This should never get printed\n");
}
