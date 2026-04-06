#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  uint8_t mode;
} chip_state_t;

static uint8_t on_i2c_read(void *user_data) {
  // Alternates between sending SpO2 (98) and Heart Rate (120)
  static bool flip = false;
  flip = !flip;
  return flip ? 98 : 120;
}

bool on_i2c_connect(void *user_data, uint32_t address, bool connect) {
  return address == 0x57; // Standard MAX30102 address
}

void chip_init() {
  chip_state_t *chip = malloc(sizeof(chip_state_t));
  const i2c_config_t i2c_config = {
    .user_data = chip,
    .address = 0x57,
    .scl = pin_init("SCL", INPUT),
    .sda = pin_init("SDA", INPUT),
    .connect = on_i2c_connect,
    .read = on_i2c_read,
  };
  i2c_init(&i2c_config);
}