/**
 * @file DTSU666 emulator
 * @author Michiel STeltman (git: michielfromNL, msteltman@disway.nl 
 * @brief  Emulates a bunch of DTSU666 meters over modbus RTU . Uses MQTT to get data
 * @version 0.1
 * @date 2024-06-17
 * 
 * @copyright Copyright (c) 2024, MIT license
 *  DTSU666 manual: https://www.solaxpower.com/uploads/file/dtsu666-user-manual-en.pdf
 * 
 * Todo: add OTA 
 */
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>  
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <DTSU666.h>

// Max485 module, We use 3v3, should be 5v but this works fine for not very long lines.
#define TX1     D7  // 485 DI Pin
#define RX1     D6  // 485 RO pin
#define RE_DE1  D2  // 485 combined RE DE

#define BUTTON  D5

// #define TX2 D0  // 485 DI Pin
// #define RX2 D5  // 485 RO pin
// #define RE_DE2 D1  // 485 combined RE DE

#define SLAVE_ID_1            0x15
#define CHECK_INTERVAL        5432  // milliseconds

// MQTT section. Lster: preferences
const char * HOSTNAME             = "dtsu666PV.local";
const char * AP_NAME              = "DTSU666PV_AP";
const char * MQTT_CLIENT_ID       = "ESP8266_DTSU666PV";                        

Preferences   prefs;
WiFiClient    wificlient;
PubSubClient  mqtt(wificlient);
JsonDocument  doc;

// the custome parameters strings for configuration
char mqttserver[40] = "diskstation.local";
char mqttport[6]    = "1883";
char mqtttopic[32]  = "pvdata/#";

// Declare the meters and serial lines
// DTSU666 Power(SLAVE_ID_1);
SoftwareSerial S1(RX1,TX1);
//DTSU666 Source;
DTSU666 PV(SLAVE_ID_1);

// define what daat we need from PV, and where to store it
typedef struct JsonEntry {
  const char * key;
  int         multiplier;
  word        address;
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

// the callback
//
ulong ledOn = 0;   // `flash time is too short, switch off in mainloop
void readPV (char* topic, byte* payload, unsigned int length) {

  doc.clear();
  deserializeJson(doc, payload);

  if (doc.size() > 1) {
    digitalWrite(LED_BUILTIN, LOW); ledOn = millis();
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

// reconnect to wifi, if not available goto AP config mode
// when called with "force", always got to AP mode to get MQTT params
//
bool shouldSaveConfig = false;
void saveCb() {
  Serial.println("Should save config");
  shouldSaveConfig = true; 
}
void reconnectWifi(int force = false) {
  // local vars, when done no need to have them around
  WiFiManager   wm;
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqttserver, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqttport, 6);
  WiFiManagerParameter custom_mqtt_topic("topic", "mqtt topic", mqtttopic, 32);

  //set config save notify callback. 
  // Problem: if call expetcs a function pointer, we can't use a lambda with capture so the use
  // of a lambda is pointless
  // so use g
  wm.setSaveConfigCallback(saveCb);

  //add all your parameters here
  wm.addParameter(&custom_mqtt_server);
  wm.addParameter(&custom_mqtt_port);
  wm.addParameter(&custom_mqtt_topic);

  // go into config mode , block or wat and retry 
  // this is usefull to prevent a block in AP mode when Wifi is offline
  // indefinite wait shoudl only happen whe nothing is configured
  if (force) {
    Serial.println(F("Start AP and configuration mode (forced) "));
    wm.startConfigPortal(AP_NAME);
  } else {
    wm.setTimeout(120);
    Serial.println(F("Try to connect, if not goto AP and configuration mode for 2 minutes"));
    if (!wm.autoConnect(AP_NAME)) {
      Serial.println(F("failed to connect and hit timeout, restart"));
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(5000);
    }
  }
  Serial.print(F("Connected to SSID ")) ; Serial.println(WiFi.SSID());
  Serial.print(F("IP address ")) ; Serial.println(WiFi.localIP());

  // parameters changed ?
  if (shouldSaveConfig) {
    Serial.print(F("Saving MQTT parameters ")) ;
    strcpy(mqttserver, custom_mqtt_server.getValue());
    prefs.putString("mqttserver", mqttserver);

    strcpy(mqttport, custom_mqtt_port.getValue());
    prefs.putString("mqttport",mqttport);

    strcpy(mqtttopic, custom_mqtt_topic.getValue());
    prefs.putString("mqtttopic", mqtttopic);
    
    shouldSaveConfig = false;
  }
}

// connect to broker
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
    reconnectWifi(true);
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

// 
void setup() {

  Serial.begin(115200);
  Serial.println(F("Modbus DTSU666 PV emulator V 1.0"));

  pinMode(LED_BUILTIN,OUTPUT);
  digitalWrite(LED_BUILTIN,HIGH);
  pinMode(BUTTON,INPUT_PULLUP);
  
  prefs.begin("DTSU666"); // use "dtsu" namespace
  // get stored persistent values. If nothing there? stay in AP mode
  if (! (prefs.isKey("mqttserver") && prefs.isKey("mqttport") && prefs.isKey("mqtttopic"))) {
    reconnectWifi(true);
  } else {
    // aparently we have been configured in the pat, so all should work
    prefs.getString("mqttserver", mqttserver, sizeof(mqttserver));
    prefs.getString("mqttport", mqttport, sizeof(mqttport));
    prefs.getString("mqtttopic", mqtttopic, sizeof(mqtttopic));
    // just keep trying until we are online
    reconnectWifi(false);
  }

  WiFi.setAutoReconnect(true);

  // init Serial lines
  S1.begin(9600, SWSERIAL_8N1);
  PV.begin(&S1,RE_DE1);
  PV.printRegs(0x0,11);

  // Connect to the MQTT broker
  mqtt.setBufferSize(2048);
  mqtt.setServer(mqttserver, String(mqttport).toInt());
  mqtt.setCallback(readPV);

  // Try to connect, if fails no problem mainloop will retry 
  reConnectMQTT();
  
  Serial.println(F("Setup done "));

}

ulong lastcheck = 0;
void loop() {

  if (digitalRead(BUTTON) == LOW) {
    Serial.println(F("Button pressed, going into config mode"));
    reconnectWifi(true);
  }
  // switch led off if on
  if (ledOn > 0 && millis() - ledOn > 250 ) {
    digitalWrite(LED_BUILTIN,HIGH);
    ledOn = 0;
  }

  if (millis() - lastcheck > CHECK_INTERVAL) {
    // not connected? TRy to reconnect . If that fails the unit will retsart,
    // there is no point keep serving modbus requests with outdated data
    if (!WiFi.isConnected())  reconnectWifi(120);
    if (!mqtt.connected())    reConnectMQTT();
      //PV.printRegs(0x2006,20);
    lastcheck = millis();
  }
  mqtt.loop();
  PV.task();
  yield();
}
