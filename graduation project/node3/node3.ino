//node3 code
#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>  // Include for JSON parsing
#include "DHTesp.h"
DHTesp dht;
// Node 3 MAC address: 24:DC:C3:A8:27:20
// Node 2 MAC address: 24:DC:C3:A8:86:E0

// 2 node2 24:DC:C3:A8:7B:84

//uint8_t peerMACAddress[] = { 0x24, 0xDC, 0xC3, 0xA8, 0x7B, 0x84 };
uint8_t peerMACAddress[] = { 0x24, 0xDC, 0xC3, 0xA8, 0x86, 0xE0 };
// WiFi and Server Configuration
const char *ssid = "Maan_plus";
const char *password = "maan2143";
const char *serverUrl = "http://192.168.1.16:5000/fire-alert";  // Fire alert endpoint
const char *getPathUrl = "http://192.168.1.16:5000/get-path";   // Get evacuation path endpoint

const int HUB_ID = 3;  // Unique identifier for this hub
bool pathReceived = false;
String receivedJsonPath;
#define MQ135_PIN 32
#define DHTpin 4
int LED = 25;     // LED pin
int flame3 = 34;  // Flame sensor pin

int leftLED = 12;
int rightLED = 13;
int dangerLED = 27;

//bool fireDetected = false;

esp_now_peer_info_t peerInfo;
unsigned long lastMessageTime = 0;
const unsigned long timeout = 5000;  // 5 seconds timeout
// Callback function to handle received data from node3
void onDataReceived(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
  String receivedMessage = "";
  for (int i = 0; i < len; i++) {
    receivedMessage += (char)incomingData[i];
  }

  //Serial.print("Received Message: ");
  Serial.println(receivedMessage);

  if (receivedMessage == "fire") {
    // if node2 received "fire" from node3, turn LED2 on then Send "fire" message to node1
    Serial.println("Fire Detected by node2 !");
    lastMessageTime = millis();  // Reset timer on receiving a message
    digitalWrite(LED, HIGH);     // Turn on LED
    delay(2000);                 // LED stays on for 2 seconds
  } else {
    digitalWrite(LED, LOW);
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

  // Initialize LED and Flame Sensor
  pinMode(LED, OUTPUT);
  pinMode(leftLED, OUTPUT);
  pinMode(rightLED, OUTPUT);
  pinMode(dangerLED, OUTPUT);
  pinMode(flame3, INPUT);
  pinMode(MQ135_PIN, INPUT);
  dht.setup(DHTpin, DHTesp::DHT11);


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

  // Add peer
  memcpy(peerInfo.peer_addr, peerMACAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }
  Serial.println("Peer added successfully");

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
          digitalWrite(dangerLED, HIGH);
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

    String jsonPayload = "{\"esp_id\":\"ESP3\", \"location\":[273, 622]}";
    Serial.println("Sending payload: " + jsonPayload);

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
  // Read sensors
  int infrared_value = analogRead(flame3);
  int mq_value = analogRead(MQ135_PIN);
  TempAndHumidity data = dht.getTempAndHumidity();
  float temperature = data.temperature;
  float humidity = data.humidity;
if (pathReceived) {
    processEvacuationPath(receivedJsonPath);
    pathReceived = false;  // Reset after processing
  }
  // Evaluate conditions for fire detection
  if (infrared_value < 2000 || mq_value > 2000 || temperature > 50 || humidity < 30) {
    Serial.println("Node 3 Sending packet: fire");
    digitalWrite(LED, HIGH);
    //digitalWrite(leftLED, HIGH);

    //digitalWrite(rightLED, HIGH);

    //digitalWrite(dangerLED, HIGH);
    delay(2000);

    // Send "fire" message to peer
    const char *message = "fire";
    esp_now_send(peerMACAddress, (uint8_t *)message, strlen(message));
    delay(1000);  // Avoid spamming messages
    sendFireAlert();
    //digitalWrite(dangerLED, HIGH);
  } else {
    //Serial.println("Node 3 Sending packet: no fire");
    digitalWrite(LED, LOW);
    delay(500);
  }

  // Turn off LED if no message is received for a long time
  if (millis() - lastMessageTime > timeout) {
    digitalWrite(LED, LOW);
    //Serial.println("No message timeout: LED OFF");
  }
}