/**
 * @file    DTSU666 PV emulator
 * @author  Michiel Steltman (git: michielfromNL, msteltman@disway.nl 
 * @brief   Emulates a  DTSU666 meter over modbus RTU . Uses MQTT to get data
 * @version 1.0
 * @date    2024-06-30
 * 
 * @copyright Copyright (c) 2024, MIT license
 *  DTSU666 manual: https://www.solaxpower.com/uploads/file/dtsu666-user-manual-en.pdf
 * 
 */
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <ArduinoOTA.h>
#include <WiFiManager.h>  
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <DTSU666.h>

// Max485 module, We use 3v3 which works fine for not very long lines
#define TX1     D7  // 485 DI Pin
#define RX1     D6  // 485 RO pin
#define RE_DE1  D2  // 485 combined RE DE

#define BUTTON  D5

// Fixed configs
#ifdef PRODUCTION
#define LEDPIN  D1          // external led, HIGH is on
#define DEFAULT_MQTTSERVER                ""
const char * MQTT_CLIENT_ID     = "ESP8266_DTSU666PV";
const char * HOSTNAME           = "dtsu666PV.local";
const char * AC_AP_NAME         = "DTSU666PV_AC";
const char * CFG_AP_NAME        = "DTSU666PV_CFG";

#else
#define LEDPIN  LED_BUILTIN  // Interal led, LOW is on
#define DEFAULT_MQTTSERVER         "diskstation.local"
const char * MQTT_CLIENT_ID       = "ESP8266_DTSU666PV_DBG";
const char * HOSTNAME             = "dtsu666PV_DBG.local";
const char * AP_NAME              = "DTSU666PV_DBG_AP";
#endif

#define CHECK_INTERVAL        5432  // milliseconds interval to check if still connected

Preferences   prefs;
WiFiClient    wificlient;
PubSubClient  mqtt(wificlient);
JsonDocument  doc;

// the custome parameters strings for configuration, with defaults
// The defaults will show up in the portal
char mqttserver[40] = DEFAULT_MQTTSERVER;
char mqttport[6]    = "1883";
char mqtttopic[32]  = "pvdata/#";
char address[4]     = "1";

// Declare the meters and serial lines
SoftwareSerial S1(RX1,TX1);
DTSU666 PV;

// Led flash 
ulong ledOnSince = 0;   // switch off in mainloop
void LedOn(bool on) {
  if (on) {
    digitalWrite(LEDPIN,LEDPIN == LED_BUILTIN ? LOW : HIGH);
    ledOnSince = millis();
  } else {
    digitalWrite(LEDPIN,LEDPIN == LED_BUILTIN ? HIGH : LOW);
    ledOnSince = 0;
  }
}

/**
 * @brief This is the data-source implementation specific part
 * THis specifies where to het DATA from a json record.  
 * 
 */
typedef struct JsonEntry {
  const char * key;
  int         multiplier;
  word        address; // the target address
} JsonEntry;

JsonEntry pvData[] = {
  { "GridFrequency",100,0x2044 } ,
  { "L1ThreePhaseGridVoltage",10,0x2006 } ,
  { "L2ThreePhaseGridVoltage",10,0x2008 } ,
  { "L3ThreePhaseGridVoltage",10,0x200A } ,
  { "L1ThreePhaseGridOutputCurrent",1000,0x200C } ,
  { "L2ThreePhaseGridOutputCurrent",1000,0x200E } ,
  { "L3ThreePhaseGridOutputCurrent",1000,0x2010 } ,
  { "OutputPower",10,0x2012 } ,
  { "L1ThreePhaseGridOutputPower",10,0x2014 } ,
  { "L2ThreePhaseGridOutputPower",10,0x2016 } ,
  { "L3ThreePhaseGridOutputPower",10,0x2018 }
};
const size_t NUM_PVREGS = ARRAY_SIZE(pvData); 


// the MQTT inbound message callback
//
void readPV (char* topic, byte* payload, unsigned int length) {

  doc.clear();
  deserializeJson(doc, payload);

  if (doc.size() > 1) {
    // Led on, builtin leed = LOW on.
    LedOn(true); 
    for (size_t i=0; i< NUM_PVREGS; i++) {
      float val = 0;
      val = doc[pvData[i].key].as<float>() * pvData[i].multiplier;
      if (pvData[i].address == 0x2012) {
        Serial.printf("%s = %.1f\n", pvData[i].key,val);
      }
      PV.setReg(pvData[i].address,val);
    }
  }
}

/**
 * @brief reconnect to wifi, and/or configure
 * @param Force if true, we start config AP mode on demand witout a timeout
 * Else there is a 2 minute timeout, reason: if wifi goes away we otherwise would 
 * stay in autoconf (AP) mode. So in this case try to reconnect every 2 minutes
 */
bool shouldSaveConfig = false;  // flag indicating that we should save parameters in flash

// a separate Callback, since the way the Wifimanerg class defines the callback ( as a pointer), we cannot ue a lambda
// see c++ 
void saveCb() {
  Serial.println(F("Parameters changed, must save them"));
  shouldSaveConfig = true; 
}

// Automatic Wifi configuration mode
//
void WifiautoConnect(int force = false) {
  // local vars, when done no need to have them around
  WiFiManager   wm;
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqttserver, sizeof(mqttserver));
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqttport, sizeof(mqttport));
  WiFiManagerParameter custom_mqtt_topic("topic", "mqtt topic", mqtttopic, sizeof(mqtttopic));
  WiFiManagerParameter custom_rtu_address("address", "Modbus address", address, sizeof(address));

  //set config save notify callback. 
  // Problem: if call expetcs a function pointer, we can't use a lambda with capture so the use
  // of a lambda is pointless
  // so use g
  wm.setSaveConfigCallback(saveCb);

  //add all your parameters here
  wm.addParameter(&custom_mqtt_server);
  wm.addParameter(&custom_mqtt_port);
  wm.addParameter(&custom_mqtt_topic);
  wm.addParameter(&custom_rtu_address);
#ifdef PRODUCTION
  wm.setDebugOutput(false);
#endif

  // go into config mode , block or wat and retry 
  // this is usefull to prevent a block in AP mode when Wifi is offline
  // indefinite wait shoudl only happen whe nothing is configured
  LedOn(true);
  if (force) {
    Serial.println(F("Start AP and configuration mode (forced) "));
    wm.startConfigPortal(CFG_AP_NAME);
  } else {
    wm.setTimeout(120);
    Serial.println(F("Try to connect, if not goto AP and configuration mode for 2 minutes"));
    if (!wm.autoConnect(AC_AP_NAME)) {
      Serial.println(F("failed to connect and hit timeout, restart"));
      delay(3000);
      //reset and try again. 
      // Best to reset since otherwise we could remain serving autodated PV RTU data
      ESP.reset();
      delay(5000);
    }
  }

  // We have come out of AP mode and are connected to Wifi
  LedOn(false);

  Serial.print(F("Connected to SSID ")) ; Serial.println(WiFi.SSID());
  Serial.print(F("IP address ")) ; Serial.println(WiFi.localIP());

  // parameters changed ?
  if (shouldSaveConfig) {
    Serial.print(F("Saving MQTT and RTU parameters ")) ;
    strcpy(mqttserver, custom_mqtt_server.getValue());
    prefs.putString("mqttserver", mqttserver);

    strcpy(mqttport, custom_mqtt_port.getValue());
    prefs.putString("mqttport",mqttport);

    strcpy(mqtttopic, custom_mqtt_topic.getValue());
    prefs.putString("mqtttopic", mqtttopic);
    
    strcpy(address, custom_rtu_address.getValue());
    prefs.putString("address", address);

    shouldSaveConfig = false;
  }
  
  WiFi.setAutoReconnect(true);
}

// (Re) connect to MQTT broker with known parameters
//
bool reConnectMQTT() {

  int numtries = 40;
  Serial.print(F("(re)connecting to MQTT broker ")); Serial.print(mqttserver);
  Serial.print(F(" on port ")); Serial.println(mqttport);

  while (!mqtt.connected() && numtries-- > 0) {
    delay(250);
    mqtt.connect(MQTT_CLIENT_ID);
    Serial.print(".");
  }
  
  if (!mqtt.connected()) {
    Serial.println(F("\nMQTT connect Timeout!"));
    WifiautoConnect(true);
    return false;
  }
  Serial.println(" OK");

  // Subscribe to a topic, the incoming messages are processed by messageHandler() function
  Serial.print(F("Subscribe to topic ")); Serial.print(mqtttopic);
  if (mqtt.subscribe(mqtttopic))
    Serial.println(F(" : OK"));
  else {
    Serial.println(F(" : Failed"));
    return false;
  }
  Serial.println(F("MQTT broker Connected!"));
  return true;
}

// Standard code from Arduino OTA
// 
void setupOTA() {

	ArduinoOTA.setHostname(HOSTNAME);

	// MD5("admin") = 21232f297a57a5a743894a0e4a801fc3
	ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

	ArduinoOTA.onStart([]() {
		String type;
		type = ArduinoOTA.getCommand() == U_FLASH ? "sketch" : "filesystem";
		Serial.println("Start updating " + type);
	});

	ArduinoOTA.onEnd([]() { Serial.println("\nEnd"); });

	ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
		Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
	});

	ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error[%u]", error);
    });

	ArduinoOTA.begin();
	Serial.println(F("OTA Ready"));
}

// 
void setup() {

  Serial.begin(115200);
  Serial.println(F("Modbus DTSU666 PV emulator V 1.0"));

  pinMode(LEDPIN,OUTPUT);
  LedOn(false);

  pinMode(BUTTON,INPUT_PULLUP);
  
  prefs.begin("DTSU666"); // use "dtsu" namespace
  // get stored persistent values. If nothing there? Goto config mode and stay there
  if (! (prefs.isKey("mqttserver") && prefs.isKey("mqttport") 
        && prefs.isKey("mqtttopic") && prefs.isKey("address"))) {
    WifiautoConnect(true);
  } else {
    // aparently we have been configured in the past, so all should work
    prefs.getString("mqttserver", mqttserver, sizeof(mqttserver));
    prefs.getString("mqttport", mqttport, sizeof(mqttport));
    prefs.getString("mqtttopic", mqtttopic, sizeof(mqtttopic));
    prefs.getString("address", address, sizeof(address));
    // just keep trying until we are online
    WifiautoConnect(false);
  }

  // init Serial line and out Modbus RTU Slave
  S1.begin(9600, SWSERIAL_8N1);
  PV.begin(&S1,RE_DE1,String(address).toInt());
  PV.printRegs(0x0,11);

  // Connect to the MQTT broker
  mqtt.setBufferSize(2048);
  mqtt.setServer(mqttserver, String(mqttport).toInt());
  mqtt.setCallback(readPV);

  // Try to connect, if fails no problem because mainloop will retry 
  reConnectMQTT();
  
  setupOTA();

  Serial.println(F("Setup done "));

}

// Mainloop.
// check if button pressed, still connected to wifi and/or mqtt.
//
void loop() {

  static ulong lastcheck = 0;
  static ulong firstPressed = 0;
  ulong now = millis();

  // if button pressed loniger than 2 seconds, goto config mode
  if (digitalRead(BUTTON) == LOW) {
    if (firstPressed == 0) {
      firstPressed = now; 
    } else if (now - firstPressed > 2000) {
      Serial.println(F("Button pressed > 2 seconds, start AP config mode"));  
      LedOn(true);
      WifiautoConnect(true);
    }
  } else {
    firstPressed = 0;
  }
  
  // switch led off if on
  if (ledOnSince > 0 && now - ledOnSince > 150 ) {
    LedOn(false);
  }

  if (now - lastcheck > CHECK_INTERVAL) {
    // not connected? TRy to reconnect . If that fails the unit will restart,
    // there is no point keep serving modbus requests with outdated data
    if (!WiFi.isConnected())  WifiautoConnect(120);
    if (!mqtt.connected())    reConnectMQTT();
      //PV.printRegs(0x2006,20);
    lastcheck = now;
  }
  ArduinoOTA.handle();
  mqtt.loop();
  PV.task();
  yield();
}
