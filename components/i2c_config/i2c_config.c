#include "i2c_config.h"
#include "esp_log.h"

static const char TAG[] = "i2c_config";

void initialize_i2c(i2c_port_t *i2c_bus) {
  ESP_LOGI(TAG, "Initialize I2C bus");
  i2c_config_t conf = {
      .mode = I2C_MODE_MASTER,
      .sda_io_num = PIN_NUM_SDA,
      .scl_io_num = PIN_NUM_SCL,
      .sda_pullup_en = GPIO_PULLUP_ENABLE,
      .scl_pullup_en = GPIO_PULLUP_ENABLE,
      .master.clk_speed = 500000,
      .clk_flags = 0,
  };
  ESP_ERROR_CHECK(i2c_param_config(*i2c_bus, &conf));
  ESP_ERROR_CHECK(i2c_driver_install(*i2c_bus, conf.mode, 0, 0, 0));
  ESP_LOGI(TAG, "Initialize I2C bus done");
}