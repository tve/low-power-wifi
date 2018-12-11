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

IPAddress srvip = IPAddress(192, 168, 0, 14);
#define MQTT_PORT 8883

#define LED 5
#define TRIG 13

#define TLS 1

// end of configuration stuff

#if TLS
WiFiClientSecure cl;
#else
WiFiClient cl;
#endif
PubSubClient mqtt(cl);

uint32_t startMs, wifiConnMs, mqttConnMs;

bool done = false;
byte msgBuf[64];
uint32_t msgLen;
uint32_t msgSeq = 100;

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    if (strcmp(topic, "/esp32/ping") == 0 && length == msgLen &&
        memcmp(payload, msgBuf, msgLen) == 0)
    {
        //printf("SUB: %s\n", payload);
        done = true;
        return;
    }
    printf("MQTT CB: %s\n", topic);
}

void setup ( ) {
    // Grab time and say hello
    uint32_t startMs = millis();
    digitalWrite(TRIG, HIGH);
    pinMode(TRIG, OUTPUT);
    digitalWrite(LED, HIGH);
    pinMode(LED, OUTPUT);
    Serial.begin(115200);
    printf("\n===== Light sleep MQTT test starting =====\n");

    // Init Wifi in STA mode and connect
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    digitalWrite(TRIG, LOW);
    bool ok = WiFi.begin(ssid, pass);
    if (!ok) {
        printf("*** Wifi.begin failed\n");
        return;
    }
    while (WiFi.status() != WL_CONNECTED) delay(1);
    digitalWrite(TRIG, HIGH);
    uint32_t wifiConnMs = millis();
    printf("Wifi connect in %dms\n", wifiConnMs);

    // Connect to MQTT server
    mqtt.setServer(srvip, MQTT_PORT);
    mqtt.setCallback(mqttCallback);
#if TLS
    cl.setPreSharedKey(mqtt_ident, mqtt_psk);
#endif
    cl.setNoDelay(true);
    if (!mqtt.connect("esp32_test2")) {
        printf("*** mqtt.connect failed err:%d\n", mqtt.state());
        return;
    }
    uint32_t mqttConnMs = millis();
    cl.setNoDelay(true);
    mqtt.subscribe("/esp32/ping");
    printf("Init %d ms, Wifi %d ms, Mqtt %d ms, SSID %s, IDF %s\n",
            startMs, wifiConnMs, mqttConnMs, ssid, esp_get_idf_version());
    digitalWrite(TRIG, LOW);
    digitalWrite(LED, LOW);
}

void loop() {
    digitalWrite(TRIG, HIGH);
    digitalWrite(LED, HIGH);
    msgLen = sprintf((char *)msgBuf, "seq=%d", msgSeq) + 1;
    //printf("PUB: %s\n", msgBuf);
    mqtt.publish("/esp32/ping", msgBuf, msgLen);
    done = false;
    uint32_t loopMs = millis();
    while (!done && millis()-loopMs < 2*1000) {
        mqtt.loop();
        delay(1);
    }
    msgSeq++;
    uint32_t doneMs = millis();
    if (!done) {
        printf("No MQTT response\n");
    } else {
        printf("MQTT in %dms\n", doneMs - loopMs);
    }
    digitalWrite(TRIG, LOW);
    digitalWrite(LED, LOW);
    delay(4500);
}
