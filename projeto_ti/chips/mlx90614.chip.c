#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  uint32_t mode_attr;
} chip_state_t;

static uint8_t on_i2c_read(void *user_data) {
  chip_state_t *chip = (chip_state_t *)user_data;
  
  // This line reads the "mode" slider from the UI
  uint32_t mode = attr_read(chip->mode_attr);

  switch (mode) {
    case 0: return 36 + (rand() % 2); // Ideal (36-37°C)
    case 1: return 38 + (rand() % 2); // Warning (38-39°C)
    case 2: return 40 + (rand() % 3); // Critical (40-42°C)
    default: return 37;
  }
}

bool on_i2c_connect(void *user_data, uint32_t address, bool connect) {
  return address == 0x5A;
}

void chip_init() {
  chip_state_t *chip = malloc(sizeof(chip_state_t));
  
  // Initialize the attribute to match the ID in your .json file
  chip->mode_attr = attr_init("mode", 0); 

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