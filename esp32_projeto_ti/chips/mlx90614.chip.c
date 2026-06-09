#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  uint32_t mode_attr;
} chip_state_t;

/*
  mode 0 → Normal / Healthy  : 36–37°C
  mode 1 → Mild fever         : 37.5–38.5°C
  mode 2 → High fever         : 39.5–40.5°C
  mode 3 → Mild hypothermia   : 32–34°C
  mode 4 → Severe hypothermia : 28–31°C
*/
static uint8_t on_i2c_read(void *user_data) {
  chip_state_t *chip = (chip_state_t *)user_data;
  uint32_t mode = attr_read(chip->mode_attr);
  switch (mode) {
    case 0:  return 36 + (rand() % 2);
    case 1:  return 37 + (rand() % 2);   /* 37–38 */
    case 2:  return 39 + (rand() % 2);   /* 39–40 */
    case 3:  return 32 + (rand() % 3);   /* 32–34 */
    case 4:  return 28 + (rand() % 3);   /* 28–30 */
    default: return 37;
  }
}

bool on_i2c_connect(void *user_data, uint32_t address, bool connect) {
  return address == 0x5A;
}

void chip_init() {
  chip_state_t *chip = malloc(sizeof(chip_state_t));
  chip->mode_attr = attr_init("mode", 0);
  const i2c_config_t i2c_config = {
    .user_data = chip,
    .address   = 0x5A,
    .scl       = pin_init("SCL", INPUT),
    .sda       = pin_init("SDA", INPUT),
    .connect   = on_i2c_connect,
    .read      = on_i2c_read,
  };
  i2c_init(&i2c_config);
}