#include <SPI.h> 
#include <LoRa.h>
#include "DHTesp.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>  // Include for JSON parsing

DHTesp dht;

// Pin Definitions
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
const char* fireAlertUrl = "http://192.168.1.67:5000/fire-alert";  // Fire alert endpoint

const int HUB_ID = 1;  
bool fireDetected = false;
bool directionReceived = false;
String receivedJsonDirection;
unsigned long lastRequestTime = 0;
const unsigned long requestCooldown = 10000;  // 10 seconds cooldown

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

    http.begin(fireAlertUrl);
    http.setTimeout(15000);  // Increase timeout to 15 seconds
    http.addHeader("Content-Type", "application/json");

    String jsonPayload = "{\"esp_id\":\"ESP1\"}";
    Serial.println("📤 Payload: " + jsonPayload);

    int httpResponseCode = http.POST(jsonPayload);
    
    Serial.print("🔍 HTTP Response Code: ");
    Serial.println(httpResponseCode);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("✅ Server Response: " + response);
      processEvacuationDirection(response);
    } else {
      Serial.println("❌ Error sending POST: " + String(httpResponseCode));
      Serial.println("🛑 Possible causes: Server Down, Incorrect URL, or Network Issues.");
    }

    http.end();
  } else {
    Serial.println("⚠️ WiFi not connected, cannot send request.");
  }
}

// Process Evacuation Direction and Turn on LEDs Accordingly
void processEvacuationDirection(String jsonResponse) {
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, jsonResponse);

  if (!error && doc.containsKey("direction")) {
    String direction = doc["direction"];

    // Reset all LEDs first
    digitalWrite(leftLED, LOW);
    digitalWrite(rightLED, LOW);
    digitalWrite(dangerLED, LOW);

    // Activate LED based on received direction
    if (direction == "left") {
      digitalWrite(leftLED, HIGH);
      Serial.println("⬅️ Left LED ON");
    } else if (direction == "right") {
      digitalWrite(rightLED, HIGH);
      Serial.println("➡️ Right LED ON");
    } else {
      digitalWrite(dangerLED, HIGH);
      Serial.println("⚠️ No Safe Path! Danger LED ON");
    }
    directionReceived = true;
  } else {
    Serial.println("❌ Error parsing JSON or missing 'direction' key");
  }
}

void loop() {
  digitalWrite(LED1, LOW);
  
  // **FORCE SENSOR VALUES TO UPDATE**
  int infrared_value1 = analogRead(flame1);
  int mq_value = analogRead(MQ135_PIN);
  TempAndHumidity data = dht.getTempAndHumidity();
  float temperature = data.temperature;
  float humidity = data.humidity;

  // **DEBUG: Print Sensor Values**
  /*Serial.print("\n🔥 Sensor Readings -> ");
  Serial.print("Flame: "); Serial.print(infrared_value1);
  Serial.print(" | Gas: "); Serial.print(mq_value);
  Serial.print(" | Temp: "); Serial.print(temperature);
  Serial.print(" | Humidity: "); Serial.println(humidity);*/

  // ✅ **RESET ESP WHEN FIRE IS GONE**  
  if (fireDetected && (infrared_value1 >= 2000 && mq_value <= 2000 || (temperature <= 50 && humidity >= 30))) {
    Serial.println("✅ Fire Cleared! Resetting System.");
    
    // RESET FIRE DETECTION SYSTEM
    fireDetected = false;
    directionReceived = false;

    // TURN OFF ALL LEDs
    digitalWrite(LED1, LOW);
    digitalWrite(leftLED, LOW);
    digitalWrite(rightLED, LOW);
    digitalWrite(dangerLED, LOW);

    delay(500);  // Small delay to prevent false triggering
  }

  // ✅ **IF NEW FIRE DETECTED AFTER RESET, PROCESS IT**
  if (!fireDetected && (infrared_value1 < 2000 || mq_value > 2000 || temperature > 50 || humidity < 30)) {
    fireDetected = true;  // Mark fire detected
    directionReceived = false; // Reset direction reception flag

    for (int i = 0; i < 5; i++) {
      Serial.println("🔥 Fire detected! Sending alert...");
      digitalWrite(LED1, HIGH);
      LoRa.beginPacket();
      LoRa.print("N1:fire");
      LoRa.endPacket();
      delay(500);
    }

    sendFireAlert();  // Send the alert and request direction at the same time
  } else {
    digitalWrite(LED1, LOW);
    delay(500);
  }

  // ✅ **Listen for LoRa Messages (Fire Alerts from Other Nodes)**
  int packetSize = LoRa.parsePacket();
  while (packetSize) {
    String LoRaData = LoRa.readString();
    Serial.print("Received via LoRa: ");
    Serial.println(LoRaData);
    digitalWrite(LED1, HIGH);
    delay(2000);

    if (LoRaData.startsWith("N2:fire")) {
      Serial.println("🔥 Fire detected by Node 2!");
      digitalWrite(LED1, HIGH);
      delay(2000);
    }
    digitalWrite(LED1, LOW);
    delay(1000);
    packetSize = LoRa.parsePacket();
  }

  delay(500);
}
