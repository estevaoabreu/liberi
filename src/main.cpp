#include <Arduino.h>
#include <Wire.h>

#ifdef ESP32
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#endif

// Pin Definitions
#ifdef ESP32
const int RED_PIN = 27;
const int GREEN_PIN = 26;
const int BLUE_PIN = 25;
const int BTN_PIN = 17;
#else
const int RED_PIN = 9;
const int GREEN_PIN = 10;
const int BLUE_PIN = 11;
const int BTN_PIN = 2;
#endif

// System Variables
bool systemActive = false;
unsigned long lastUpdate = 0;

#include "secrets.h"

// Wi-Fi settings for ESP32 as seen in the setup guide md
#ifdef ESP32
const char* ssid     = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// Web Server and Event Source
AsyncWebServer server(80);
AsyncEventSource events("/events");

#include "index_html.h"
#endif

// Helper function to set RGB Color
void setStatusColor(int r, int g, int b) {
  analogWrite(RED_PIN, r);
  analogWrite(GREEN_PIN, g);
  analogWrite(BLUE_PIN, b);
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

  setStatusColor(0, 0, 0); // Start OFF
  Serial.println("SYSTEM_BOOTED");

#ifdef ESP32
  // Connect to Wi-Fi with fallback to AP mode
  Serial.print("Connecting to Wi-Fi...");
  WiFi.begin(ssid, password);
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 15) { // 7.5 seconds timeout
    delay(500);
    Serial.print(".");
    retry++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.print("Connected! IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("");
    Serial.println("Wi-Fi connection failed. Starting fallback Access Point...");
    WiFi.softAP("LiberiMonitor", "12345678");
    Serial.print("AP Mode started. Connect to SSID 'LiberiMonitor' with password '12345678'. IP address: ");
    Serial.println(WiFi.softAPIP());
  }

  // Initialize LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed");
  } else {
    Serial.println("LittleFS Mounted Successfully");
  }

  // Serve static assets from LittleFS if it mounted successfully
  server.serveStatic("/", LittleFS, "/");

  // Fallback / default direct serving of web dashboard from memory
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", index_html);
  });
  server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", index_html);
  });

  // Add Event Source endpoint
  server.addHandler(&events);

  // Start the server
  server.begin();
  Serial.println("HTTP Web Server started on port 80");
#endif
}

void loop() {
  // 1. Check Button for Power Toggle
  if (digitalRead(BTN_PIN) == LOW) {
    systemActive = !systemActive;

    if (!systemActive) {
      setStatusColor(0, 0, 0); // Turn off LED
      Serial.println("STATUS,OFF");
#ifdef ESP32
      if (WiFi.status() == WL_CONNECTED) {
        Serial.print("IP: ");
        Serial.println(WiFi.localIP().toString());
      } else {
        Serial.print("AP IP: ");
        Serial.println(WiFi.softAPIP().toString());
      }
      events.send("STATUS,OFF", "wokwi");
#endif
    } else {
      Serial.println("STATUS,ON");
#ifdef ESP32
      if (WiFi.status() == WL_CONNECTED) {
        Serial.print("IP: ");
        Serial.println(WiFi.localIP().toString());
      } else {
        Serial.print("AP IP: ");
        Serial.println(WiFi.softAPIP().toString());
      }
      events.send("STATUS,ON", "wokwi");
#endif
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

      // --- SEND DATA TO NODE.JS APP & SSE CLIENTS ---
      // Format: DATA,temp,vital1,vital2
      String payload = "DATA," + String(temp) + "," + String(vital1) + "," + String(vital2);
      Serial.println(payload);
#ifdef ESP32
      events.send(payload.c_str(), "wokwi");
#endif

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