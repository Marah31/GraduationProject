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
const char* ssid = "Maan_plus";
const char* password = "maan2143";
const char* serverUrl = "http://192.168.1.67:5000/fire-alert";  
const char* getPathUrl = "http://192.168.1.67:5000/get-path";  

const int HUB_ID = 1;  
bool fireDetected = false;
bool pathRequested = false;
bool pathReceived = false;
String receivedJsonPath;
int maxAlarms = 5;  // Limit the number of alarms processed
int receivedAlarms = 0;
unsigned long lastRequestTime = 0;
const unsigned long requestCooldown = 10000;
void setup() {
  Serial.begin(115200);
  while (!Serial);

  pinMode(LED1, OUTPUT);
  pinMode(leftLED, OUTPUT);
  pinMode(rightLED, OUTPUT);
  pinMode(dangerLED, OUTPUT);

  pinMode(flame1, INPUT);
  pinMode(MQ135_PIN, INPUT);
  dht.setup(DHTpin, DHTesp::DHT11);

  Serial.println("LoRa Transceiver - Node 1");

  // Setup LoRa transceiver module
  LoRa.setPins(ss, rst, dio0);

  while (!LoRa.begin(868E6)) {
    Serial.println("Initializing LoRa...");
    delay(500);
  }

  LoRa.setSyncWord(0xF3);
  Serial.println("LoRa Initialized!");

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1500);
    Serial.print(".");
  }
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());
  Serial.println("\nWiFi connected");
}

void sendFireAlert() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    Serial.println("\n🚀 Sending Fire Alert...");

    String fullUrl = String(serverUrl);
    Serial.println("📡 Connecting to: " + fullUrl);

    http.begin(fullUrl);
    http.setTimeout(10000);
    http.addHeader("Content-Type", "application/json");

    String jsonPayload = "{\"esp_id\":\"ESP1\", \"location\":[689, 123]}";
    Serial.println("📤 Payload: " + jsonPayload);

    int httpResponseCode = http.POST(jsonPayload);
    
    Serial.print("🔍 HTTP Response Code: ");
    Serial.println(httpResponseCode);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("✅ Server Response: " + response);
    } else {
      Serial.println("❌ Error sending POST: " + String(httpResponseCode));
    }

    http.end();
  } else {
    Serial.println("⚠️ WiFi not connected, cannot send request.");
  }
}


// Function to request evacuation path **only after fire alert is sent**
void requestEvacuationPath() {
  if (WiFi.status() == WL_CONNECTED && fireDetected && pathRequested && millis() - lastRequestTime > requestCooldown) {
    HTTPClient http;
    Serial.println("\n🚨 Requesting Evacuation Path...");

    http.begin(getPathUrl);
    http.setTimeout(10000);
    http.addHeader("Content-Type", "application/json");

    int httpResponseCode = http.GET();
    Serial.print("Path Request HTTP Response Code: ");
    Serial.println(httpResponseCode);

    if (httpResponseCode > 0) {
      receivedJsonPath = http.getString();
      Serial.println("✅ Received Path Data: " + receivedJsonPath);
      pathReceived = true;
    } else {
      Serial.println("❌ Error retrieving path: " + String(httpResponseCode));
    }

    http.end();
    lastRequestTime = millis();  // Reset cooldown timer
  }
}

// Process and activate LEDs based on the received path
void processEvacuationPath(String jsonResponse) {
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, jsonResponse);
  
  if (!error && doc.containsKey("path")) {
    JsonArray path = doc["path"];

    digitalWrite(leftLED, LOW);
    digitalWrite(rightLED, LOW);
    digitalWrite(dangerLED, LOW);

    bool foundDirection = false;

    for (int i = 1; i < path.size(); i++) {
      int prevX = path[i-1][0];
      int prevY = path[i-1][1];
      int currX = path[i][0];
      int currY = path[i][1];

      // Horizontal movement
      if (currY == prevY) {
        if (currX > prevX) {
          digitalWrite(rightLED, HIGH);
          Serial.println("➡️ Moving Right");
          foundDirection = true;
          break;
        } else if (currX < prevX) {
          digitalWrite(leftLED, HIGH);
          Serial.println("⬅️ Moving Left");
          foundDirection = true;
          break;
        }
      }

      // Vertical movement
      if (currX == prevX) {
        if (currY > prevY) {
          Serial.println("⬇️ Moving Down (Potential Danger)");
          foundDirection = true;
          break;
        } else if (currY < prevY) {
          digitalWrite(leftLED, HIGH);
          digitalWrite(rightLED, HIGH);
          Serial.println("⬆️ Moving Up (Safe Exit)");
          foundDirection = true;
          break;
        }
      }
    }

    if (!foundDirection) {
      Serial.println("⚠️ No safe path found! Danger LED ON");
    }

    delay(2000);
  } else {
    Serial.println("❌ Error parsing evacuation path JSON or missing 'path' key");
  }
}


void loop() {
  //LoRa.begin(868E6);
  digitalWrite(LED1, LOW);
  int infrared_value1 = analogRead(flame1);
  int mq_value = analogRead(MQ135_PIN);
  TempAndHumidity data = dht.getTempAndHumidity();
  float temperature = data.temperature;
  float humidity = data.humidity;
/*if (!fireDetected){
    fireDetected = true;
    sendFireAlert();
}*/
  if (pathReceived) {
    processEvacuationPath(receivedJsonPath);
    pathReceived = false;  // Reset after processing
  }

  if (!fireDetected && (infrared_value1 < 2000 || mq_value > 2000 || temperature > 50 || humidity < 30)) {
    fireDetected = true;
    for (int i = 0; i < 3; i++) {
      Serial.println("Fire detected! Sending alert...");
      digitalWrite(LED1, HIGH);
      // Send LoRa packet to receiver
      LoRa.beginPacket();
      LoRa.print("N1:fire");
      //LoRa.print("fire");
      LoRa.endPacket();
    }
    sendFireAlert();
    // Send HTTP alert to server
    //sendFireAlert();
  } else {
    digitalWrite(LED1, LOW);
    delay(500);
  }
  if (fireDetected && !pathRequested) {
    requestEvacuationPath();
  }
  if (pathReceived) {
    processEvacuationPath(receivedJsonPath);
    pathReceived = false;
  }

  // Listen for incoming messages from LoRa
  int packetSize = LoRa.parsePacket();
  while (packetSize) {
    String LoRaData = LoRa.readString();
    Serial.print("Received via LoRa: ");
    Serial.println(LoRaData);
    digitalWrite(LED1, HIGH);
    delay(2000);

    if (LoRaData.startsWith("N2:fire")) {
      Serial.println("Fire detected by Node 2!");
      digitalWrite(LED1, HIGH);
      delay(2000);
    }
    digitalWrite(LED1, LOW);
    delay(1000);
    receivedAlarms++;  // Count received messages
    packetSize = LoRa.parsePacket();
  }
  /*if (packetSize) {
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
  }*/
   if (fireDetected && (infrared_value1 >= 2000 && mq_value <= 2000 && temperature <= 50 && humidity >= 30)) {
    Serial.println("✅ Fire Cleared! Resetting System.");
    fireDetected = false;
    pathRequested = false;
    digitalWrite(LED1, LOW);
    digitalWrite(leftLED, LOW);
    digitalWrite(rightLED, LOW);
    digitalWrite(dangerLED, LOW);
  }

  delay(500);
}