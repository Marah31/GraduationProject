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
const char *serverUrl = "http://192.168.1.16:5000/fire-alert";  // Fire alert endpoint
const char *getPathUrl = "http://192.168.1.16:5000/get-path";   // Get evacuation path endpoint

const int HUB_ID = 2;  // Unique identifier for this hub
bool pathReceived = false;
String receivedJsonPath;
//bool fireDetected = false;
esp_now_peer_info_t peerInfo;
unsigned long lastMessageTime = 0;
const unsigned long timeout = 5000;  // 5 seconds timeout

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
  while (!Serial)
    ;
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


void requestEvacuationPath() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    Serial.println("Requesting evacuation path from: " + String(getPathUrl));

    http.begin(getPathUrl);
    http.setTimeout(10000);
    http.addHeader("Content-Type", "application/json");

    int httpResponseCode = http.GET();

    Serial.print("Path Request HTTP Response Code: ");
    Serial.println(httpResponseCode);

    if (httpResponseCode > 0) {
      receivedJsonPath = http.getString();
      Serial.println("Received Path Data: " + receivedJsonPath);
      pathReceived = true;  // Set flag to process path
    } else {
      Serial.println("Error retrieving path: " + String(httpResponseCode));
    }

    http.end();
  } else {
    Serial.println("WiFi not connected, cannot request path.");
  }
}

void processEvacuationPath(String jsonResponse) {
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, jsonResponse);
  
  if (!error && doc.containsKey("path")) {
    JsonArray path = doc["path"];
    
    // Reset LEDs
    digitalWrite(leftLED, LOW);
    digitalWrite(rightLED, LOW);
    digitalWrite(dangerLED, LOW);

    bool foundDirection = false;

    // Check coordinate changes
    for (int i = 1; i < path.size(); i++) {
      int prevX = path[i-1][0];
      int prevY = path[i-1][1];
      int currX = path[i][0];
      int currY = path[i][1];
      
      // Horizontal movement
      if (currY == prevY) {
        if (currX > prevX) {
          digitalWrite(rightLED, HIGH);
          Serial.println("Moving Right");
          foundDirection = true;
          break;
        } else if (currX < prevX) {
          digitalWrite(leftLED, HIGH);
          Serial.println("Moving Left");
          foundDirection = true;
          break;
        }
      }
      
      // Vertical movement
      if (currX == prevX) {
        if (currY > prevY) {
          //digitalWrite(dangerLED, HIGH);
          Serial.println("Moving Down (Potential Danger)");
          foundDirection = true;
          break;
        } else if (currY < prevY) {
          digitalWrite(leftLED, HIGH);
          digitalWrite(rightLED, HIGH);
          Serial.println("Moving Up (Safe Exit)");
          foundDirection = true;
          break;
        }
      }
    }

    // If no direction found, indicate danger
    if (!foundDirection) {
      //digitalWrite(dangerLED, HIGH);
      Serial.println("No safe path found! Danger LED ON");
    }

    delay(2000);  // Keep LED on for visibility
  } else {
    Serial.println("Error parsing evacuation path JSON or missing 'path' key");
    //digitalWrite(dangerLED, HIGH);
  }
}

void sendFireAlert() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    Serial.println(" Connecting to: " + String(serverUrl));

    http.begin(serverUrl);
    http.setTimeout(10000);  // Increase timeout to 10s
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Connection", "keep-alive");

    String jsonPayload = "{\"esp_id\":\"ESP2\", \"location\":[505, 383]}";
    Serial.println(" Sending payload: " + jsonPayload);

    int httpResponseCode = http.POST(jsonPayload);

    Serial.print(" HTTP Response Code: ");
    Serial.println(httpResponseCode);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println(" Server Response: " + response);
      pathReceived = true;
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
  //LoRa.begin(868E6);
  int infrared_value1 = analogRead(flame2);
  int mq_value = analogRead(MQ135_PIN);
  TempAndHumidity data = dht.getTempAndHumidity();
  float temperature = data.temperature;
  float humidity = data.humidity;
  if (pathReceived) {
    processEvacuationPath(receivedJsonPath);
    pathReceived = false;  // Reset after processing
  }
  if (infrared_value1 < 2000 || mq_value > 2500 || temperature > 50 || humidity < 30) {
    for (int i = 0; i < 8; i++) {
      // Send LoRa packet to receiver
      LoRa.beginPacket();
      LoRa.print("N2:fire");
      LoRa.endPacket();
      //delay(2000);
      Serial.println("Node 2 Sending packet to node 1: fire");
      digitalWrite(LED2, HIGH);
      //digitalWrite(leftLED, HIGH);

      //digitalWrite(rightLED, HIGH);

      //digitalWrite(dangerLED, HIGH);

      const char *message = "fire";
      esp_now_send(node3MACAddress, (uint8_t *)message, strlen(message));
      //delay(1000);  // Avoid spamming messages
    }
    sendFireAlert();
    //digitalWrite(dangerLED, HIGH);
  } else {
    digitalWrite(LED2, LOW);
    delay(500);
  }

  // Listen for incoming messages
  int packetSize = LoRa.parsePacket();
  while (packetSize) {
    String LoRaData = LoRa.readString();
    Serial.print("Received via LoRa: ");
    Serial.println(LoRaData);

    if (LoRaData.startsWith("N1:fire")) {
      const char *message = "fire";
      Serial.println("Fire detected by Node 1!");
      digitalWrite(LED2, HIGH);
      esp_now_send(node3MACAddress, (uint8_t *)message, strlen(message));

      delay(2000);
    }
    packetSize = LoRa.parsePacket();  // Check if more messages exist
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