# matrixClock4s

YAEC-4 (Yet Another Esp8266 Clock with 4 "segments" of dot matrix 8x8 display)  
Dot matrix clock with 4 segments, NTP, WiFi manager and support for daylight saving and LDR backlight adjustment and MQTT messages displaying
---

Arduino code for ESP8266 with connected Matrix display(CS:D3; CLK:D5; MOSI/DIN:D7) and LDR(A0)

Just connect power, set the WifiManager (SSID: MatrixClock) and connect to your WiFi. Daylight saving mode is set to Czech republic (CET,CEST)

![alt text](https://raw.githubusercontent.com/owarek/matrixClock4s/master/img/IMG_20181108_080316.jpg)
TODO:  
*Create a 6 segment version  
*Add WifiManager settings for DLS mode  
*Add WifiManager onDemand setting  
*Add WifiManager setting for SPIFFS format  
*Upload some Wifimanager settings screenshots   
*Add support for cap. touch sensors
*Add support for buzzer (Alarms)
*Add support for Neopixels
