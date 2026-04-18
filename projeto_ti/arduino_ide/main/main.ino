#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"

// Pins
const int RED_PIN = 9;
const int GREEN_PIN = 10;
const int BLUE_PIN = 11;
const int BTN_PIN = 2;

// Objects
Adafruit_MLX90614 mlx = Adafruit_MLX90614();
MAX30105 particleSensor;

// Buffers — reduced from 100 to 50 to save ~200 bytes of RAM
uint16_t irBuffer[50];
uint16_t redBuffer[50];
int32_t bufferLength = 50;
int32_t spo2;
int8_t validSPO2;
int32_t heartRate;
int8_t validHeartRate;

volatile bool systemActive = false;
unsigned long lastDebounceTime = 0;


void setStatusColor(int r, int g, int b) {
  analogWrite(RED_PIN, r);
  analogWrite(GREEN_PIN, g);
  analogWrite(BLUE_PIN, b);
}

void toggleSystem() {
  if (millis() - lastDebounceTime > 250) {
    systemActive = !systemActive;
    lastDebounceTime = millis();
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(100000); // Force 100 kHz for MLX90614 SMBus compatibility

  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  pinMode(BTN_PIN, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(BTN_PIN), toggleSystem, FALLING);

  // Initialize MLX90614 FIRST, before MAX30102 touches the bus
  delay(200);
  if (!mlx.begin()) {
    Serial.println(F("ERR: MLX90614 Fail"));
    setStatusColor(255, 0, 0);
    // Don't halt — MAX30102 may still work
  }

  // Initialize MAX30102 using STANDARD speed to avoid overriding Wire clock
  if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD)) {
    Serial.println(F("ERR: MAX30102 Fail"));
    setStatusColor(255, 0, 0);
    while (1); // Halt — SpO2/HR cannot function without this sensor
  }

  // Re-assert 100 kHz after MAX30102 init, in case the library changed it
  Wire.setClock(100000);

  // MAX30102 settings
  byte ledBrightness = 60;
  byte sampleAverage = 4;
  byte ledMode = 2;    // Red + IR
  int sampleRate = 50; // Matches 50-sample buffer
  int pulseWidth = 411;
  int adcRange = 4096;

  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);

  // Test LED: flash green briefly to confirm boot
  setStatusColor(0, 255, 0);
  delay(500);
  setStatusColor(0, 0, 0);

  Serial.println(F("SYSTEM_READY"));
}

void loop() {
  if (systemActive) {
    long irValue = particleSensor.getIR();

    if (irValue < 50000) {
      // Blue: waiting for finger placement
      setStatusColor(0, 0, 255);
      Serial.println(F("DATA,0,0,0"));
      delay(100);
    } else {
      // Read MLX90614 BEFORE the MAX30102 sample burst
      // This prevents bus contention between the two sensors
      float temp = mlx.readObjectTempC();

      // Collect MAX30102 samples
      for (byte i = 0; i < bufferLength; i++) {
        // Exit early if button pressed during collection
        if (!systemActive) break;

        while (particleSensor.available() == false) particleSensor.check();
        redBuffer[i] = particleSensor.getRed();
        irBuffer[i] = particleSensor.getIR();
        particleSensor.nextSample();
      }

      if (systemActive) {
        maxim_heart_rate_and_oxygen_saturation(
          irBuffer, bufferLength, redBuffer,
          &spo2, &validSPO2, &heartRate, &validHeartRate
        );

        int finalHR   = (validHeartRate == 1) ? heartRate : 0;
        int finalSPO2 = (validSPO2 == 1)      ? spo2      : 0;

        Serial.print(F("DATA,"));
        Serial.print(temp);
        Serial.print(F(","));
        Serial.print(finalHR);
        Serial.print(F(","));
        Serial.println(finalSPO2);

        // Red alert: fever (>=38C) or low SpO2 (<94%, but only if valid)
        if (temp >= 38.0 || (finalSPO2 < 94 && finalSPO2 > 0)) {
          setStatusColor(255, 0, 0);
        } else {
          setStatusColor(0, 255, 0);
        }
      }
    }
  } else {
    // System inactive: LED off
    setStatusColor(0, 0, 0);
    delay(200);
  }
}