#include "e18d80nk.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include <stdlib.h>

static const char *TAG = "e18d80nk";

struct e18d80nk_dev_t {
    gpio_num_t gpio_num;
    bool active_low;
    bool use_interrupt;
    e18d80nk_callback_t on_change;
    void *user_ctx;
    QueueHandle_t evt_queue;
    TaskHandle_t task_handle;
    bool last_state;
};

#define DEBOUNCE_MS 40

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    struct e18d80nk_dev_t *dev = (struct e18d80nk_dev_t *)arg;
    BaseType_t high_task_wakeup = pdFALSE;
    uint32_t dummy = 0;
    xQueueSendFromISR(dev->evt_queue, &dummy, &high_task_wakeup);
    if (high_task_wakeup) {
        portYIELD_FROM_ISR();
    }
}

static bool read_raw(struct e18d80nk_dev_t *dev)
{
    int level = gpio_get_level(dev->gpio_num);
    bool detected = dev->active_low ? (level == 0) : (level == 1);
    return detected;
}

static void e18d80nk_task(void *arg)
{
    struct e18d80nk_dev_t *dev = (struct e18d80nk_dev_t *)arg;
    uint32_t dummy;
    for (;;) {
        if (xQueueReceive(dev->evt_queue, &dummy, portMAX_DELAY)) {
            // debounce simples
            vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_MS));
            bool detected = read_raw(dev);
            if (detected != dev->last_state) {
                dev->last_state = detected;
                if (dev->on_change) {
                    dev->on_change(detected, dev->user_ctx);
                }
            }
        }
    }
}

esp_err_t e18d80nk_init(const e18d80nk_config_t *config, e18d80nk_handle_t *out_handle)
{
    if (!config || !out_handle) {
        return ESP_ERR_INVALID_ARG;
    }

    struct e18d80nk_dev_t *dev = calloc(1, sizeof(struct e18d80nk_dev_t));
    if (!dev) {
        return ESP_ERR_NO_MEM;
    }

    dev->gpio_num = config->gpio_num;
    dev->active_low = config->active_low;
    dev->use_interrupt = config->use_interrupt;
    dev->on_change = config->on_change;
    dev->user_ctx = config->user_ctx;

    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << dev->gpio_num,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,   // saída NPN é coletor aberto -> precisa de pull-up
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = dev->use_interrupt ? GPIO_INTR_ANYEDGE : GPIO_INTR_DISABLE,
    };

    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config falhou: %s", esp_err_to_name(err));
        free(dev);
        return err;
    }

    dev->last_state = read_raw(dev);

    if (dev->use_interrupt) {
        dev->evt_queue = xQueueCreate(10, sizeof(uint32_t));
        if (!dev->evt_queue) {
            free(dev);
            return ESP_ERR_NO_MEM;
        }

        BaseType_t ok = xTaskCreate(e18d80nk_task, "e18d80nk_task", 2048, dev, 10, &dev->task_handle);
        if (ok != pdPASS) {
            vQueueDelete(dev->evt_queue);
            free(dev);
            return ESP_ERR_NO_MEM;
        }

        err = gpio_install_isr_service(0);
        // ESP_ERR_INVALID_STATE significa que o serviço já foi instalado por outro componente; tudo bem.
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "gpio_install_isr_service falhou: %s", esp_err_to_name(err));
            vTaskDelete(dev->task_handle);
            vQueueDelete(dev->evt_queue);
            free(dev);
            return err;
        }

        err = gpio_isr_handler_add(dev->gpio_num, gpio_isr_handler, dev);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "gpio_isr_handler_add falhou: %s", esp_err_to_name(err));
            vTaskDelete(dev->task_handle);
            vQueueDelete(dev->evt_queue);
            free(dev);
            return err;
        }
    }

    *out_handle = dev;
    ESP_LOGI(TAG, "Sensor E18-D80NK inicializado no GPIO%d (active_low=%d, interrupt=%d)",
             dev->gpio_num, dev->active_low, dev->use_interrupt);
    return ESP_OK;
}

bool e18d80nk_is_object_detected(e18d80nk_handle_t handle)
{
    if (!handle) {
        return false;
    }
    return read_raw(handle);
}

esp_err_t e18d80nk_deinit(e18d80nk_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    if (handle->use_interrupt) {
        gpio_isr_handler_remove(handle->gpio_num);
        if (handle->task_handle) {
            vTaskDelete(handle->task_handle);
        }
        if (handle->evt_queue) {
            vQueueDelete(handle->evt_queue);
        }
    }
    free(handle);
    return ESP_OK;
}