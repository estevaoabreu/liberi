// ─────────────────────────────────────────────────────────────────────────────
//   Liberi – Vital Sign Monitor   (Streaming Contínuo)
// ─────────────────────────────────────────────────────────────────────────────

#include "MAX30105.h"
#include "secrets.h"
#include "spo2_algorithm.h"
#include <Adafruit_MLX90614.h>
#include <Arduino.h>
#include <Wire.h>
#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

// ── GPIO pins (ESP32) ────────────────────────────────────────────────────────
const int RED_PIN = 27;
const int GREEN_PIN = 26;
const int BLUE_PIN = 25;
const int BTN_PIN = 17;

// ── Sensor objects ──────────────────────────────────────────────────────────
Adafruit_MLX90614 mlx = Adafruit_MLX90614();
MAX30105 particleSensor;

// ── Vitals buffers & variables ──────────────────────────────────────────────
#define BUFFER_SIZE 50
uint32_t irBuffer[BUFFER_SIZE];
uint32_t redBuffer[BUFFER_SIZE];
int32_t spo2;
int8_t validSPO2;
int32_t heartRate;
int8_t validHeartRate;

// ── System states ───────────────────────────────────────────────────────────
volatile bool systemActive = false;
bool lastSystemActive = false;
unsigned long lastDebounceTime = 0;
bool bufferPrimeiroEnchimento = true; // Controla o primeiro setup de dados



void setStatusColor(int r, int g, int b) {
  analogWrite(RED_PIN, r);
  analogWrite(GREEN_PIN, g);
  analogWrite(BLUE_PIN, b);
}

void IRAM_ATTR toggleSystem() {
  if (millis() - lastDebounceTime > 300) {
    systemActive = !systemActive;
    lastDebounceTime = millis();
  }
}



// ────────────────────────────────────────────────────────────────────────────
//  Setup
// ────────────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(1500); 

  SerialBT.begin("Liberi_Monitor");

  Wire.begin(21, 22);
  Wire.setClock(100000); 

  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  pinMode(BTN_PIN, INPUT_PULLUP);
  setStatusColor(0, 0, 0);

  attachInterrupt(digitalPinToInterrupt(BTN_PIN), toggleSystem, FALLING);

  delay(200);
  mlx.begin();

  particleSensor.begin(Wire, I2C_SPEED_STANDARD);
  Wire.setClock(100000); 

  // Configuração focada em rapidez de resposta
  byte ledBrightness = 60; 
  byte sampleAverage = 1;  // Reduzido para 1 para feedback instantâneo amostra a amostra
  byte ledMode = 2;    
  int sampleRate = 50; 
  int pulseWidth = 411;
  int adcRange = 4096;
  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);


  
  // Flash de sucesso
  setStatusColor(0, 255, 0);
  delay(500);
  setStatusColor(0, 0, 0);
}

// ────────────────────────────────────────────────────────────────────────────
//  Main loop
// ────────────────────────────────────────────────────────────────────────────
void loop() {
  // Trata botão ON/OFF
  if (systemActive != lastSystemActive) {
    lastSystemActive = systemActive;
    if (!systemActive) {
      Serial.println("STATUS,OFF");
      SerialBT.println("STATUS,OFF");
      setStatusColor(0, 0, 0);
      bufferPrimeiroEnchimento = true; // Reseta o buffer ao desligar
    } else {
      Serial.println("STATUS,ON");
      SerialBT.println("STATUS,ON");
      setStatusColor(0, 0, 255); // Azul enquanto calibra
    }
  }

  if (systemActive) {
    // 1. Se for a primeira execução após ligar, enche o buffer inicial de 50 amostras
    if (bufferPrimeiroEnchimento) {
      for (byte i = 0; i < BUFFER_SIZE; i++) {
        while (particleSensor.available() == false) { particleSensor.check(); }
        redBuffer[i] = particleSensor.getRed();
        irBuffer[i] = particleSensor.getIR();
        particleSensor.nextSample();
      }
      bufferPrimeiroEnchimento = false;
    }

    // 2. Descala os buffers (faz o shift dos dados antigos para trás)
    // Descartamos a amostra mais antiga [0] e abrimos espaço em [49]
    for (byte i = 1; i < BUFFER_SIZE; i++) {
      redBuffer[i - 1] = redBuffer[i];
      irBuffer[i - 1] = irBuffer[i];
    }

    // 3. Lê a amostra mais recente para a última posição do buffer
    while (particleSensor.available() == false) { particleSensor.check(); }
    redBuffer[BUFFER_SIZE - 1] = particleSensor.getRed();
    irBuffer[BUFFER_SIZE - 1] = particleSensor.getIR();
    particleSensor.nextSample();

    // 4. Lê a temperatura (MLX90614)
    float temp = mlx.readObjectTempC();
    if (isnan(temp)) temp = 0.0; // Fallback simples para evitar quebras de parsing no JS

    // 5. Corre o algoritmo com os dados atualizados (Input Constante)
    maxim_heart_rate_and_oxygen_saturation(irBuffer, BUFFER_SIZE, redBuffer,
                                           &spo2, &validSPO2, &heartRate, &validHeartRate);

    int finalHR = heartRate;
    int finalSPO2 = spo2;

    // 6. Envia a string imediatamente para o Serial (Sem delays longos)
    char sseMsg[128];
    snprintf(sseMsg, sizeof(sseMsg), "DATA,%.1f,%d,%d", temp, finalHR, finalSPO2);
    Serial.println(sseMsg);
    SerialBT.println(sseMsg);



    // 7. Atualização dinâmica dos LEDs baseada nos valores lidos
    if (finalSPO2 >= 95 && finalHR >= 50 && finalHR <= 130) {
      setStatusColor(0, 255, 0);  // Verde: Valores bons
    } else {
      setStatusColor(255, 0, 0);  // Vermelho: Valores maus/críticos
    }

    // Nota: Retirámos o delay(1500) do loop ativo. 
    // Agora a velocidade do input é ditada puramente pela taxa de amostragem do sensor.
  } else {
    setStatusColor(0, 0, 0);
    delay(100);
  }
}