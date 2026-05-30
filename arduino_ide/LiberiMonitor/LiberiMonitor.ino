// ─────────────────────────────────────────────────────────────────────────────
//  Liberi – Vital Sign Monitor   (ESP32 + MLX90614 + MAX30102)
//  Uses Maxim SpO2 algorithm and LittleFS for web page serving
// ─────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <esp_bt.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include "secrets.h"

// ── Wi‑Fi Settings ──────────────────────────────────────────────────────────
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;
const char *ap_ssid = "LiberiMonitor";
const char *ap_pass = "12345678";

// Address of the Node.js server.
const char *local_server_host = LOCAL_SERVER_HOST;
const int local_server_port = 3000;

// ── GPIO pins (ESP32) ────────────────────────────────────────────────────────
const int RED_PIN = 27;
const int GREEN_PIN = 26;
const int BLUE_PIN = 25;
const int BTN_PIN = 17;

// ── Sensor objects ──────────────────────────────────────────────────────────
Adafruit_MLX90614 mlx = Adafruit_MLX90614();
MAX30105 particleSensor;

// ── Vitals buffers & variables ────────────────────────────────────────────────
uint32_t irBuffer[50];
uint32_t redBuffer[50];
int32_t bufferLength = 50;
int32_t spo2;
int8_t validSPO2;
int32_t heartRate;
int8_t validHeartRate;

// ── System states ───────────────────────────────────────────────────────────
volatile bool systemActive = false;
bool lastSystemActive = false;
unsigned long lastDebounceTime = 0;

// ── Web server & SSE endpoint ──────────────────────────────────────────────
AsyncWebServer server(80);
AsyncEventSource events("/events");

// ── Helper function to set RGB LED ──────────────────────────────────────────
void setStatusColor(int r, int g, int b) {
  analogWrite(RED_PIN, r);
  analogWrite(GREEN_PIN, g);
  analogWrite(BLUE_PIN, b);
}

// ── ISR to toggle system status ──────────────────────────────────────────────
void IRAM_ATTR toggleSystem() {
  if (millis() - lastDebounceTime > 300) {
    systemActive = !systemActive;
    lastDebounceTime = millis();
  }
}

// ── Asynchronous background task to send HTTP POST requests on Core 0 ─────────
void httpPostTask(void *parameter) {
  String *payload = (String *)parameter;
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String serverUrl = "http://" + String(local_server_host) + ":" +
                       String(local_server_port) + "/api/data";
    http.begin(serverUrl);
    http.addHeader("Content-Type", "text/plain");
    http.POST(*payload);
    http.end();
  }
  delete payload;      // Free the heap allocated string
  vTaskDelete(NULL);   // Delete this task instance
}

// ────────────────────────────────────────────────────────────────────────────
//  Setup
// ────────────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(1500); // Give Serial Monitor time to connect after reset

  // Disable Bluetooth
  btStop();

  // Wi‑Fi connection with fallback Access Point
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi‑Fi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected! IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nWi‑Fi failed – starting AP");
    WiFi.softAP(ap_ssid, ap_pass);
    Serial.println("AP IP: " + WiFi.softAPIP().toString());
  }

  // I²C
  Wire.begin(21, 22);
  Wire.setClock(100000); // Force 100 kHz for MLX90614 SMBus compatibility

  // GPIO setup
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  pinMode(BTN_PIN, INPUT_PULLUP);
  setStatusColor(0, 0, 0); // Start OFF

  // Attach interrupt for the button
  attachInterrupt(digitalPinToInterrupt(BTN_PIN), toggleSystem, FALLING);

  // Initialize MLX90614
  delay(200);
  if (!mlx.begin()) {
    // Check raw device for simulation or retry
    Wire.beginTransmission(0x5A);
    if (Wire.endTransmission() == 0) {
      Serial.println("MLX90614 library init failed, but device responded at 0x5A (simulation fallback).");
    } else {
      Serial.println("ERR: MLX90614 Fail");
      setStatusColor(255, 0, 0);
    }
  }

  // Initialize MAX30102 using STANDARD speed to avoid overriding Wire clock
  if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD)) {
    Serial.println("ERR: MAX30102 Fail");
    setStatusColor(255, 0, 0);
    while (1); // Halt
  }

  // Re-assert 100 kHz after MAX30102 init
  Wire.setClock(100000);

  // MAX30102 settings
  byte ledBrightness = 60; // 60 = ~12mA current
  byte sampleAverage = 4;
  byte ledMode = 2;    // Red + IR
  int sampleRate = 50; // Matches 50-sample buffer
  int pulseWidth = 411;
  int adcRange = 4096;
  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);

  // LittleFS – holds the web dashboard
  bool fsMounted = LittleFS.begin(true);
  if (!fsMounted) {
    Serial.println("LittleFS Mount Failed!");
  } else {
    Serial.println("LittleFS mounted.");
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
  }

  // Fallback web routing
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (LittleFS.exists("/index.html")) {
      request->send(LittleFS, "/index.html", "text/html");
    } else {
      request->send(404, "text/plain", "index.html not found on LittleFS");
    }
  });
  server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (LittleFS.exists("/index.html")) {
      request->send(LittleFS, "/index.html", "text/html");
    } else {
      request->send(404, "text/plain", "index.html not found on LittleFS");
    }
  });

  events.onConnect([](AsyncEventSourceClient *client) {
    client->send("SYSTEM_BOOTED", NULL, millis());
  });
  server.addHandler(&events);
  server.begin();
  Serial.println("HTTP server started");

  // Flash green briefly to confirm successful setup
  setStatusColor(0, 255, 0);
  delay(500);
  setStatusColor(0, 0, 0);
}

// ────────────────────────────────────────────────────────────────────────────
//  Main loop
// ────────────────────────────────────────────────────────────────────────────
void loop() {
  // Handle state change reports asynchronously
  if (systemActive != lastSystemActive) {
    lastSystemActive = systemActive;
    if (!systemActive) {
      Serial.println("STATUS,OFF");
      events.send("STATUS,OFF");

      if (WiFi.status() == WL_CONNECTED) {
        String *payloadStr = new String("STATUS,OFF");
        xTaskCreatePinnedToCore(httpPostTask, "HTTPPostTask", 4096, payloadStr, 1, NULL, 0);
      }
      setStatusColor(0, 0, 0);
    } else {
      Serial.println("STATUS,ON");
      events.send("STATUS,ON");

      if (WiFi.status() == WL_CONNECTED) {
        String *payloadStr = new String("STATUS,ON");
        xTaskCreatePinnedToCore(httpPostTask, "HTTPPostTask", 4096, payloadStr, 1, NULL, 0);
      }
      setStatusColor(0, 0, 255); // Waiting for finger
    }
  }

  if (systemActive) {
    long irValue = particleSensor.getIR();

    if (irValue < 50000) {
      // Blue: waiting for finger placement
      setStatusColor(0, 0, 255);
      Serial.println("DATA,0.0,0,0");
      
      // Update browser and Node.js server
      events.send("DATA,0.0,0,0");
      if (WiFi.status() == WL_CONNECTED) {
        String *payloadStr = new String("DATA,0.0,0,0");
        xTaskCreatePinnedToCore(httpPostTask, "HTTPPostTask", 4096, payloadStr, 1, NULL, 0);
      }
      delay(200);
    } else {
      // Read MLX90614 before collection burst to prevent bus contention
      float temp = mlx.readObjectTempC();
      
      // Fallback I2C read if library fails/returns NaN (simulation support)
      if (isnan(temp)) {
        Wire.beginTransmission(0x5A);
        if (Wire.endTransmission() == 0) {
          Wire.requestFrom(0x5A, 1);
          if (Wire.available()) {
            temp = Wire.read();
          }
        }
      }

      // Collect MAX30102 samples
      for (byte i = 0; i < bufferLength; i++) {
        if (!systemActive) break;

        while (particleSensor.available() == false) {
          if (!systemActive) break;
          particleSensor.check();
        }
        if (!systemActive) break;
        
        redBuffer[i] = particleSensor.getRed();
        irBuffer[i] = particleSensor.getIR();
        particleSensor.nextSample();
      }

      if (systemActive) {
        // Run Maxim SpO2 algorithm
        maxim_heart_rate_and_oxygen_saturation(
          irBuffer, bufferLength, redBuffer,
          &spo2, &validSPO2, &heartRate, &validHeartRate
        );

        int finalHR   = (validHeartRate == 1) ? heartRate : 0;
        int finalSPO2 = (validSPO2 == 1)      ? spo2      : 0;

        char sseMsg[128];
        snprintf(sseMsg, sizeof(sseMsg), "DATA,%.1f,%d,%d", temp, finalHR, finalSPO2);
        Serial.println(sseMsg);

        // Send to SSE and local server
        events.send(sseMsg, NULL, millis());
        if (WiFi.status() == WL_CONNECTED) {
          String *payloadStr = new String(sseMsg);
          xTaskCreatePinnedToCore(httpPostTask, "HTTPPostTask", 4096, payloadStr, 1, NULL, 0);
        }

        // Color coding: Red alert if fever or low oxygen, Green if normal
        if (temp >= 38.0 || (finalSPO2 < 94 && finalSPO2 > 0)) {
          setStatusColor(255, 0, 0);
        } else {
          setStatusColor(0, 255, 0);
        }
      }
    }
  } else {
    // Inactive: LED Off
    setStatusColor(0, 0, 0);
    delay(200);
  }
}