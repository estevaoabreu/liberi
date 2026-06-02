#include <Arduino.h>
#include <Wire.h>

// Pin Definitions for Arduino Uno (Wokwi Simulator)
const int RED_PIN = 9;
const int GREEN_PIN = 10;
const int BLUE_PIN = 11;
const int BTN_PIN = 2;

// System Variables
volatile bool systemActive = false;
bool lastSystemActive = false;
volatile unsigned long lastDebounceTime = 0;

// Helper function to set RGB Color
void setStatusColor(int r, int g, int b) {
  analogWrite(RED_PIN, r);
  analogWrite(GREEN_PIN, g);
  analogWrite(BLUE_PIN, b);
}

void toggleSystem() {
  if (millis() - lastDebounceTime > 300) {
    systemActive = !systemActive;
    lastDebounceTime = millis();
  }
}

void setup() {
  Serial.begin(115200);
  delay(1500); // Give Serial Monitor time to connect after reset
  Wire.begin();

  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  
  // Use INPUT_PULLUP so the button doesn't need a resistor
  pinMode(BTN_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BTN_PIN), toggleSystem, FALLING);

  setStatusColor(0, 0, 0); // Start OFF
  Serial.println("SYSTEM_BOOTED");
  
  // Flash success
  setStatusColor(0, 255, 0);
  delay(500);
  setStatusColor(0, 0, 0);
}

void loop() {
  // 1. Check Button for Power Toggle
  if (systemActive != lastSystemActive) {
    lastSystemActive = systemActive;

    if (!systemActive) {
      Serial.println("STATUS,OFF");
      setStatusColor(0, 0, 0); // Turn off LED
    } else {
      Serial.println("STATUS,ON");
      setStatusColor(0, 0, 255); // Blue while calibrating
    }
  }

  // 2. Only run monitoring if the system is ON
  if (systemActive) {
    // Read Temperature from MLX90614 (0x5A)
    Wire.requestFrom(0x5A, 1);
    int rawTemp = Wire.available() ? Wire.read() : 0;

    // Read Vitals from MAX30102 (0x57)
    // We read twice because the custom chip alternates SpO2 and Heart Rate
    Wire.requestFrom(0x57, 1);
    int rawVital1 = Wire.available() ? Wire.read() : 0;
    delay(50);
    Wire.requestFrom(0x57, 1);
    int rawVital2 = Wire.available() ? Wire.read() : 0;

    // Map simulation temperature to healthy wrist temperature
    int temp = rawTemp;
    if (rawTemp >= 36 && rawTemp <= 37) {
      temp = rawTemp - 3; // 36-37 -> 33-34 (Healthy: 32-35)
    } else if (rawTemp >= 38 && rawTemp <= 39) {
      temp = rawTemp - 2; // 38-39 -> 36-37 (Warning)
    } else if (rawTemp >= 40) {
      temp = rawTemp - 2; // 40-42 -> 38-40 (Critical)
    }

    // De-alternate vitals from MAX30102 custom chip
    int hr = 0;
    int spo2 = 0;
    if (rawVital1 > 100) {
      hr = rawVital1;
      spo2 = rawVital2;
    } else if (rawVital2 > 100) {
      hr = rawVital2;
      spo2 = rawVital1;
    } else {
      // Both are <= 100, so the larger one is SpO2 (e.g. 98 vs 72)
      if (rawVital1 > rawVital2) {
        spo2 = rawVital1;
        hr = rawVital2;
      } else {
        spo2 = rawVital2;
        hr = rawVital1;
      }
    }

    // --- SEND DATA ---
    // Format: DATA,temp,hr,spo2
    String payload = "DATA," + String(temp) + "," + String(hr) + "," + String(spo2);
    Serial.println(payload);

    // --- LOGIC FOR RGB LED STATUS ---
    if (spo2 >= 95 && hr >= 50 && hr <= 130) {
      setStatusColor(0, 255, 0);  // Green: Good values
    } else {
      setStatusColor(255, 0, 0);  // Red: Bad/Critical values
    }

    // Delay to simulate a ~20Hz sample rate (50ms interval to not overwhelm the simulator)
    delay(50);
  } else {
    delay(100);
  }
}