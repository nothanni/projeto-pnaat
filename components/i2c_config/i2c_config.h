#ifndef __I2C_CONFIG_H__
#define __I2C_CONFIG_H__

#include "driver/i2c.h"

#define PIN_NUM_SDA 17
#define PIN_NUM_SCL 18

void initialize_i2c(i2c_port_t *i2c_bus);
#endif // __I2C_CONFIG_H_