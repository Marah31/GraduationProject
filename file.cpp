// We have to test all of these in Arduino Ide.
// Arduino IoT Cloud Code


/*
#include "thingProperties.h"

// Define LED pins for entrances
const int numEntrances = 3;
int ledGreenPins[numEntrances] = {2, 4, 6}; // Green LEDs for each entrance
int ledRedPins[numEntrances] = {3, 5, 7};   // Red LEDs for each entrance

void setup() {
  // Initialize IoT Cloud
  initProperties();
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);

  // Initialize LED pins
  for (int i = 0; i < numEntrances; i++) {
    pinMode(ledGreenPins[i], OUTPUT);
    pinMode(ledRedPins[i], OUTPUT);
    digitalWrite(ledGreenPins[i], LOW);
    digitalWrite(ledRedPins[i], LOW);
  }

  // Debug output
  Serial.begin(9600);
  delay(1000);
  Serial.println("Arduino IoT Cloud ready...");
}

void loop() {
  // Update IoT Cloud
  ArduinoCloud.update();

  // Check if the path is updated
  if (pathUpdated) {
    pathUpdated = false; // Reset the flag
    Serial.print("Received Safe Entrance ID: ");
    Serial.println(safeEntranceID);

    // Update LED status
    updateLEDs(safeEntranceID);
  }
}

// Function to update LED states
void updateLEDs(int safeEntrance) {
  for (int i = 0; i < numEntrances; i++) {
    if (i == safeEntrance) {
      // Nearest safe exit: Green LED ON, Red LED OFF
      digitalWrite(ledGreenPins[i], HIGH);
      digitalWrite(ledRedPins[i], LOW);
    } else {
      // Other exits: Green LED OFF, Red LED ON
      digitalWrite(ledGreenPins[i], LOW);
      digitalWrite(ledRedPins[i], HIGH);
    }
  }
}

*/