#include <Arduino.h>
#include <Wire.h>

// ── Pin definitions (ESP32) ──────────────────────────────────────────────────
const int RED_PIN   = 27;
const int GREEN_PIN = 26;
const int BLUE_PIN  = 25;
const int BTN_PIN   = 17;

// ── State machine ────────────────────────────────────────────────────────────
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
  "ERROR",
  "IDLE",
  "SAMPLING",
  "CRITICAL_ILLNESS",
  "SEVERE_HYPOTHERMIA",
  "EARLY_ILLNESS",
  "MILD_HYPOTHERMIA",
  "STRESS",
  "CIRCULATION",
  "HEALTHY"
};

// ── LED transition target/current ────────────────────────────────────────────
struct RGB { float r, g, b; };

RGB targetColor  = {0, 0, 0};
RGB currentColor = {0, 0, 0};

// Speed of smooth LED blending (0–1 per tick; higher = faster)
const float BLEND_SPEED = 0.06f;

// Blink / strobe state
bool          blinkState = false;
unsigned long lastBlink  = 0;

// ── Sampling config ───────────────────────────────────────────────────────────
// Raised from 3 → 5 for a more stable 7.5s baseline before classifying
const int STARTUP_SAMPLES_REQUIRED = 5;

// Force a fresh classification every N readings (~15s at 1500ms polling)
const int RECLASSIFY_INTERVAL = 10;

// ── Hysteresis: only commit to a new state after N consecutive confirmations ──
const int HYSTERESIS_THRESHOLD = 2;
SystemState candidateState = STATE_IDLE;
int         candidateCount = 0;

// ── System state ──────────────────────────────────────────────────────────────
bool          systemActive  = false;
bool          i2cError      = false;
int           startupCount  = 0;
unsigned long lastUpdate    = 0;
unsigned long lastDebounce  = 0;

// ── Latest readings ───────────────────────────────────────────────────────────
float tempC   = 0.0f;
int   hrBPM   = 0;
int   spo2Pct = 0;
float pi      = 0.0f;

// HRV: simplified — abs difference between consecutive HR reads
int prevHR = 0;
int hrv    = 999; // large = unknown

SystemState currentState = STATE_IDLE;

// ────────────────────────────────────────────────────────────────────────────
// Helpers
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

// ────────────────────────────────────────────────────────────────────────────
// State → colour mapping
// ────────────────────────────────────────────────────────────────────────────
void applyStateColor(SystemState s) {
  unsigned long now = millis();
  switch (s) {
    case STATE_ERROR:
      if (now - lastBlink > 250) { blinkState = !blinkState; lastBlink = now; }
      if (blinkState) setTarget(255, 0, 0); else setTarget(0, 0, 0);
      break;

    case STATE_IDLE:
      setTarget(255, 240, 220);
      break;

    case STATE_SAMPLING:
      if (now - lastBlink > 500) { blinkState = !blinkState; lastBlink = now; }
      if (blinkState) setTarget(30, 80, 200); else setTarget(5, 20, 60);
      break;

    case STATE_CRITICAL_ILLNESS:
      setTarget(180, 0, 0);
      break;

    case STATE_SEVERE_HYPOTHERMIA:
      setTarget(120, 0, 20);
      break;

    case STATE_EARLY_ILLNESS:
      setTarget(255, 80, 0);
      break;

    case STATE_MILD_HYPOTHERMIA:
      setTarget(200, 160, 0);
      break;

    case STATE_STRESS:
      setTarget(100, 0, 180);
      break;

    case STATE_CIRCULATION:
      setTarget(0, 220, 255);
      break;

    case STATE_HEALTHY:
      setTarget(0, 200, 60);
      break;
  }
}

// ────────────────────────────────────────────────────────────────────────────
// State classification (strict priority, top → bottom = high → low)
// ────────────────────────────────────────────────────────────────────────────
SystemState classifyState(float t, int hr, int hrv_val, int spo2, float pi_val, bool err, bool sampling) {
  if (err)                                    return STATE_ERROR;
  if (!systemActive || (hr == 0 && spo2 == 0)) return STATE_IDLE;
  if (sampling)                               return STATE_SAMPLING;

  if (t >= 39.5f && spo2 < 90 && spo2 > 0)                    return STATE_CRITICAL_ILLNESS;
  if (t <= 32.0f && hr < 60 && pi_val < 1.0f)                  return STATE_SEVERE_HYPOTHERMIA;
  if (t >= 37.5f && t <= 39.4f && hr > 100 && hrv_val < 40)   return STATE_EARLY_ILLNESS;
  if (t >= 32.1f && t <= 34.9f && hr > 90)                     return STATE_MILD_HYPOTHERMIA;
  if (t >= 35.0f && t <= 37.4f && hr > 100 && hrv_val < 40)   return STATE_STRESS;
  if (pi_val < 2.0f && pi_val > 0.0f)                          return STATE_CIRCULATION;
  if (t >= 35.0f && t <= 37.4f && hr >= 60 && hr <= 95 &&
      hrv_val > 50 && (pi_val >= 4.0f || spo2 >= 95))          return STATE_HEALTHY;

  return STATE_SAMPLING;
}

// ────────────────────────────────────────────────────────────────────────────
// Hysteresis guard — only commit to a new state after HYSTERESIS_THRESHOLD
// consecutive readings agree, preventing flicker between adjacent states
// ────────────────────────────────────────────────────────────────────────────
SystemState applyHysteresis(SystemState newState) {
  if (newState == candidateState) {
    candidateCount++;
    if (candidateCount >= HYSTERESIS_THRESHOLD) {
      return newState; // confirmed
    }
    return currentState; // not yet confirmed, hold current
  } else {
    candidateState = newState;
    candidateCount = 1;
    return currentState; // reset, hold current until confirmed
  }
}

// ────────────────────────────────────────────────────────────────────────────
// I2C reads (Wokwi chip protocol)
// ────────────────────────────────────────────────────────────────────────────
void readSensors() {
  // --- MLX90614 (0x5A) ---
  Wire.requestFrom(0x5A, 1);
  if (Wire.available()) {
    tempC    = (float)Wire.read();
    i2cError = false;
  } else {
    i2cError = true;
    return;
  }

  // --- MAX30102 (0x57) — HR read ---
  Wire.requestFrom(0x57, 1);
  if (Wire.available()) {
    int newHR = Wire.read();
    hrv       = abs(newHR - prevHR); // simplified HRV proxy
    prevHR    = hrBPM;
    hrBPM     = newHR;
  } else {
    i2cError = true;
    return;
  }

  delay(20);

  // --- MAX30102 (0x57) — SpO2 read ---
  Wire.requestFrom(0x57, 1);
  if (Wire.available()) {
    spo2Pct = Wire.read();
  } else {
    i2cError = true;
    return;
  }

  // Perfusion Index: simulated as spo2/25 (0–4 range for wokwi)
  pi = (spo2Pct > 0) ? (spo2Pct / 25.0f) : 0.0f;

  startupCount++;
}

// ────────────────────────────────────────────────────────────────────────────
// Setup & Loop
// ────────────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  Wire.setClock(100000);

  pinMode(RED_PIN,   OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN,  OUTPUT);
  pinMode(BTN_PIN,   INPUT_PULLUP);

  setTarget(0, 0, 0);
  Serial.println("SYSTEM_BOOTED");
}

void loop() {
  // ── Button debounce ───────────────────────────────────────────────────────
  if (digitalRead(BTN_PIN) == LOW && millis() - lastDebounce > 300) {
    lastDebounce = millis();
    systemActive = !systemActive;

    // Reset all tracking on every toggle
    startupCount   = 0;
    hrv            = 999;
    prevHR         = 0;
    candidateState = STATE_IDLE;
    candidateCount = 0;

    if (!systemActive) {
      hrBPM = 0; spo2Pct = 0; tempC = 0; pi = 0;
      currentState = STATE_IDLE;
      Serial.println("STATUS,OFF");
    } else {
      Serial.println("STATUS,ON");
    }
  }

  // ── Sensor polling ────────────────────────────────────────────────────────
  if (systemActive && millis() - lastUpdate > 1500) {
    lastUpdate = millis();
    readSensors();

    bool sampling = (startupCount < STARTUP_SAMPLES_REQUIRED);

    // Classify raw state
    SystemState rawState = classifyState(tempC, hrBPM, hrv, spo2Pct, pi, i2cError, sampling);

    // Force reclassification every RECLASSIFY_INTERVAL readings for slow drifts
    if (!sampling && startupCount % RECLASSIFY_INTERVAL == 0) {
      rawState = classifyState(tempC, hrBPM, hrv, spo2Pct, pi, i2cError, false);
    }

    // Apply hysteresis before committing to a new state
    currentState = applyHysteresis(rawState);

    // Emit data line for the dashboard
    Serial.print("DATA,");
    Serial.print(tempC, 1);
    Serial.print(",");
    Serial.print(hrBPM);
    Serial.print(",");
    Serial.print(spo2Pct);
    Serial.print(",");
    Serial.print(pi, 2);
    Serial.print(",");
    Serial.print(hrv);
    Serial.print(",");
    Serial.println(stateNames[currentState]);
  }

  // ── LED smooth blend ──────────────────────────────────────────────────────
  applyStateColor(currentState);
  blendLED();
  delay(16); // ~60 fps blend ticks
}