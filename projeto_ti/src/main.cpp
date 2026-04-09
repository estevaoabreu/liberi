#include <Arduino.h>
#include <Wire.h>

void setup() {
  pinMode(13, OUTPUT);
  Serial.begin(115200);
  Wire.begin();
}

void loop() {
  byte error;
  int bpm = 0;
  float temp = 0.0f;

  Wire.beginTransmission(0x5A);
  error = Wire.endTransmission();

  if (error == 0) {
    Wire.requestFrom(0x5A, 1);
    if (Wire.available()) {
      temp = static_cast<float>(Wire.read());
    }
  }

  Wire.beginTransmission(0x57);
  error = Wire.endTransmission();

  if (error == 0) {
    Wire.requestFrom(0x57, 1);
    if (Wire.available()) {
      bpm = static_cast<int>(Wire.read());
    }
  }

  Serial.print("{\"bpm\": ");
  Serial.print(bpm);
  Serial.print(", \"temp\": ");
  Serial.print(temp, 1);
  Serial.println("}");

  delay(2000);
}