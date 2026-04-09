#include <Arduino.h>
#include <Wire.h>

// Pin Definitions
const int RED_PIN = 9;
const int GREEN_PIN = 10;
const int BLUE_PIN = 11;
const int BTN_PIN = 2;

// System Variables
bool systemActive = false;
unsigned long lastUpdate = 0;

// Helper function to set RGB Color
void setStatusColor(int r, int g, int b) {
  analogWrite(RED_PIN, r);
  analogWrite(GREEN_PIN, g);
  analogWrite(BLUE_PIN, b);
}

void setup() {
  Serial.begin(115200);
  Wire.begin();

  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  
  // Use INPUT_PULLUP so the button doesn't need a resistor
  pinMode(BTN_PIN, INPUT_PULLUP);

  setStatusColor(0, 0, 0); // Start OFF
  Serial.println("SYSTEM_BOOTED");
}

void loop() {
  // 1. Check Button for Power Toggle
  if (digitalRead(BTN_PIN) == LOW) {
    systemActive = !systemActive;
    
    if (!systemActive) {
      setStatusColor(0, 0, 0); // Turn off LED
      Serial.println("STATUS,OFF");
    } else {
      Serial.println("STATUS,ON");
    }
    delay(500); // Debounce delay
  }

  // 2. Only run monitoring if the system is ON
  if (systemActive) {
    if (millis() - lastUpdate > 1500) { // Update every 1.5 seconds
      lastUpdate = millis();

      // Read Temperature from MLX90614 (0x5A)
      Wire.requestFrom(0x5A, 1);
      int temp = Wire.available() ? Wire.read() : 0;

      // Read Vitals from MAX30102 (0x57) 
      // We read twice because the custom chip alternates SpO2 and Heart Rate
      Wire.requestFrom(0x57, 1);
      int vital1 = Wire.available() ? Wire.read() : 0;
      delay(50);
      Wire.requestFrom(0x57, 1);
      int vital2 = Wire.available() ? Wire.read() : 0;

      // --- SEND DATA TO NODE.JS APP ---
      // Format: DATA,temp,vital1,vital2
      Serial.print("DATA,");
      Serial.print(temp);
      Serial.print(",");
      Serial.print(vital1);
      Serial.print(",");
      Serial.println(vital2);

      // --- LOGIC FOR RGB LED STATUS ---
      
      // CRITICAL: Temp >= 40 OR either vital reading < 90 (Critical Heart/Oxygen)
      if (temp >= 40 || (vital1 < 90 && vital1 > 0) || (vital2 < 90 && vital2 > 0)) {
        setStatusColor(255, 0, 0); // SOLID RED
      } 
      // WARNING: Temp 38-39 OR vital reading 90-94
      else if (temp >= 38 || (vital1 < 95 && vital1 > 0) || (vital2 < 95 && vital2 > 0)) {
        setStatusColor(255, 100, 0); // ORANGE/YELLOW
      } 
      // IDEAL: Temp 36-37 AND Vitals >= 95
      else {
        setStatusColor(0, 255, 0); // SOLID GREEN
      }
    }
  }
}