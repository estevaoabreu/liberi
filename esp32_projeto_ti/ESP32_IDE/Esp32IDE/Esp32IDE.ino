// ─────────────────────────────────────────────────────────────────────────────
//  Liberi – Vital Sign Monitor   (ESP32 + MLX90614 + MAX30102)
//  Direct SSE dashboard via LittleFS  |  Real PI, RMSSD HRV, ratio‑of‑ratios SpO₂
// ─────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <esp_bt.h>

// ── Wi‑Fi ───────────────────────────────────────────────────────────────────
const char* ssid     = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* ap_ssid  = "LiberiMonitor";
const char* ap_pass  = "12345678";

// ── GPIO pins ──────────────────────────────────────────────────────────────
const int RED_PIN  = 27;
const int GREEN_PIN= 26;
const int BLUE_PIN = 25;
const int BTN_PIN  = 17;

// ── Sensor objects ──────────────────────────────────────────────────────────
Adafruit_MLX90614 mlx = Adafruit_MLX90614();
MAX30105 particleSensor;

// ── System states ───────────────────────────────────────────────────────────
enum SystemState {
  STATE_ERROR,
  STATE_IDLE,
  STATE_SAMPLING,
  STATE_CRITICAL_ILLNESS,
  STATE_SEVERE_HYPOTHERMIA,
  STATE_EARLY_ILLNESS,
  STATE_MILD_HYPOTHERMIA,
  STATE_STRESS,
  STATE_CIRCULATION,
  STATE_HEALTHY
};

const char* stateNames[] = {
  "ERROR", "IDLE", "SAMPLING", "CRITICAL_ILLNESS",
  "SEVERE_HYPOTHERMIA", "EARLY_ILLNESS", "MILD_HYPOTHERMIA",
  "STRESS", "CIRCULATION", "HEALTHY"
};

// ── Mutex‑protected shared data (for future multi‑core use) ─────────────────
SemaphoreHandle_t dataMutex;
struct SharedData {
  float tempC;
  int   hrBPM;
  int   spo2Pct;
  float pi;
  int   hrv;
  SystemState state;
};
SharedData latestData;

// ── RGB LED blending ────────────────────────────────────────────────────────
struct RGB { float r, g, b; };
RGB targetColor  = {0, 0, 0};
RGB currentColor = {0, 0, 0};
const float BLEND_SPEED = 0.06f;
bool blinkState = false;
unsigned long lastBlink = 0;

// ── State machine parameters ────────────────────────────────────────────────
const int STARTUP_SAMPLES_REQUIRED = 5;      // readings before valid classification
const int RECLASSIFY_INTERVAL      = 10;     // re‑evaluate every N readings
const int HYSTERESIS_THRESHOLD     = 2;      // consecutive same‑state counts
SystemState candidateState = STATE_IDLE;
int candidateCount = 0;

// ── System flags & timers ───────────────────────────────────────────────────
bool systemActive = false;
bool i2cError     = false;
int  startupCount = 0;
unsigned long lastUpdate   = 0;
unsigned long lastDebounce = 0;

// ── Latest vital signs ──────────────────────────────────────────────────────
float tempC   = 0.0f;
int   hrBPM   = 0;
int   spo2Pct = 0;
float pi      = 0.0f;

// ── Heart rate & HRV (RMSSD) ────────────────────────────────────────────────
int hrv       = 999;        // will hold RMSSD [ms], 999 = not enough data
long lastBeat = 0;
const byte RATE_SIZE = 4;   // HR smoothing average
byte rates[RATE_SIZE];
byte rateSpot = 0;

// RR‑interval buffer for RMSSD computation
const int RR_WINDOW = 10;
long rrIntervals[RR_WINDOW];
int rrIndex = 0;
bool rrBufferFull = false;

// ── PI & SpO₂: min/max tracking for IR and Red over 1.5 s window ───────────
long irMin  = 100000L, irMax  = 0L;
long redMin = 100000L, redMax = 0L;

SystemState currentState = STATE_IDLE;

// ── Web server & SSE endpoint ──────────────────────────────────────────────
AsyncWebServer server(80);
AsyncEventSource events("/events");

// ── SpO₂ calibration constants (can be tuned against reference oximeter) ───
//  Defaults from Mendelson‑Ochs linear fit. Adjust A (intercept) and B (slope)
//  after a two‑point calibration: measure R at ~99% and ~95% SpO₂.
const float SPO2_A = 104.0f;
const float SPO2_B =  17.0f;

// ────────────────────────────────────────────────────────────────────────────
//  Helper functions
// ────────────────────────────────────────────────────────────────────────────

void setTarget(int r, int g, int b) {
  targetColor = { (float)r, (float)g, (float)b };
}

void blendLED() {
  currentColor.r += (targetColor.r - currentColor.r) * BLEND_SPEED;
  currentColor.g += (targetColor.g - currentColor.g) * BLEND_SPEED;
  currentColor.b += (targetColor.b - currentColor.b) * BLEND_SPEED;
  analogWrite(RED_PIN,   (int)currentColor.r);
  analogWrite(GREEN_PIN, (int)currentColor.g);
  analogWrite(BLUE_PIN,  (int)currentColor.b);
}

void applyStateColor(SystemState s) {
  unsigned long now = millis();
  switch (s) {
    case STATE_ERROR:
      if (now - lastBlink > 250) { blinkState = !blinkState; lastBlink = now; }
      if (blinkState) setTarget(255, 0, 0); else setTarget(0, 0, 0);
      break;
    case STATE_IDLE:               setTarget(255, 240, 220); break;
    case STATE_SAMPLING:
      if (now - lastBlink > 500) { blinkState = !blinkState; lastBlink = now; }
      if (blinkState) setTarget(30, 80, 200); else setTarget(5, 20, 60);
      break;
    case STATE_CRITICAL_ILLNESS:   setTarget(180, 0, 0);    break;
    case STATE_SEVERE_HYPOTHERMIA: setTarget(120, 0, 20);   break;
    case STATE_EARLY_ILLNESS:      setTarget(255, 80, 0);   break;
    case STATE_MILD_HYPOTHERMIA:   setTarget(200, 160, 0);  break;
    case STATE_STRESS:             setTarget(100, 0, 180);  break;
    case STATE_CIRCULATION:        setTarget(0, 220, 255);  break;
    case STATE_HEALTHY:            setTarget(0, 200, 60);   break;
  }
}

// ── Compute SpO₂ using ratio‑of‑ratios (both LED channels) ──────────────────
int computeSpO2(long irMn, long irMx, long redMn, long redMx) {
  if (irMx <= irMn || redMx <= redMn) return 0;   // no valid swing

  float acIr  = (irMx  - irMn)  / 2.0f;
  float dcIr  = (irMx  + irMn)  / 2.0f;
  float acRed = (redMx - redMn) / 2.0f;
  float dcRed = (redMx + redMn) / 2.0f;

  if (dcIr < 1.0f || dcRed < 1.0f) return 0;      // avoid division by zero

  float R = (acRed / dcRed) / (acIr / dcIr);
  float spo2 = SPO2_A - SPO2_B * R;

  return constrain((int)spo2, 0, 100);
}

// ── State classification logic ──────────────────────────────────────────────
SystemState classifyState(float t, int hr, int hrv_val, int spo2, float pi_val, bool err, bool sampling) {
  if (err)                                                return STATE_ERROR;
  if (!systemActive || (hr == 0 && spo2 == 0))            return STATE_IDLE;
  if (sampling)                                           return STATE_SAMPLING;

  // Thresholds reflect clinically relevant patterns (neonatal/infant focus)
  if (t >= 39.5f && spo2 < 90 && spo2 > 0)                return STATE_CRITICAL_ILLNESS;
  if (t <= 32.0f && hr < 60 && pi_val < 0.5f)             return STATE_SEVERE_HYPOTHERMIA;
  if (t >= 37.5f && t <= 39.4f && hr > 100 && hrv_val < 20) return STATE_EARLY_ILLNESS;
  if (t >= 32.1f && t <= 34.9f && hr > 90)                return STATE_MILD_HYPOTHERMIA;
  if (t >= 35.0f && t <= 37.4f && hr > 100 && hrv_val < 20) return STATE_STRESS;
  if (pi_val < 1.0f && pi_val > 0.0f)                     return STATE_CIRCULATION;
  if (t >= 35.0f && t <= 37.4f && hr >= 60 && hr <= 95 &&
      hrv_val > 40 && (pi_val >= 2.0f || spo2 >= 95))     return STATE_HEALTHY;

  // Fallback: if vitals are present but no rule matches, flag circulation concern
  if (hr > 0 && spo2 > 0) return STATE_CIRCULATION;

  return STATE_SAMPLING;
}

SystemState applyHysteresis(SystemState newState) {
  if (newState == candidateState) {
    candidateCount++;
    if (candidateCount >= HYSTERESIS_THRESHOLD) return newState;
    return currentState;
  } else {
    candidateState = newState;
    candidateCount = 1;
    return currentState;
  }
}

// ────────────────────────────────────────────────────────────────────────────
//  Setup
// ────────────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  // Disable Bluetooth (safe, no deinit needed)
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
  Wire.setClock(100000);

  // GPIO
  pinMode(RED_PIN,   OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN,  OUTPUT);
  pinMode(BTN_PIN,   INPUT_PULLUP);
  setTarget(0, 0, 0);

  // MLX90614 (object temperature)
  if (!mlx.begin()) {
    Serial.println("MLX90614 Error! Check wiring.");
    i2cError = true;
  }

  // MAX30102 – explicit configuration for SpO₂ (Red + IR)
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102 Error! Check wiring.");
    i2cError = true;
  } else {
    byte powerLevel    = 0x1F;   // ~6.4 mA
    byte sampleAverage = 4;
    byte ledMode       = 2;      // 1=Red only, 2=Red+IR
    int  sampleRate    = 100;    // Hz
    int  pulseWidth    = 411;    // µs
    int  adcRange      = 4096;
    particleSensor.setup(powerLevel, sampleAverage, ledMode,
                         sampleRate, pulseWidth, adcRange);
  }

  // Mutex for shared data
  dataMutex = xSemaphoreCreateMutex();

  // LittleFS – holds the web dashboard (index.html, css.css, javascript.js)
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed – did you upload the data folder?");
    return;
  }
  Serial.println("LittleFS mounted.");

  // Serve static files, SSE endpoint
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
  events.onConnect([](AsyncEventSourceClient *client){
    client->send("SYSTEM_BOOTED", NULL, millis());
  });
  server.addHandler(&events);
  server.begin();
  Serial.println("HTTP server started");
}

// ────────────────────────────────────────────────────────────────────────────
//  Main loop (Core 1)
// ────────────────────────────────────────────────────────────────────────────
void loop() {
  // ── Button toggle (on/off) ──────────────────────────────────────────────
  if (digitalRead(BTN_PIN) == LOW && millis() - lastDebounce > 300) {
    lastDebounce = millis();
    systemActive = !systemActive;

    // Reset all trackers on state change
    startupCount   = 0;
    hrv            = 999;
    candidateState = STATE_IDLE;
    candidateCount = 0;
    for (byte i = 0; i < RATE_SIZE; i++) rates[i] = 0;
    rrBufferFull = false;
    rrIndex = 0;
    irMin  = 100000L; irMax  = 0L;
    redMin = 100000L; redMax = 0L;

    if (!systemActive) {
      hrBPM = 0; spo2Pct = 0; tempC = 0; pi = 0;
      currentState = STATE_IDLE;
      Serial.println("STATUS,OFF");
      events.send("STATUS,OFF");
      particleSensor.shutDown();
    } else {
      Serial.println("STATUS,ON");
      events.send("STATUS,ON");
      particleSensor.wakeUp();
    }
  }

  // ── Continuous PPG reading (every loop iteration) ────────────────────────
  if (systemActive) {
    long irValue  = particleSensor.getIR();
    long redValue = particleSensor.getRed();

    // Track min/max for both channels over the 1.5 s window
    if (irValue  < irMin)  irMin  = irValue;
    if (irValue  > irMax)  irMax  = irValue;
    if (redValue < redMin) redMin = redValue;
    if (redValue > redMax) redMax = redValue;

    // Finger detection threshold
    if (irValue < 50000) {
      hrBPM   = 0;
      spo2Pct = 0;
    }
    else if (checkForBeat(irValue)) {
      long delta = millis() - lastBeat;
      lastBeat = millis();

      float beatsPerMinute = 60.0f / (delta / 1000.0f);
      if (beatsPerMinute < 255 && beatsPerMinute > 20) {
        // Rolling average for HR display
        rates[rateSpot++] = (byte)beatsPerMinute;
        rateSpot %= RATE_SIZE;
        int beatAvg = 0;
        for (byte x = 0; x < RATE_SIZE; x++) beatAvg += rates[x];
        hrBPM = beatAvg / RATE_SIZE;

        // RR‑interval buffer for RMSSD
        rrIntervals[rrIndex++] = delta;
        if (rrIndex >= RR_WINDOW) {
          rrIndex = 0;
          rrBufferFull = true;
        }
        if (rrBufferFull) {
          float sumSq = 0;
          for (int i = 1; i < RR_WINDOW; i++) {
            long diff = rrIntervals[i] - rrIntervals[i-1];
            sumSq += diff * diff;
          }
          hrv = (int)sqrt(sumSq / (RR_WINDOW - 1));
        } else {
          hrv = 999;   // not enough data
        }
      }
    }
  }

  // ── 1.5 s update: temperature, PI, SpO₂, classification, SSE ────────────
  if (systemActive && millis() - lastUpdate > 1500) {
    lastUpdate = millis();

    // Body temperature (MLX90614 object reading)
    tempC    = mlx.readObjectTempC();
    i2cError = isnan(tempC);

    // Perfusion Index from IR min/max
    if (irMax > irMin) {
      float acIr = (irMax - irMin) / 2.0f;
      float dcIr = (irMax + irMin) / 2.0f;
      pi = (acIr / dcIr) * 100.0f;
    } else {
      pi = 0.0f;
    }

    // SpO₂ via ratio‑of‑ratios (uses both IR and Red)
    spo2Pct = computeSpO2(irMin, irMax, redMin, redMax);

    // Reset window for next cycle
    irMin  = 100000L; irMax  = 0L;
    redMin = 100000L; redMax = 0L;

    // State classification with hysteresis
    startupCount++;
    bool sampling = (startupCount < STARTUP_SAMPLES_REQUIRED);
    SystemState rawState = classifyState(tempC, hrBPM, hrv, spo2Pct, pi, i2cError, sampling);
    if (!sampling && startupCount % RECLASSIFY_INTERVAL == 0) {
      rawState = classifyState(tempC, hrBPM, hrv, spo2Pct, pi, i2cError, false);
    }
    currentState = applyHysteresis(rawState);

    // Store in mutex‑protected shared struct (future expansion)
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      latestData.tempC   = tempC;
      latestData.hrBPM   = hrBPM;
      latestData.spo2Pct = spo2Pct;
      latestData.pi      = pi;
      latestData.hrv     = hrv;
      latestData.state   = currentState;
      xSemaphoreGive(dataMutex);
    }

    // Push update to all connected browsers via SSE
    char sseMsg[128];
    snprintf(sseMsg, sizeof(sseMsg),
             "DATA,%.1f,%d,%d,%.2f,%d,%s",
             tempC, hrBPM, spo2Pct, pi, hrv, stateNames[currentState]);
    events.send(sseMsg, NULL, millis());

    // Serial debug log
    Serial.print("DATA,");
    Serial.print(tempC, 1);    Serial.print(",");
    Serial.print(hrBPM);       Serial.print(",");
    Serial.print(spo2Pct);     Serial.print(",");
    Serial.print(pi, 2);       Serial.print(",");
    Serial.print(hrv);         Serial.print(",");
    Serial.println(stateNames[currentState]);
  }

  // ── Smooth LED colour blending (always runs) ─────────────────────────────
  applyStateColor(currentState);
  blendLED();
  delay(16);   // ~60 fps
}