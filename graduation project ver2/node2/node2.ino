#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <esp_now.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "DHTesp.h"
DHTesp dht;
// Node 3 MAC address: 24:DC:C3:A8:27:20
// Node 2 MAC address: 24:DC:C3:A8:86:E0
uint8_t node3MACAddress[] = { 0x24, 0xDC, 0xC3, 0xA8, 0x27, 0x20 };  // Node 3's MAC Address

// Define the pins used by the transceiver module
#define ss 5
#define rst 14
#define dio0 2
#define MQ135_PIN 32
#define DHTpin 4
int LED2 = 25;
int flame2 = 34;
int leftLED = 12;
int rightLED = 13;
int dangerLED = 27;

// WiFi and Server Configuration
const char *ssid = "Maan_plus";
const char *password = "maan2143";
/*const char *serverUrl = "http://192.168.1.16:5000/fire-alert";  // Fire alert endpoint
const char *getPathUrl = "http://192.168.1.16:5000/get-path";   // Get evacuation path endpoint
*/
const char* fireAlertUrl = "http://192.168.1.67:5000/fire-alert";  // Fire alert endpoint
const int HUB_ID = 2;  // Unique identifier for this hub
bool pathReceived = false;
String receivedJsonPath;
//bool fireDetected = false;
esp_now_peer_info_t peerInfo;
unsigned long lastMessageTime = 0;
const unsigned long timeout = 5000;  // 5 seconds timeout
bool fireDetected = false;
bool directionReceived = false;
String receivedJsonDirection;
unsigned long lastRequestTime = 0;
const unsigned long requestCooldown = 10000;  // 10 seconds cooldown
unsigned long lastLoRaMessageTime = 0;  // Timestamp of last received LoRa message
const unsigned long loRaTimeout = 10000;

// Callback function to handle received data from Node 3
void onDataReceived(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
  String receivedMessage = "";
  for (int i = 0; i < len; i++) {
    receivedMessage += (char)incomingData[i];
  }

  Serial.println(receivedMessage);

  if (receivedMessage == "fire") {
    for (int i = 0; i < 3; i++) {
      // If Node 2 received "fire" from Node 3, turn LED2 on and send "fire" to Node 1
      Serial.println("Fire Detected by Node 3!");
      lastMessageTime = millis();  // Reset timer on receiving a message
      digitalWrite(LED2, HIGH);    // Turn on LED
      delay(2000);                 // LED stays on for 2 seconds

      LoRa.beginPacket();
      LoRa.print("fire");
      LoRa.endPacket();
      Serial.println("Sending fire to Node 1!");
      sendFireAlert();
      delay(5000);
    }
    //delay(300);
  } else {
    digitalWrite(LED2, LOW);
    delay(500);
  }
}

// Callback function to confirm data was sent
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\nMessage Sent Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);
  while (!Serial);
  pinMode(LED2, OUTPUT);
  pinMode(leftLED, OUTPUT);
  pinMode(rightLED, OUTPUT);
  pinMode(dangerLED, OUTPUT);

  pinMode(flame2, INPUT);
  pinMode(MQ135_PIN, INPUT);
  dht.setup(DHTpin, DHTesp::DHT11);
  Serial.println("LoRa Transceiver - Node 2");


  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);
  Serial.println("ESP32 in Wi-Fi Station Mode");

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  Serial.println("ESP-NOW Initialized");

  // Register callback functions
  esp_now_register_recv_cb(onDataReceived);
  delay(2000);
  esp_now_register_send_cb(onDataSent);

  // Add Node 3 as a peer
  memcpy(peerInfo.peer_addr, node3MACAddress, 6);  // Add Node 3's MAC Address
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add Node 3 as peer");
    return;
  }
  Serial.println("Node 3 added successfully as peer");

  // Setup LoRa transceiver module
  LoRa.setPins(ss, rst, dio0);

  while (!LoRa.begin(868E6)) {
    Serial.println("Initializing LoRa...");
    delay(500);
  }
  LoRa.setSyncWord(0xF3);
  Serial.println("LoRa Initializing OK!");
  delay(2000);

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
    Serial.println("\n Sending Fire Alert...");

    http.begin(fireAlertUrl);
    http.setTimeout(30000);  // Increase timeout to 15 seconds
    http.addHeader("Content-Type", "application/json");

    String jsonPayload = "{\"esp_id\":\"ESP2\"}";
    Serial.println(" Payload: " + jsonPayload);

    int httpResponseCode = http.POST(jsonPayload);
    
    Serial.print(" HTTP Response Code: ");
    Serial.println(httpResponseCode);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println(" Server Response: " + response);
      processEvacuationDirection(response);
    } else {
      Serial.println(" Error sending POST: " + String(httpResponseCode));
      Serial.println(" Possible causes: Server Down, Incorrect URL, or Network Issues.");
    }

    http.end();
  } else {
    Serial.println(" WiFi not connected, cannot send request.");
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
      Serial.println(" Left LED ON");
      delay(5000);
    } else if (direction == "right") {
      digitalWrite(rightLED, HIGH);
      Serial.println(" Right LED ON");
      delay(5000);
    } else {
      digitalWrite(dangerLED, HIGH);
      Serial.println(" No Safe Path! Danger LED ON");
    }
    directionReceived = true;
  } else {
    Serial.println(" Error parsing JSON or missing 'direction' key");
  }
}

void loop() {
  //LoRa.begin(868E6);
  int infrared_value1 = analogRead(flame2);
  int mq_value = analogRead(MQ135_PIN);
  TempAndHumidity data = dht.getTempAndHumidity();
  float temperature = data.temperature;
  float humidity = data.humidity;

if (fireDetected && (infrared_value1 >= 2000 && mq_value <= 2000 || (temperature <= 50 && humidity >= 30))) {
    if (millis() - lastLoRaMessageTime > loRaTimeout) { 
      Serial.println(" Fire Cleared! Resetting System.");
      
      // RESET FIRE DETECTION SYSTEM
      fireDetected = false;
      directionReceived = false;

      // TURN OFF ALL LEDs
      digitalWrite(LED2, LOW);
      digitalWrite(leftLED, LOW);
      digitalWrite(rightLED, LOW);
      digitalWrite(dangerLED, LOW);

      delay(500);  // Small delay to prevent false triggering
    }
  }


  if (!fireDetected && (infrared_value1 < 2000 || mq_value > 2500 || temperature > 50 || humidity < 30)) {
    fireDetected = true;  // Mark fire detected
    directionReceived = false; // Reset direction reception flag
    for (int i = 0; i < 5; i++) {
      // Send LoRa packet to receiver
      Serial.println(" Fire detected! Sending alert...");
      LoRa.beginPacket();
      LoRa.print("N2:fire");
      LoRa.endPacket();
      delay(500);
      Serial.println("Node 2 Sending packet to node 1: fire");
      digitalWrite(LED2, HIGH);
      //digitalWrite(leftLED, HIGH);
      //digitalWrite(rightLED, HIGH);
      //digitalWrite(dangerLED, HIGH);
      const char *message = "fire";
      esp_now_send(node3MACAddress, (uint8_t *)message, strlen(message));
      //delay(1000);  // Avoid spamming messages
    }
    delay(5000);
    sendFireAlert();
    //digitalWrite(dangerLED, HIGH);
  } else {
    digitalWrite(LED2, LOW);
    delay(500);
  }

  // Listen for incoming messages
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String LoRaData = LoRa.readString();
    Serial.print("Received via LoRa: ");
    Serial.println(LoRaData);
    lastLoRaMessageTime = millis();  

    if (LoRaData.startsWith("N1:fire")) {
      const char *message = "fire";
      Serial.println("Fire detected by Node 1!");
      digitalWrite(LED2, HIGH);
      esp_now_send(node3MACAddress, (uint8_t *)message, strlen(message));

      delay(2000);
    }
    packetSize = LoRa.parsePacket();  // Check if more messages exist
    delay(5000);
    sendFireAlert();
  }

  /*if (packetSize) {
    while (LoRa.available()) {
      String LoRaData = LoRa.readString();
      Serial.print("Node 2 Received: ");
      Serial.println(LoRaData);

      if (LoRaData == "fire") {
        const char *message = "fire";
        esp_now_send(node3MACAddress, (uint8_t *)message, strlen(message));
        delay(2000);  // Avoid spamming messages
        Serial.println("Fire detected by node1 !");
        digitalWrite(LED2, HIGH);
        delay(2000);  // Keep LED on for 2 seconds
      }
    }
  }*/

  // Turn off LED if no message is received for a long time
  if (millis() - lastMessageTime > timeout) {
    digitalWrite(LED2, LOW);
    delay(200);
  }
}