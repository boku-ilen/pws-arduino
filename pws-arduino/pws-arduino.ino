#include <Wire.h>
#include <Adafruit_INA219.h>
#include <WiFiNINA.h>
#include <ArduinoJson.h> 
#include <ArduinoHttpClient.h>
#include <RTCZero.h>

#include "arduino_secrets.h"

#define TABLE_ID 1
#define CREATE_URL "/tables/create/"
#define SERVER_IP "192.168.208.192"
#define SERVER_PORT 8000

char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)
int status = WL_IDLE_STATUS;     // the Wifi radio's status

Adafruit_INA219 ina219;

// Table readouts
unsigned int previousMillis = 0;
const int chipSelect = 10;
float shuntvoltage = 0;
float busvoltage = 0;
float current_mA = 0;
float loadvoltage = 0;
float energy = 0;

const unsigned int interval = 5000;

// Internet connection
WiFiClient wifi;
HttpClient http = HttpClient(wifi, SERVER_IP, SERVER_PORT);

RTCZero rtc;

void printWifiData() {
  Serial.println("Board Information:");

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  Serial.println();
  Serial.println("Network Information:");
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.println(rssi);
}

void sendTableUpdate() {
  DynamicJsonDocument doc(1024);

  doc["table_id"] = TABLE_ID;
  doc["timestamp"]   = rtc.getEpoch();
  doc["energy_production"] = current_mA;
  doc["battery_charge"] = 0.8; // TODO
  doc["port_usage"][0] = 0; // TODO
  doc["port_usage"][1] = 1; // TODO
  doc["port_usage"][2] = 0; // TODO

  // Print JSON to console
  serializeJson(doc, Serial);
  Serial.print("\n");

  // Build request
  String json;
  String content_type = "application/x-www-form-urlencoded";
  serializeJson(doc, json);

  // Send request
  http.post(CREATE_URL, content_type, json);

  // Read response
  String response = http.responseBody();
  Serial.println(response);
}

void setup() {
  Serial.begin(9600);
  //while(!Serial);
  delay(1000);
  ina219.begin();

  // attempt to connect to Wifi network:
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to network: ");
    Serial.println(ssid);

    // Connect to WPA/WPA2 network
    status = WiFi.begin(ssid, pass);

    // wait 10 seconds for connection
    delay(10000);
  }

  printWifiData();

  // Set RTC clock by WiFi
  rtc.begin();
  unsigned long epoch = 0;

  while (epoch == 0) {
    Serial.print("Attempting to get RTC epoch");

    epoch = WiFi.getTime();

    // wait 10 seconds for connection
    delay(10000);
  }

  Serial.print("Epoch received: ");
  Serial.println(epoch);
  rtc.setEpoch(epoch);
  Serial.println();
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    
    ina219values();
    
    sendTableUpdate();
  }
}

void ina219values() {
  shuntvoltage = ina219.getShuntVoltage_mV();
  busvoltage = ina219.getBusVoltage_V();
  current_mA = ina219.getCurrent_mA();
  loadvoltage = busvoltage + (shuntvoltage / 1000);
  energy = energy + (loadvoltage * current_mA * interval) / 1000 / 3600;
}
