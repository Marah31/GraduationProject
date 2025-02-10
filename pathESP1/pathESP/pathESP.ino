#include <SPI.h>
#include <LoRa.h>
#include "DHTesp.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>  // Include for JSON parsing

DHTesp dht;

// Existing pin definitions
#define ss 5
#define rst 14
#define dio0 2
#define MQ135_PIN 32
#define DHTpin 4
int LED1 = 25;
int flame1 = 34;
int leftLED = 12;
int rightLED = 13;
int dangerLED = 27;

// WiFi and Server Configuration
const char* ssid = "Alsayed";
const char* password = "29111999";
const char* serverUrl = "http://192.168.1.16:5000/fire-alert";  // Fire alert endpoint
const char* getPathUrl = "http://192.168.1.16:5000/get-path";   // Get evacuation path endpoint

const int HUB_ID = 1;  // Unique identifier for this hub

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);
  while (!Serial)
    ;

  pinMode(LED1, OUTPUT);
  pinMode(leftLED, OUTPUT);
  pinMode(rightLED, OUTPUT);
  pinMode(dangerLED, OUTPUT);

  pinMode(flame1, INPUT);
  pinMode(MQ135_PIN, INPUT);
  dht.setup(DHTpin, DHTesp::DHT11);

  Serial.println("LoRa Transceiver - Node 1");

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1500);
    Serial.print(".");
  }
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());
  Serial.println("\nWiFi connected");

  // Setup LoRa transceiver module
  LoRa.setPins(ss, rst, dio0);

  while (!LoRa.begin(868E6)) {
    Serial.println("Initializing LoRa...");
    delay(500);
  }

  LoRa.setSyncWord(0xF3);
  Serial.println("LoRa Initialized!");
}

void sendFireAlert() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    Serial.println(" Connecting to: " + String(serverUrl));

    http.begin(serverUrl);
    http.setTimeout(10000);  // Increase timeout to 10s
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Connection", "keep-alive");

    String jsonPayload = "{\"esp_id\":\"ESP1\", \"location\":[689, 123]}";
    Serial.println(" Sending payload: " + jsonPayload);

    int httpResponseCode = http.POST(jsonPayload);

    Serial.print(" HTTP Response Code: ");
    Serial.println(httpResponseCode);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println(" Server Response: " + response);
    } else {
      Serial.println(" Error sending POST: " + String(httpResponseCode));
      Serial.println(" Check server URL and connection.");
    }

    http.end();
  } else {
    Serial.println(" WiFi not connected, cannot send request.");
  }
}

void loop() {
  int infrared_value1 = analogRead(flame1);
  int mq_value = analogRead(MQ135_PIN);
  TempAndHumidity data = dht.getTempAndHumidity();
  float temperature = data.temperature;
  float humidity = data.humidity;

  if (infrared_value1 < 2000 || mq_value > 2000 || temperature > 50 || humidity < 30) {
    for (int i = 0; i < 3; i++) {
      Serial.println("Fire detected! Sending alert...");
      digitalWrite(LED1, HIGH);

      /*digitalWrite(leftLED, HIGH);

      digitalWrite(rightLED, HIGH);

      digitalWrite(dangerLED, HIGH);*/


      // Send LoRa packet to receiver
      LoRa.beginPacket();
      LoRa.print("fire");
      LoRa.endPacket();
    }
    // Send HTTP alert to server
    sendFireAlert();
  } else {
    digitalWrite(LED1, LOW);
    delay(500);
  }

  // Listen for incoming messages
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    while (LoRa.available()) {
      String LoRaData = LoRa.readString();
      Serial.print("Received via LoRa: ");
      Serial.println(LoRaData);

      if (LoRaData.startsWith("Path:")) {
        Serial.println("Received Evacuation Path via LoRa!");
      }

      if (LoRaData == "fire") {
        delay(100);
        Serial.println("Fire detected by another node!");
        digitalWrite(LED1, HIGH);
        delay(2000);
      }
    }
  }
}
