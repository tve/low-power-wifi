// ESP8266 light sleep test
// Copyright 2018 by Thosten von Eicken

#include <ESP8266WiFi.h>
#include "wifi.h" // #defines SSID and PASSWD

#define SRVIP IPAddress(192, 168, 0, 2)
#define SRVPORT 12345

void wakeup_cb() {
    wifi_fpm_close();
    Serial.print("W");
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n\n=== light sleep test ===");

    digitalWrite(5, 1); // random I/O pin to trigger scope
    digitalWrite(0, 0); // LED
    pinMode(5, OUTPUT);
    pinMode(0, OUTPUT);

    // Blink 3 times to show start of sketch
    for (int i=0; i<3; i++) {
        digitalWrite(0, 1);
        delay(100);
        digitalWrite(0, 0);
        delay(100);
    }

    WiFi.persistent(false);
#if 1
    // Connect and set light sleep mode
    WiFi.mode(WIFI_STA);
    //if (!WiFi.setSleepMode(WIFI_LIGHT_SLEEP, 5)) Serial.println("setSleepMode failed");
    WiFi.begin(SSID, PASSWD);
    Serial.print("Connecting");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.print("\nConnected, IP address: ");
    Serial.println(WiFi.localIP());
    if (!WiFi.setSleepMode(WIFI_LIGHT_SLEEP, 5)) Serial.println("setSleepMode failed");
#else
    Serial.println("No Wifi");
    WiFi.mode(WIFI_OFF);
    yield();
    //if (!WiFi.setSleepMode(WIFI_LIGHT_SLEEP, 5)) Serial.println("setSleepMode failed");
    yield();
#endif

}

WiFiClient cl;

extern void delaySleep(unsigned long);
void loop() {
    digitalWrite(5, 1); // trigger scope
    digitalWrite(0, 0); // turn LED on
    //Serial.print(".");
    //delay(125);

#if 1
    if (cl.connect(SRVIP, SRVPORT)) {
        char buf[64];
        sprintf(buf, "Light sleep, SSID %s, millis %d\n", SSID, millis());
        cl.write(buf);
        cl.stop();
    } else {
        printf("TCP could not connect\n");
    }
    digitalWrite(0, 1);
    digitalWrite(5, 0);
    delay(4000);

#else
    // failed attempt at using forced light sleep, with arduino it crashes...
    wifi_station_disconnect();yield();
    wifi_set_opmode_current(NULL_MODE);
    yield()
    wifi_fpm_set_sleep_type(LIGHT_SLEEP_T);
    wifi_fpm_open();
    wifi_fpm_set_wakeup_cb(wakeup_cb);
    yield();
    digitalWrite(5, 1);
    digitalWrite(0, 1);
    wifi_fpm_do_sleep(450 * 1000);
    yield();
    digitalWrite(0, 0);
    digitalWrite(5, 0);
    wifi_set_opmode_current(STATION_MODE);
    wifi_station_connect();
#endif
}

