#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  uint32_t condition_attr;
} chip_state_t;

static uint8_t on_i2c_read(void *user_data) {
  chip_state_t *chip = (chip_state_t *)user_data;
  
  // Read the "condition" slider from the UI
  uint32_t condition = attr_read(chip->condition_attr);
  
  static bool flip = false;
  flip = !flip;

  if (condition == 0) {        // IDEAL
    return flip ? 98 : 72;     // 98% SpO2, 72 BPM
  } else if (condition == 1) {  // WARNING
    return flip ? 94 : 105;    // 94% SpO2, 105 BPM
  } else {                     // CRITICAL
    return flip ? 88 : 140;    // 88% SpO2, 140 BPM
  }
}

bool on_i2c_connect(void *user_data, uint32_t address, bool connect) {
  return address == 0x57;
}

void chip_init() {
  chip_state_t *chip = malloc(sizeof(chip_state_t));
  
  // Initialize the attribute to match the ID in your .json file
  chip->condition_attr = attr_init("condition", 0);

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