# MQTT_Thermometer_ESP-01
MQTT_Thermometer which runs on ESP-01

based on https://github.com/gmag11/FSBrowserNG at the core for a web based ui

This is a MQTT temp sensor, which will run on an ESP-01

MQTT temp to /control/WorkRoom/TempC
a min max json string is posted to /control/WorkRoom/minMaxTempC
Last will posted to /control/WorkRoom/LWT

control is set as topic in mqtt.html
WorkRoom is set as client id  in mqtt.html

TempC is fixed
minMaxTempC is fixed

exmaple minmax json
{"Dev":"WorkRoom","maxTp":16.94,"maxTm":"00:00:05","minTp":16.93,"minTm":"17:25:56","TC":16.93,"TCRAW":16.87}

json string also available from
http://IPADDRESS/json/TempC

see  https://github.com/gmag11/FSBrowserNG  for WiFi setup details
read notes carefully, a file tweak is required to get time.h to compile

Data files for SPIFFS in data folder

NOTE !!

change required to PubSubClient.h
// MQTT_MAX_PACKET_SIZE : Maximum packet size
#ifndef MQTT_MAX_PACKET_SIZE
#define MQTT_MAX_PACKET_SIZE 192
#endif

