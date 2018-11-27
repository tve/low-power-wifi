# ESP8266 Deep-sleep with periodic wake-up

This sketch tests how long it takes the esp8266 to wake-up from deep sleep,
connect to wifi, send some data to a server over TCP (or UDP), and go back
to sleep.

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

### Start server to capture data packets

`nc -lukn -p 12345`

