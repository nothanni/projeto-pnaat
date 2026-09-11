// Exemplo de uso do componente e18d80nk em main/main.c

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "e18d80nk.h"

static const char *TAG = "app";

#define SENSOR_GPIO GPIO_NUM_4

// Callback chamado automaticamente quando o estado muda (modo interrupção)
static void sensor_callback(bool object_detected, void *ctx)
{
    if (object_detected) {
        ESP_LOGI(TAG, "Objeto detectado!");
    } else {
        ESP_LOGI(TAG, "Sem objeto.");
    }
}

void example_app_main(void)
{
    e18d80nk_handle_t sensor;

    e18d80nk_config_t config = {
        .gpio_num = SENSOR_GPIO,
        .active_low = true,      // a maioria dos módulos E18-D80NK puxa o sinal para LOW ao detectar
        .use_interrupt = true,   // modo evento/interrupção (recomendado)
        .on_change = sensor_callback,
        .user_ctx = NULL,
    };

    esp_err_t err = e18d80nk_init(&config, &sensor);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao iniciar sensor: %s", esp_err_to_name(err));
        return;
    }

    // --- Alternativa: modo polling, sem interrupção ---
    // config.use_interrupt = false;
    // config.on_change = NULL;
    // e18d80nk_init(&config, &sensor);
    // while (1) {
    //     bool detectado = e18d80nk_is_object_detected(sensor);
    //     ESP_LOGI(TAG, "Detectado: %d", detectado);
    //     vTaskDelay(pdMS_TO_TICKS(200));
    // }

    // No modo interrupção, o app_main pode seguir fazendo outras coisas.
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}