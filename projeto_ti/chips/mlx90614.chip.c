#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

typedef struct {
  pin_t pin_sda;
  pin_t pin_scl;
} chip_state_t;

static uint8_t on_i2c_read(void *user_data) {
  // Simulates a temperature around 37°C
  // Returns a simple byte for the demo
  return 37 + (rand() % 2); 
}

bool on_i2c_connect(void *user_data, uint32_t address, bool connect) {
  return address == 0x5A; // Default MLX address
}

void chip_init() {
  chip_state_t *chip = malloc(sizeof(chip_state_t));

  const i2c_config_t i2c_config = {
    .user_data = chip,
    .address = 0x5A,
    .scl = pin_init("SCL", INPUT),
    .sda = pin_init("SDA", INPUT),
    .connect = on_i2c_connect,
    .read = on_i2c_read,
  };
  i2c_init(&i2c_config);
}