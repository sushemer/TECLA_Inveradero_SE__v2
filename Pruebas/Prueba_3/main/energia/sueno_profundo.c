#include "sueno_profundo.h"

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_sleep.h"

/* El único modo de sueño del banco es sueño profundo con despertar por temporizador. */
bool energia_entrar_sueno_profundo(const prueba_sueno_profundo_config_t *config)
{
    if (config == NULL || !config->activado) return false;

    ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup((uint64_t)config->duracion_s * 1000000ULL));
    esp_deep_sleep_start();
    return true;
}
