#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  uint32_t condition_attr;
} chip_state_t;

/*
  condition 0 → Healthy       : HR 65–80,  SpO2 97–99
  condition 1 → Stressed/Ill  : HR 105–120, SpO2 94–96
  condition 2 → Critical       : HR 130–145, SpO2 86–89
  condition 3 → Bradycardia    : HR 45–55,  SpO2 93–95  (low perf index)
  condition 4 → Poor circ.     : HR 70–80,  SpO2 91–93  (low PI)

  Flip trick: alternate between two reads to give HR on read-1 and SpO2 on read-2.
*/
static bool flip = false;

static uint8_t on_i2c_read(void *user_data) {
  chip_state_t *chip = (chip_state_t *)user_data;
  uint32_t cond = attr_read(chip->condition_attr);
  flip = !flip;

  if (flip) {
    /* Heart-rate byte */
    switch (cond) {
      case 0: return 65  + (rand() % 15);
      case 1: return 105 + (rand() % 15);
      case 2: return 130 + (rand() % 15);
      case 3: return 45  + (rand() % 10);
      case 4: return 70  + (rand() % 10);
      default: return 72;
    }
  } else {
    /* SpO2 byte */
    switch (cond) {
      case 0: return 97 + (rand() % 2);
      case 1: return 94 + (rand() % 2);
      case 2: return 86 + (rand() % 3);
      case 3: return 93 + (rand() % 2);
      case 4: return 91 + (rand() % 2);
      default: return 98;
    }
  }
}

bool on_i2c_connect(void *user_data, uint32_t address, bool connect) {
  return address == 0x57;
}

void chip_init() {
  chip_state_t *chip = malloc(sizeof(chip_state_t));
  chip->condition_attr = attr_init("condition", 0);
  const i2c_config_t i2c_config = {
    .user_data = chip,
    .address   = 0x57,
    .scl       = pin_init("SCL", INPUT),
    .sda       = pin_init("SDA", INPUT),
    .connect   = on_i2c_connect,
    .read      = on_i2c_read,
  };
  i2c_init(&i2c_config);
}