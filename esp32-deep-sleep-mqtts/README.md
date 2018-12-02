# ESP32 Deep-sleep with MQTT

This sketch tests how long it takes the esp32 to wake-up from deep sleep,
connect to wifi, connect to an MQTT server using PSK auth, subscribe to a topic,
publish to that topic, and receive the message back on the subscription.

It has 4 modes which in the amount of information passed into the Wifi initialization functions:                        
`WiFi.begin` and `WiFi.config`:                                                                                       

mode | ssid, passwd | channel, bssid | ip, mask, gw, dns                                                              
:---:|:---:|:---:|:---:                                                                                               
0 | ✔ |   |..                                                                                                         
1 | ✔ | ✔ |..                                                                                                         
2 | ✔ |   | ✔                                                                                                         
3 | ✔ | ✔ | ✔     

### Compile and run

`platformio run -t upload -t monitor`

