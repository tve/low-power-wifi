// Esp32 deep-sleep experiments
// Originally posted by pvvx on a russian forum and modified by TvE
// for a blog post series https://blog.voneicken.com/projects/low-power/wifi
// Further adapted to esp32
// Further enhanced to send an MQTT message using TLS-PSK security
//
// To run create a wifi.h file with:
//#define SSID "your ssid"
//#define PASSWD "your password"
//#define MQTTIDENT "mqtt PSK identity"
//#define MQTTPSK "mqtt hex PSK string"
// Then adjust the server IP address (srvip) and network parameters further down

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

#include "wifi.h" // #defines SSID and PASSWD
const char ssid[] = SSID; // your network SSID (name)
const char *pass = PASSWD; // "password"
const char mqtt_ident[] = MQTTIDENT;
const char mqtt_psk[] = MQTTPSK;

#define FORCE_MODE 3

IPAddress srvip = IPAddress(192, 168, 0, 14);
#define MQTT_PORT 8883

#define LED 5
#define TRIG 13

// end of configuration stuff

WiFiClientSecure cl;
PubSubClient mqtt(cl);

char chbuf[512];

static RTC_DATA_ATTR struct {
    byte mac [ 6 ];
    byte mode;
    byte chl;
    uint32_t ip;
    uint32_t gw;
    uint32_t msk;
    uint32_t dns;
    uint32_t seq;
    uint32_t chk;
} cfgbuf;

uint32_t startMs, wifiConnMs, MqttConnMs;

bool checkCfg() {
    uint32_t x = 0;
    uint32_t *p = (uint32_t *)cfgbuf.mac;
    for (uint32_t i = 0; i < sizeof(cfgbuf)/4; i++) x += p[i];
    //printf("RTC read: chk=%x x=%x ip=%08x mode=%d %s\n", cfgbuf.chk, x, cfgbuf.ip, cfgbuf.mode,
    //        x==0?"OK":"FAIL");
    if (x == 0 && cfgbuf.ip != 0) return true;
    printf("NVRAM cfg init\n");
    // bad checksum, init data
    for (uint32_t i = 0; i < 6; i++) cfgbuf.mac[i] = 0xff;
    cfgbuf.mode = 0; // chk err, reconfig
    cfgbuf.chl = 0;
    cfgbuf.ip = IPAddress(0, 0, 0, 0);
    cfgbuf.gw = IPAddress(0, 0, 0, 0);
    cfgbuf.msk = IPAddress(255, 255, 255, 0);
    cfgbuf.dns = IPAddress(0, 0, 0, 0);
    cfgbuf.seq = 100;
    return false;
}

void writecfg(void) {
    // save new info
    uint8_t *bssid = WiFi.BSSID();
    for (int i=0; i<sizeof(cfgbuf.mac); i++) cfgbuf.mac[i] = bssid[i];
    cfgbuf.chl = WiFi.channel();
    cfgbuf.ip = WiFi.localIP();
    cfgbuf.gw = WiFi.gatewayIP();
    cfgbuf.msk = WiFi.subnetMask();
    cfgbuf.dns = WiFi.dnsIP();
    // recalculate checksum
    uint32_t x = 0;
    uint32_t *p = (uint32_t *)cfgbuf.mac;
    for (uint32_t i = 0; i < sizeof(cfgbuf)/4-1; i++) x += p[i];
    cfgbuf.chk = -x;
    //printf("RTC write: chk=%x x=%x ip=%08x mode=%d\n", cfgbuf.chk, x, cfgbuf.ip, cfgbuf.mode);
}

bool done = false;
byte msgBuf[64];
uint32_t msgLen;

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    if (strcmp(topic, "/esp32/ping") == 0 && length == msgLen &&
        memcmp(payload, msgBuf, msgLen) == 0)
    {
        done = true;
        return;
    }
    printf("MQTT CB: %s\n", topic);
}

void deep_sleep() {
    printf("Sleep at %d ms\n\n", millis());
    delay(20);
    digitalWrite(TRIG, LOW);
    digitalWrite(LED, LOW);
    esp_sleep_enable_timer_wakeup(20000 * 1000); // Deep-Sleep time in microseconds
    esp_deep_sleep_start();
}

void setup ( ) {
    // Grab time and say hello
    uint32_t startMs = millis();
    digitalWrite(TRIG, HIGH);
    pinMode(TRIG, OUTPUT);
    digitalWrite(LED, HIGH);
    pinMode(LED, OUTPUT);
    Serial.begin(115200);
    printf("\n===== Deep sleep MQTT test starting =====\n");

    // Read config from NVRAM
#ifdef FORCE_MODE
    if (checkCfg()) cfgbuf.mode = FORCE_MODE; // can only force if we got saved info
#else
    checkCfg();
#endif

    // Make sure Wifi settings in flash are off so it doesn't start automagically at next boot
    if (WiFi.getMode() != WIFI_OFF) {
        printf("Wifi wasn't off!\n");
        WiFi.persistent(true);
        WiFi.mode(WIFI_OFF);
    }

    // Init Wifi in STA mode and connect
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    int m = cfgbuf.mode;
    bool ok;
    digitalWrite(TRIG, LOW);
    switch (cfgbuf.mode) {
    case 0:
        ok = WiFi.begin(ssid, pass);
        break;
    case 1:
        ok = WiFi.begin(ssid, pass, cfgbuf.chl, cfgbuf.mac);
        break;
    case 2:
        ok = WiFi.config(cfgbuf.ip, cfgbuf.gw, cfgbuf.msk, cfgbuf.dns);
        if (!ok) printf("*** Wifi.config failed, mode=%d\n", m);
        ok = WiFi.begin(ssid, pass);
        break;
    default:
        ok = WiFi.config(cfgbuf.ip, cfgbuf.gw, cfgbuf.msk, cfgbuf.dns);
        if (!ok) printf("*** Wifi.config failed, mode=%d\n", m);
        ok = WiFi.begin(ssid, pass, cfgbuf.chl, cfgbuf.mac);
        //printf("BSSID: %x:%x:%x:%x:%x:%x\n", cfgbuf.mac[0], cfgbuf.mac[1], cfgbuf.mac[2],
        //    cfgbuf.mac[3], cfgbuf.mac[4], cfgbuf.mac[5]);
        cfgbuf.mode = -1;
        break;
    }
    if (!ok) {
        printf("*** Wifi.begin failed, mode=%d\n", m);
        deep_sleep();
    }
    while (WiFi.status() != WL_CONNECTED) delay(1);
    digitalWrite(TRIG, HIGH);
    uint32_t wifiConnMs = millis();
    printf("Wifi connect in %dms\n", wifiConnMs);

    // Connect to MQTT server
    mqtt.setServer(srvip, 8883);
    mqtt.setCallback(mqttCallback);
    cl.setPreSharedKey(mqtt_ident, mqtt_psk);
    if (!mqtt.connect("esp32_test")) {
        printf("*** mqtt.connect failed err:%d\n", mqtt.state());
        deep_sleep();
    }
    uint32_t mqttConnMs = millis();
    mqtt.subscribe("/esp32/ping");
    cfgbuf.seq++;
    msgLen = sprintf((char *)msgBuf, "seq=%d", cfgbuf.seq);
    mqtt.publish("/esp32/ping", msgBuf, msgLen);

    printf("Mode %d, Init %d ms, Wifi %d ms, Mqtt %d ms, seq=%d, SSID %s, IDF %s\n",
            m, startMs, wifiConnMs, mqttConnMs, cfgbuf.seq,ssid, esp_get_idf_version());

    cfgbuf.mode++;
    writecfg();
    digitalWrite(TRIG, LOW);
}

void loop() {
    digitalWrite(TRIG, HIGH);
    mqtt.loop();
    if (done) deep_sleep();
    if (millis() > 20*1000) {
        printf("No MQTT response\n");
        deep_sleep();
    }
    digitalWrite(TRIG, LOW);
    delay(10);
}
