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

  Serial.println("--- INFANT MONITOR INITIALIZED ---");
  Serial.println("Press Button to Start...");
  setStatusColor(0, 0, 0); // Start OFF
}

void loop() {
  // 1. Check Button for Power Toggle
  if (digitalRead(BTN_PIN) == LOW) {
    systemActive = !systemActive;
    Serial.print("SYSTEM STATUS: ");
    Serial.println(systemActive ? "ON" : "OFF");
    
    if (!systemActive) {
      setStatusColor(0, 0, 0); // Turn off LED if system is off
    }
    delay(500); // Simple debounce to prevent double-toggle
  }

  // 2. Only run monitoring if the system is ON
  if (systemActive) {
    if (millis() - lastUpdate > 1500) { // Update every 1.5 seconds
      lastUpdate = millis();

      // Read Temperature from MLX90614 (0x5A)
      Wire.requestFrom(0x5A, 1);
      int temp = Wire.available() ? Wire.read() : 0;

      // Read Vitals from MAX30102 (0x57) 
      // Since our custom chip alternates SpO2 and Heart Rate, we read twice
      Wire.requestFrom(0x57, 1);
      int vital1 = Wire.available() ? Wire.read() : 0;
      delay(50);
      Wire.requestFrom(0x57, 1);
      int vital2 = Wire.available() ? Wire.read() : 0;

      Serial.print("Temp: "); Serial.print(temp);
      Serial.print(" | Vitals: "); Serial.print(vital1); 
      Serial.print(" / "); Serial.println(vital2);

      // --- LOGIC FOR RGB LED STATUS ---
      
      // CRITICAL: Temp >= 40 OR either vital reading < 90 (Critical Heart/Oxygen)
if (temp >= 40 || vital1 < 60 || vital2 < 60) { 
    // CRITICAL (Red) if Heart Rate < 60 or Temp > 40
    setStatusColor(255, 0, 0); 
} 
else if (temp >= 38 || vital1 < 90 || vital2 < 90) { 
    // WARNING (Orange) if Vitals drop below 90
    setStatusColor(255, 100, 0); 
} 
else { 
    // GREEN (Healthy) - Now 72 and 98 will both fall here!
    setStatusColor(0, 255, 0); 
}
    }
  }
}