#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "i2c_config.h"
#include "oled_printf.h"
#include "oled_setup.h"
#include <stdio.h>

#define FRONT_SENSOR_PIN 36

static const char TAG[] = "main";

extern lv_disp_t *local_disp;

void app_main(void) {
  i2c_port_t i2c_port_num = I2C_NUM_0;
  initialize_i2c(&i2c_port_num);

  gpio_config_t io_conf = {
      .intr_type = GPIO_INTR_DISABLE,
      .mode = GPIO_MODE_OUTPUT,
      .pin_bit_mask = (1ULL << FRONT_SENSOR_PIN),
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .pull_up_en = GPIO_PULLUP_ENABLE,
  };
  esp_err_t err = gpio_config(&io_conf);
  if (err != ESP_OK) {
    printf("GPIO config failed: %d", err);
  }
  err = gpio_set_level(FRONT_SENSOR_PIN, 0);
  if (err != ESP_OK) {
    printf("Error to set pin level");
  }

  vTaskDelay(pdMS_TO_TICKS(100));
  configure_oled_screen(&i2c_port_num);
  oled_printf_init(local_disp);

  ESP_LOGI(TAG, "Enter in the main loop...");
  int count = 0;
  while (1) {
    printf_oled("Count: %d", count++);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
