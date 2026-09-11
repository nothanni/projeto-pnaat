#pragma once

#include <stdbool.h>
#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Tipo de callback chamado quando o estado de detecção muda.
 * @param object_detected true = objeto detectado dentro do alcance ajustado (3-80cm)
 */
typedef void (*e18d80nk_callback_t)(bool object_detected, void *user_ctx);

typedef struct {
    gpio_num_t gpio_num;        // GPIO ligado ao fio de sinal (preto) do sensor
    bool active_low;            // true (padrão) = sensor puxa o pino para LOW ao detectar objeto
    bool use_interrupt;         // true = usa interrupção (recomendado), false = só leitura por polling
    e18d80nk_callback_t on_change; // opcional, pode ser NULL se use_interrupt = false
    void *user_ctx;             // contexto passado ao callback
} e18d80nk_config_t;

typedef struct e18d80nk_dev_t *e18d80nk_handle_t;

/**
 * Inicializa o sensor E18-D80NK em um GPIO.
 */
esp_err_t e18d80nk_init(const e18d80nk_config_t *config, e18d80nk_handle_t *out_handle);

/**
 * Leitura direta e instantânea (polling).
 * Retorna true se objeto detectado dentro da faixa ajustada no sensor.
 */
bool e18d80nk_is_object_detected(e18d80nk_handle_t handle);

/**
 * Libera recursos (remove interrupção, se houver).
 */
esp_err_t e18d80nk_deinit(e18d80nk_handle_t handle);

#ifdef __cplusplus
}
#endif