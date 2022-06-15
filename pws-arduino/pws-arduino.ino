#include <Wire.h>
#include <Adafruit_INA219.h>
#include <ArduinoJson.h> 
#include <ArduinoHttpClient.h>
#include <RTCZero.h>

#include "configuration.h"

#ifdef IS_WIFI_BOARD // WiFi Setup
  #include <WiFiNINA.h>
  #include "arduino_secrets.h"

  int wifi_status = WL_IDLE_STATUS;
  WiFiClient wifi;
  HttpClient http = HttpClient(wifi, SERVER_IP, SERVER_PORT);
#else // GSM Setup
  #include <MKRGSM.h>

  GSMClient client;
  GPRS gprs;
  GSM gsmAccess;

  HttpClient http = HttpClient(client, SERVER_IP, SERVER_PORT);
#endif

Adafruit_INA219 ina219;

// Table readouts
unsigned int previousMillis = 0;
const int chipSelect = 10;
float shuntvoltage = 0;
float busvoltage = 0;
float current_mA = 0;
float loadvoltage = 0;
float energy = 0;

RTCZero rtc;

#ifdef IS_WIFI_BOARD
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
#endif

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

  // Reconnect if needed
  connectToInternet();

  // Send request
  http.post(CREATE_URL, content_type, json);

  // Read response
  String response = http.responseBody();
  Serial.println(response);
}

void connectToInternet() {
#ifdef IS_WIFI_BOARD
  // attempt to connect to Wifi network:
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("Attempting to connect to network: ");
    Serial.println(SECRET_SSID);

    // Connect to WPA/WPA2 network
    wifi_status = WiFi.begin(SECRET_SSID, SECRET_PASS);

    // wait 10 seconds for connection
    delay(10000);
  }
#else
  Serial.println("Starting GSM connection...");
  boolean connected = gsmAccess.getStatus() == GSM_READY;

  while (!connected) {
    if ((gsmAccess.begin(PINNUMBER) == GSM_READY) &&
        (gprs.attachGPRS(GPRS_APN, GPRS_LOGIN, GPRS_PASSWORD) == GPRS_READY)) {
      connected = true;
    } else {
      Serial.println("Not connected, retrying...");
      delay(10000);
    }
  }
#endif
}

void setup() {
  Serial.begin(9600);
  //while(!Serial);
  delay(1000);
  ina219.begin();

  connectToInternet();

  // Set RTC clock over the internet
  rtc.begin();
  unsigned long epoch = 0;

  while (epoch == 0) {
    Serial.print("Attempting to get RTC epoch");

#ifdef IS_WIFI_BOARD
    epoch = WiFi.getTime();
#else
    epoch = gsmAccess.getTime();
#endif

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

  if (currentMillis - previousMillis >= SEND_INTERVAL_MS) {
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
  energy = energy + (loadvoltage * current_mA * SEND_INTERVAL_MS) / 1000 / 3600;
}
