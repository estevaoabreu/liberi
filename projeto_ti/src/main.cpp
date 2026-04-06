#include <Arduino.h>
#include <Wire.h>

void setup() {
  pinMode(13, OUTPUT); // Built-in LED
  Serial.begin(115200);
  Serial.println("SYSTEM BOOTING...");
  Wire.begin();
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