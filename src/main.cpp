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
 * Todo: add preferences, AP config mode and OTA 
 */
#include <Arduino.h>
#include <DTSU666.h>
#include <ESP8266WiFi.h>
#include <MQTTClient.h> 
#include <ArduinoJson.h>

// Max485 module, We use 3v3, should be 5v but this works fine for not very long lines.
#define TX1 D7  // 485 DI Pin
#define RX1 D6  // 485 RO pin
#define RE_DE1 D2  // 485 combined RE DE

#define TX2 D0  // 485 DI Pin
#define RX2 D5  // 485 RO pin
#define RE_DE2 D1  // 485 combined RE DE

#define SLAVE_ID_1  0x15
#define SLAVE_ID_2  1
#define SOURCE_ID   1
#define PV_REFRESH        5432  // milliseconds

// MQTT section. Lster: preferences
const char * MQTT_BROKER_ADRRESS  = "diskstation.kpn";
const int    MQTT_PORT            = 1883;
const char * MQTT_CLIENT_ID       = "DTSU666-emulator";
const char * MQTT_USERNAME        = "";                        
const char * MQTT_PASSWORD        = "";
const char * SUBSCRIBE_TOPIC      = "pvdata/#";             
const char * ssid                 = "WifivanSimi2,4G";
const char * password             = "2xButJptrzyr";
const char * hostname             = "dtsu666PV.kpn";

WiFiClient network;
MQTTClient mqtt = MQTTClient(2048);

// Declare the meters and serial lines
// DTSU666 Power(SLAVE_ID_1);
SoftwareSerial S1(RX1,TX1);
//DTSU666 Source;
DTSU666 PV(SLAVE_ID_1);
JsonDocument doc;

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

bool newMessage = false;
// reads data from PV, to store in relevant registers
//
bool readPV(String & topic, String & message) {

  doc.clear();
  deserializeJson(doc, message);

  for (size_t i=0; i< NUM_PVREGS; i++) {
    float val = 0;
    val = doc[pvData[i].key].as<float>() * pvData[i].multiplier;
    if (pvData[i].address == 0x2012) {
      Serial.print("OutputPower = ") ; Serial.println(val,2);
    }
    PV.setReg(pvData[i].address,val);
  }
  newMessage = true;
  return true;
}

// connect to broker
//
bool reConnectMQTT() {

  Serial.print(F("(re)connecting to MQTT broker"));

  while (!mqtt.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD)) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();
  if (!mqtt.connected()) {
    Serial.println(F("MQTT broker Timeout!"));
    return false;
  }

  // Subscribe to a topic, the incoming messages are processed by messageHandler() function
  if (mqtt.subscribe(SUBSCRIBE_TOPIC))
    Serial.print(F("Subscribed to the topic: "));
  else {
    Serial.println(F("Failed to subscribe "));
  }
  Serial.println(SUBSCRIBE_TOPIC);
  Serial.println(F("MQTT broker Connected!"));
  return true;
}

// 
void setup() {

  Serial.begin(115200);
  Serial.println(F("Modbus DTSU666 PV emulator V 1.0"));

  WiFi.setHostname(hostname);
  WiFi.begin(ssid, password);
  Serial.print(F("Connecting to ")); Serial.println(ssid);
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  WiFi.setAutoReconnect(true);
  Serial.println("");
  Serial.print(F("Connected to WiFi network with IP Address: "));
  Serial.println(WiFi.localIP());

  // init Serial lines
  S1.begin(9600, SWSERIAL_8N1);
  PV.begin(&S1,RE_DE1);
  PV.printRegs(0x0,11);

  // Connect to the MQTT broker
  mqtt.begin(MQTT_BROKER_ADRRESS, MQTT_PORT, network);
  // Register a handler for incoming messages
  mqtt.onMessage(readPV);

  // Try to connect, if fails mainloop will retry 
  reConnectMQTT();
  
  Serial.println(F("Setup done "));

}

ulong lastread = 0;
void loop() {

  if (millis() - lastread > PV_REFRESH) { 
    if (!mqtt.connected())  reConnectMQTT();
      //PV.printRegs(0x2006,20);
    lastread = millis();
  }
  mqtt.loop();
  PV.task();
  yield();
}
