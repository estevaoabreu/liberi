#include <Arduino.h>
#include <Wire.h>

// Pin Definitions updated to match your latest diagram.json
const int RED_PIN   = 27;   // Connected to GPIO 27
const int GREEN_PIN = 26;   // Connected to GPIO 26
const int BLUE_PIN  = 25;   // Connected to GPIO 25
const int BTN_PIN   = 17;   // UPDATED: Connected to GPIO 17

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
  
  // Initialize I2C explicitly on Pins 21 (SDA) and 22 (SCL)
  Wire.begin(21, 22); 

  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  
  // Button is connected to Ground, so INPUT_PULLUP is required
  pinMode(BTN_PIN, INPUT_PULLUP);

  setStatusColor(0, 0, 0); // Start OFF
  Serial.println("SYSTEM_BOOTED");
}

void loop() {
  // 1. Check Button for Power Toggle
  // Pressed = LOW because of Ground connection
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

  // 2. Monitoring logic
  if (systemActive) {
    if (millis() - lastUpdate > 1500) {
      lastUpdate = millis();

      // Read Temperature from MLX90614 (0x5A)
      Wire.requestFrom(0x5A, 1);
      int temp = Wire.available() ? Wire.read() : 0;

      // Read Vitals from MAX30102 (0x57) 
      Wire.requestFrom(0x57, 1);
      int vital1 = Wire.available() ? Wire.read() : 0;
      delay(50);
      Wire.requestFrom(0x57, 1);
      int vital2 = Wire.available() ? Wire.read() : 0;

      Serial.print("DATA,");
      Serial.print(temp);
      Serial.print(",");
      Serial.print(vital1);
      Serial.print(",");
      Serial.println(vital2);
      
      // Logic for RGB LED
      if (temp >= 40 || (vital1 < 90 && vital1 > 0) || (vital2 < 90 && vital2 > 0)) {
        setStatusColor(255, 0, 0); // RED
      } else if (temp >= 38 || (vital1 < 95 && vital1 > 0) || (vital2 < 95 && vital2 > 0)) {
        setStatusColor(255, 100, 0); // ORANGE
      } else {
        setStatusColor(0, 255, 0); // GREEN
      }
    }
  }
}