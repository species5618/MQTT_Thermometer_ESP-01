/*
 Name:		MQTT_Thermometer.ino
 Created:	11/3/2017 7:34:04 AM
 Author:	Lee
*/

//#define DEBUGLOG Serial.println

//#define PRODUCTION  // remove cemment to disable telnet debug 

#define USE_MDNS

#include <PubSubClient.h>
#include <Ticker.h>

//experimental !!!
#ifdef ESP32
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPmDNS.h>
#elif defined ESP8266
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESP8266mDNS.h>
#include <Hash.h>
//#include <ESP8266NetBIOS.h>
#else
#error Platform not supported
#endif

#include "FS.h"
#include <WiFiClient.h>
#include <TimeLib.h>
#include <NtpClientLib.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include "FSWebServerLib.h"

#ifndef PRODUCTION // Not in PRODUCTION
#include "RemoteDebug.h"        //https://github.com/JoaoLopesF/RemoteDebug
RemoteDebug Debug;
uint32_t mLastTime = 0;
uint32_t mTimeSeconds = 0;

#endif

#include <OneWire.h>
#include <DallasTemperature.h>

// Data wire is plugged into pin 2 on the Arduino
#define ONE_WIRE_BUS 2
// Setup a oneWire instance to communicate with any OneWire devices 
// (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

String configMQTT_User = "";
String configMQTT_Pass = "";
String configMQTT_Host = "";
String configMQTT_Topic = "";
String configMQTT_ClientID = "";
int configMQTT_Port = 0;
int intervalCounter = 0;
unsigned long previousMillis = 0;
unsigned long previousMillis2 = 0;
unsigned long currentMillis = millis();
float TempCLocal = 0.0;
int dTime = 10; // default sample interval
float TempC = -99.0;
float maxTempC = -99.0;
String maxTimeHrs = "";
String maxTimeMins = "";
String maxTimeSecs = "";
float minTempC = 99.9;
String minTimeHrs = "";
String minTimeMins = "";
String minTimeSecs = "";
bool minMaxResetFlag = false;
int minMaxInterval = 3600;
unsigned long MinMaxpreviousMillis = 0;
String minMaxJson = "";

#define Filterdefault 4
int sampleInterval = 60;
float filter = Filterdefault;


WiFiClient espClient;
PubSubClient client(espClient);
boolean	first = true;

String zeroPad(int number) // pad out left of int with ZERO for units less than 10 , 
{
	if (number<10)
	{
		return "0" + String(number);
	}
	else
	{
		return "" + String(number);
	}
}

boolean testMqtt()
{
	if (timeStatus() == timeNotSet || configMQTT_Host == "" || configMQTT_Pass == "" || configMQTT_Port == 0 || configMQTT_Topic == "" || configMQTT_User == "" || configMQTT_ClientID == "")
	{
		return false;
	}
	else
	{
		return true;
	}

}
void  callbackREST(AsyncWebServerRequest *request)
{
	//its possible to test the url and do different things, 
	//test you rest URL
	String values = "";
	if (request->url() == "/rest/TempC")
	{
		values += "TempC|" + (String)TempC + "|div\n";
		values += "TempCRaw|" + (String)TempCLocal + "|div\n";
		values += "minTempC|" + (String)minTempC + "|div\n";
		values += "maxTempC|" + (String)maxTempC + "|div\n";
		values += "minTempTime|" + minTimeHrs + ":" + minTimeMins + ":" + maxTimeSecs + "|div\n";
		values += "maxTempTime|" + maxTimeHrs + ":" + maxTimeMins + ":" + maxTimeSecs + "|div\n";
	}
	else
	{
		//its possible to test the url and do different things, 
		values = "";
	}
	request->send(200, "text/plain", values);
	values = "";
}

void  callbackJSON(AsyncWebServerRequest *request)
{
	//its possible to test the url and do different things, 
	//test you rest URL
	String values = "";
	if (request->url() == "/json/TempC")
	{
		values += minMaxJson;
	}
	else
	{
		//its possible to test the url and do different things, 
		values = "message:Hello world! \nurl:" + request->url() + "\n";
	}
	request->send(200, "text/plain", values);
	values = "";
}

void  callbackPOST(AsyncWebServerRequest *request)
{
	{
		String values = "message:Hello world! \nurl:" + request->url() + "\n";
		request->send(200, "text/plain", values);
		values = "";

	}
}

void callback(char* topic, byte* payload, unsigned int length) {
}

void reconnect() {
	// Loop until we're reconnected
	while (!client.connected() && testMqtt) {
		Serial.println("Attempting MQTT connection...");
		// Attempt to connect
		Serial.print("configMQTT_ClientID ");
		Serial.println(configMQTT_ClientID);

		Serial.print("configMQTT_User ");
		Serial.println(configMQTT_User);

		Serial.println("configMQTT_Pass ");
		Serial.println(configMQTT_Pass);

		Serial.println("configMQTT_Topic ");
		Serial.println(configMQTT_Topic);

		Serial.println("");
		Serial.println();

		if (
			client.connect(configMQTT_ClientID.c_str(), configMQTT_User.c_str(), configMQTT_Pass.c_str(),
			("/" + (String)configMQTT_Topic + "/" + configMQTT_ClientID + "/LWT").c_str(), 1, true, "DOWN")

			//client.connect(configMQTT_ClientID.c_str(), configMQTT_User.c_str(), configMQTT_Pass.c_str())

			) {
			Serial.println("connected");
			// Once connected, publish an announcement...
			//client.publish("outTopic", "hello world");
			//// ... and resubscribe
			//client.subscribe("inTopic");
//			client.unsubscribe("#");
			client.publish(("/" + configMQTT_Topic + "/" + configMQTT_ClientID + "/LWT").c_str(), "UP");
//			client.subscribe(("/" + configMQTT_Topic + "/" + configMQTT_ClientID + "/LWT").c_str(), 1);

		}
		else {
			Serial.println("failed, rc=");
			Serial.println(client.state());
			Serial.println(" try again in 5 seconds");
			// Wait 5 seconds before retrying
			delay(1000);
		}
	}




}

#ifndef PRODUCTION // Not in PRODUCTION

// Process commands from RemoteDebug

void processCmdRemoteDebug() {

	String lastCmd = Debug.getLastCommand();

	if (lastCmd == "bench1") {

		// Benchmark 1 - Printf

		if (Debug.isActive(Debug.ANY)) {
			Debug.println("* Benchmark 1 - one Printf");
		}

		uint32_t timeBegin = millis();
		uint8_t times = 50;

		for (uint8_t i = 1; i <= times; i++) {
			if (Debug.isActive(Debug.ANY)) {
				Debug.printf("%u - 1234567890 - AAAA\n", i);
			}
		}

		if (Debug.isActive(Debug.ANY)) {
			Debug.printf("* Time elapsed for %u printf: %u ms.\n", times, (millis() - timeBegin));
		}

	}
	else if (lastCmd == "bench2") {

		// Benchmark 2 - Print/println

		if (Debug.isActive(Debug.ANY)) {
			Debug.println("* Benchmark 2 - Print/Println");
		}

		uint32_t timeBegin = millis();
		uint8_t times = 50;

		for (uint8_t i = 1; i <= times; i++) {
			if (Debug.isActive(Debug.ANY)) {
				Debug.print(i);
				Debug.print(" - 1234567890");
				Debug.println(" - AAAA");
			}
		}

		if (Debug.isActive(Debug.ANY)) {
			Debug.printf("* Time elapsed for %u printf: %u ms.\n", times, (millis() - timeBegin));
		}
	}
}
#endif

void setup() {

	String HOST_NAME = "ESP";

	// WiFi is started inside library
	SPIFFS.begin(); // Not really needed, checked inside library and started if needed
	ESPHTTPServer.begin(&SPIFFS);
	/* add setup code here */

	//set optional callback
	ESPHTTPServer.setRESTCallback(callbackREST);

	//set optional callback
	ESPHTTPServer.setPOSTCallback(callbackPOST);

	ESPHTTPServer.setJSONCallback(callbackJSON);


	// MQTTUser/MQTTPass/MQTTTopic/MQTTHost/MQTTPort/MQTTClientID
	ESPHTTPServer.load_user_config("MQTTHost", configMQTT_Host);
	ESPHTTPServer.load_user_config("MQTTPass", configMQTT_Pass);
	ESPHTTPServer.load_user_config("MQTTPort", configMQTT_Port);
	ESPHTTPServer.load_user_config("MQTTTopic", configMQTT_Topic);
	ESPHTTPServer.load_user_config("MQTTUser", configMQTT_User);
	ESPHTTPServer.load_user_config("MQTTClientID", configMQTT_ClientID);
	ESPHTTPServer.load_user_config("Filter", filter);
	ESPHTTPServer.load_user_config("sampleInterval", sampleInterval);
	ESPHTTPServer.load_user_config("minMaxInterval", minMaxInterval);





	if ((configMQTT_Host == "") || (configMQTT_Pass == "") || (configMQTT_Port == 0) || (configMQTT_Topic == "") || (configMQTT_User == "") || (configMQTT_ClientID == ""))
	{
		Serial.println("Please Configure MQTT !!!!");
	}
	else
	{
		Serial.println("MQTT INIT");
		Serial.println(configMQTT_Host);
		Serial.println(configMQTT_Port);
		Serial.println(configMQTT_User);
		Serial.println(configMQTT_Topic);
		Serial.println(configMQTT_Pass);
		Serial.println(configMQTT_ClientID);
		client.setServer(configMQTT_Host.c_str(), configMQTT_Port);
		client.setCallback(callback);

		// init MQTT
		reconnect();
		HOST_NAME = configMQTT_ClientID;
	}

	sensors.begin();
//few throw away reading to reset hard ware

	sensors.requestTemperatures(); // Send the command to get temperatures
	delay(100);
	sensors.requestTemperatures(); // Send the command to get temperatures
	delay(100);


#if defined (USE_MDNS) // && defined(HOSTNAME)
	if (MDNS.begin(HOST_NAME.c_str())) {
		Serial.print("* MDNS responder started. Hostname -> ");
		Serial.println(configMQTT_ClientID);
	}
	// Register the services

	MDNS.addService("http", "tcp", 80);   // Web server - discomment if you need this
	MDNS.addService("telnet", "tcp", 23); // Telnet server RemoteDebug
#endif

#ifndef PRODUCTION // Not in PRODUCTION

	Debug.begin(HOST_NAME); // Initiaze the telnet server
	Debug.setResetCmdEnabled(true); // Enable the reset command

									//Debug.showDebugLevel(false); // To not show debug levels
									//Debug.showTime(true); // To show time
									//Debug.showProfiler(true); // To show profiler - time between messages of Debug
									// Good to "begin ...." and "end ...." messages

	Debug.showProfiler(true); // Profiler
	Debug.showColors(true); // Colors

							// Debug.setSerialEnabled(true); // if you wants serial echo - only recommended if ESP8266 is plugged in USB

	//String helpCmd = "bench1 - Benchmark 1\n";
	//helpCmd.concat("bench2 - Benchmark 2");

	//Debug.setHelpProjectsCmds(helpCmd);
	Debug.setCallBackProjectCmds(&processCmdRemoteDebug);

	// This sample

	Serial.println("* Arduino RemoteDebug Library");
	Serial.println("*");
	Serial.print("* WiFI connected. IP address: ");
	Serial.println(WiFi.localIP());
	Serial.println("* Please use the telnet client (telnet for Mac/Unix or putty and others for Windows)");
	Serial.println("* This sample will send messages of debug in all levels.");
	Serial.println("* Please try change debug level in telnet, to see how it works");
	Serial.println("*");

#endif

}

void loop() {
	/* add main program code here */
	currentMillis = millis();

	if (((currentMillis - previousMillis2) > (sampleInterval * 1000)) || (TempC < -90 && TempC != -127))
	{
		ESPHTTPServer.load_user_config("Filter", filter);
		ESPHTTPServer.load_user_config("sampleInterval", sampleInterval);
		ESPHTTPServer.load_user_config("dTime", dTime);
		ESPHTTPServer.load_user_config("minMaxInterval", minMaxInterval);

		previousMillis2 = millis();
		Serial.println(" Requesting temperatures...");
		sensors.requestTemperatures(); // Send the command to get temperatures
		TempCLocal = sensors.getTempCByIndex(0);
		if (TempC < -90)
		{
			sensors.requestTemperatures(); // Send the command to get temperatures
			TempCLocal = sensors.getTempCByIndex(0);
			TempC = TempCLocal;
		}
		TempC = TempC + ((TempCLocal - TempC) / filter);
		Serial.print("Temperature is: ");
		Serial.println(TempC); // Why "byIndex"? 

		//deal with min andmax
		if (minMaxResetFlag && hour() == 0)
		{
					reconnect();
		if (testMqtt())
		{
			reconnect();
			client.publish(("/" + configMQTT_Topic + "/" + configMQTT_ClientID + "/minMaxTempC").c_str(), minMaxJson.c_str(), true);
			// publish
		}
			maxTempC = -99.0;
			maxTimeHrs = "00";
			maxTimeMins = "00";
			maxTimeSecs = "00";
			minTempC = 99.9;
			minTimeHrs = "00";
			minTimeMins = "00";
			minTimeSecs = "00";
			minMaxResetFlag = false;
		}
		if (!minMaxResetFlag && hour() == 23)
		{
			minMaxResetFlag = true;
		}
		if (TempC > maxTempC && timeStatus() != timeNotSet)
		{
			maxTempC = TempC;
			maxTimeHrs = zeroPad(hour());
			maxTimeMins = zeroPad(minute());
			maxTimeSecs = zeroPad(second());
			//minMaxJson = "{\"Device\":\"" + configMQTT_ClientID + "\",\"maxTemp\":" + String(maxTempC) +
			//	",\"maxTime\":\"" + maxTimeHrs + ":" + maxTimeMins + ":" + maxTimeSecs + "\"," +
			//	"\"minTemp\":" + String(minTempC) +
			//	",\"minTime\":\"" + minTimeHrs + ":" + minTimeMins + ":" + minTimeSecs + "\"," +
			//	"\"TempC\":" + String(TempC) + ",\"TempCRAW\":" + String(TempCLocal) +
			//	"}";
// reduce ot under 128 bytes for mqtt
			minMaxJson = "{\"Dev\":\"" + configMQTT_ClientID + "\",\"maxTp\":" + String(maxTempC) +
				",\"maxTm\":\"" + maxTimeHrs + ":" + maxTimeMins + ":" + maxTimeSecs + "\"," +
				"\"minTp\":" + String(minTempC) +
				",\"minTm\":\"" + minTimeHrs + ":" + minTimeMins + ":" + minTimeSecs + "\"," +
				"\"TC\":" + String(TempC) + ",\"TCRAW\":" + String(TempCLocal) +
				"}";

		}
		if (TempC < minTempC && timeStatus() != timeNotSet)
		{
			minTempC = TempC;
			minTimeHrs = zeroPad(hour());
			minTimeMins = zeroPad(minute());
			minTimeSecs = zeroPad(second());
			//minMaxJson = "{\"Device\":\"" + configMQTT_ClientID + "\",\"maxTemp\":" + String(maxTempC) +
			//	",\"maxTime\":\"" + maxTimeHrs + ":" + maxTimeMins + ":" + maxTimeSecs + "\"," +
			//	"\"minTemp\":" + String(minTempC) +
			//	",\"minTime\":\"" + minTimeHrs + ":" + minTimeMins + ":" + minTimeSecs + "\"," +
			//	"\"TempC\":" + String(TempC) + ",\"TempCRAW\":" + String(TempCLocal) +
			//	"}";
			// reduce ot under 128 bytes for mqtt
			minMaxJson = "{\"Dev\":\"" + configMQTT_ClientID + "\",\"maxTp\":" + String(maxTempC) +
				",\"maxTm\":\"" + maxTimeHrs + ":" + maxTimeMins + ":" + maxTimeSecs + "\"," +
				"\"minTp\":" + String(minTempC) +
				",\"minTm\":\"" + minTimeHrs + ":" + minTimeMins + ":" + minTimeSecs + "\"," +
				"\"TC\":" + String(TempC) + ",\"TCRAW\":" + String(TempCLocal) +
				"}";

		}

	}
	if ((currentMillis - MinMaxpreviousMillis) > (minMaxInterval * 1000))
	{
		MinMaxpreviousMillis = millis();
		reconnect();
		if (testMqtt())
		{
			reconnect();
			client.publish(("/" + configMQTT_Topic + "/" + configMQTT_ClientID + "/minMaxTempC").c_str(), minMaxJson.c_str(), true);
			// publish
		}
	
	}
	if ((currentMillis - previousMillis) > (dTime * 1000)) 
	{
		previousMillis = millis();
		
		// call sensors.requestTemperatures() to issue a global temperature
		// request to all devices on the bus


			// You can have more than one IC on the same bus. 

			// 0 refers to the first IC on the wire
			if (testMqtt())
			{
				reconnect();
				client.publish(("/" + configMQTT_Topic + "/" + configMQTT_ClientID + "/TempC").c_str(), String(TempC).c_str(), true);
				// publish
			}
	}


	// DO NOT REMOVE. Attend OTA update from Arduino IDE
	ESPHTTPServer.handle();
	client.loop();

	// Each second
#ifndef PRODUCTION // Not in PRODUCTION
	// Each second
	if ((millis() - mLastTime) >= 1000) {

		// Time

		mLastTime = millis();

		mTimeSeconds++;




		// Debug the time (verbose level)

		if (Debug.isActive(Debug.VERBOSE)) {
			Debug.printf("* Time: %u seconds (VERBOSE)\n", mTimeSeconds);
		}

		if (mTimeSeconds % 5 == 0) { // Each 5 seconds

									 // Debug levels

			//if (Debug.isActive(Debug.VERBOSE)) {
			//	Debug.println("* This is a message of debug level VERBOSE");
			//}
			//if (Debug.isActive(Debug.DEBUG)) {
			//	Debug.println("* This is a message of debug level DEBUG");
			//}
			//if (Debug.isActive(Debug.INFO)) {
			//	Debug.println("* This is a message of debug level INFO");
			//}
			//if (Debug.isActive(Debug.WARNING)) {
			//	Debug.println("* This is a message of debug level WARNING");
			//}
			//if (Debug.isActive(Debug.ERROR)) {
			//	Debug.println("* This is a message of debug level ERROR");
			//}
		}

	}

	Debug.handle();
#endif


}
