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
  byte error;
  int address;

  // 1. Check MLX90614 (0x5A)
  Wire.beginTransmission(0x5A);
  error = Wire.endTransmission(); 

  if (error == 0) {
    Serial.print("MLX90614 detected! ");
    Wire.requestFrom(0x5A, 1);
    if (Wire.available()) {
      Serial.print("Temp: "); Serial.println(Wire.read());
    }
  } else {
    Serial.println("MLX90614 NOT found. Check SDA/SCL wiring or Chip ID.");
  }

  // 2. Check MAX30102 (0x57)
  Wire.beginTransmission(0x57);
  error = Wire.endTransmission();

  if (error == 0) {
    Serial.print("MAX30102 detected! ");
    Wire.requestFrom(0x57, 1);
    if (Wire.available()) {
      Serial.print("Heart: "); Serial.println(Wire.read());
    }
  } else {
    Serial.println("MAX30102 NOT found. Check SDA/SCL wiring or Chip ID.");
  }

  delay(2000);
}