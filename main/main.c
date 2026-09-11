#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <inttypes.h>
#include <stdatomic.h>
#include <stdint.h>
#include "e18d80nk.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "i2c_config.h"
#include "mqtt_client.h"
#include "nvs_flash.h"
#include "oled_printf.h"
#include "oled_setup.h"
#include "protocol_examples_common.h"

#define SENSOR_GPIO GPIO_NUM_4
#define COUNT_COALESCE_US (1000 * 1000) /* detections within 1s count as one */
#define MQTT_TOPIC "pnaatos/poc"

static const char TAG[] = "main";

extern lv_disp_t *local_disp;

static esp_mqtt_client_handle_t mqtt_client;
static _Atomic bool mqtt_connected;
static _Atomic uint32_t prod_count;
static int64_t last_count_us; /* only touched by the sensor callback task */

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data) {
  switch ((esp_mqtt_event_id_t)event_id) {
  case MQTT_EVENT_CONNECTED:
    ESP_LOGI(TAG, "MQTT connected to broker");
    mqtt_connected = true;
    break;
  case MQTT_EVENT_DISCONNECTED:
    ESP_LOGI(TAG, "MQTT disconnected from broker");
    mqtt_connected = false;
    break;
  case MQTT_EVENT_ERROR:
    ESP_LOGE(TAG, "MQTT error");
    break;
  default:
    break;
  }
}

static void mqtt_app_start(void) {
  esp_mqtt_client_config_t mqtt_cfg = {
      .broker.address.uri = CONFIG_BROKER_URL,
  };

  mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
  if (mqtt_client == NULL) {
    ESP_LOGE(TAG, "Failed to initialize MQTT client");
    return;
  }
  esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID,
                                 mqtt_event_handler, NULL);
  ESP_ERROR_CHECK(esp_mqtt_client_start(mqtt_client));
}

static void obtain_time(void) {
  ESP_LOGI(TAG, "Initializing and starting SNTP");
  esp_sntp_config_t config =
      ESP_NETIF_SNTP_DEFAULT_CONFIG(CONFIG_SNTP_TIME_SERVER);
  ESP_ERROR_CHECK(esp_netif_sntp_init(&config));

  // Wait for the SNTP service to synchronize the clock. The internal sync
  // semaphore is a binary semaphore signaled once on the first successful
  // sync, so esp_netif_sntp_sync_wait() must not be called again after it
  // returns ESP_OK (a second call would block until the next periodic sync).
  int retry = 0;
  const int retry_count = 15;
  while (esp_netif_sntp_sync_wait(2000 / portTICK_PERIOD_MS) ==
             ESP_ERR_TIMEOUT &&
         ++retry < retry_count) {
    ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry,
             retry_count);
  }

  // The sync semaphore may already have been signaled (and consumed) before
  // the wait above started, so also verify the clock directly.
  time_t now = time(NULL);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  while (timeinfo.tm_year < (2016 - 1900)) {
    // Never start counting/publishing with a bogus (epoch 1970) timestamp:
    // keep waiting until the clock is actually synchronized.
    ESP_LOGW(TAG, "Still waiting for SNTP time sync...");
    vTaskDelay(pdMS_TO_TICKS(5000));
    time(&now);
    localtime_r(&now, &timeinfo);
  }
  ESP_LOGI(TAG, "System time synchronized");
}

// Runs in the e18d80nk_task context on every state change of the sensor.
// Counts only the start of each detection (object entering the beam) and
// coalesces multiple detections within the same 1s window into one increment.
static void sensor_callback(bool object_detected, void *ctx) {
  if (!object_detected) {
    return;
  }
  int64_t now_us = esp_timer_get_time();
  if (now_us - last_count_us >= COUNT_COALESCE_US) {
    last_count_us = now_us;
    atomic_fetch_add(&prod_count, 1);
  }
}

static void sensor_start(void) {
  e18d80nk_handle_t sensor;
  e18d80nk_config_t cfg = {
      .gpio_num = SENSOR_GPIO,
      .active_low = true,   /* most E18-D80NK modules pull the signal LOW on detection */
      .use_interrupt = true,
      .on_change = sensor_callback,
      .user_ctx = NULL,
  };
  ESP_ERROR_CHECK(e18d80nk_init(&cfg, &sensor));
}

static void publish_count(uint32_t count) {
  if (mqtt_client == NULL || !mqtt_connected) {
    return;
  }

  time_t now = time(NULL);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);

  char ts[32];
  strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", &timeinfo);

  char payload[96];
  snprintf(payload, sizeof(payload),
           "{\"count\": %" PRIu32 ", \"ts\": \"%s\"}", count, ts);

  int msg_id = esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC, payload, 0, 0, 0);
  if (msg_id < 0) {
    ESP_LOGE(TAG, "Failed to publish to %s", MQTT_TOPIC);
  } else {
    ESP_LOGI(TAG, "Published to %s: %s (msg_id=%d)", MQTT_TOPIC, payload,
             msg_id);
  }
}

void app_main(void) {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  ESP_ERROR_CHECK(example_connect());

  setenv("TZ", "UTC0", 1);
  tzset();
  obtain_time();

  mqtt_app_start();
  sensor_start();

  i2c_port_t i2c_port_num = I2C_NUM_0;
  initialize_i2c(&i2c_port_num);

  vTaskDelay(pdMS_TO_TICKS(100));
  configure_oled_screen(&i2c_port_num);
  oled_printf_init(local_disp);

  ESP_LOGI(TAG, "Enter in the main loop...");
  uint32_t last_count = 0;
  while (1) {
    uint32_t count = atomic_load(&prod_count);
    if (count != last_count) {
      last_count = count;
      printf_oled("Count: %lu", (unsigned long)count);
      publish_count(count);
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}