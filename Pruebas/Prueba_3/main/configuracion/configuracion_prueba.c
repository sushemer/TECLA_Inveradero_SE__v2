#include <stddef.h>

#include "configuracion_prueba.h"

#include <inttypes.h>

#include "esp_log.h"

static const char *TAG = "config_prueba";

/* Traduce las opciones de menuconfig a una configuración reutilizable por módulos. */
static const configuracion_prueba_t s_config = {
    .nodo = {
        .id_nodo = CONFIG_PRUEBA_ID_NODO,
        .perfil_nodo = CONFIG_PRUEBA_PERFIL_NODO,
        .cpu_activa = CONFIG_PRUEBA_CPU_ACTIVO == 1,
        .ventana_activa_s = CONFIG_PRUEBA_TIEMPO_VENTANA_ACTIVA_S,
        .espera_inicial_s = CONFIG_PRUEBA_ESPERA_INICIAL_S,
        .intervalo_lecturas_s = CONFIG_PRUEBA_INTERVALO_LECTURAS_S,
    },
    .comunicacion = {
        .recepcion_activada = CONFIG_PRUEBA_ESPNOW_RECEPCION == 1,
        .transmision_activada = CONFIG_PRUEBA_ESPNOW_TRANSMISION == 1,
        .canal = CONFIG_PRUEBA_ESPNOW_CANAL,
        .intervalo_transmision_ms = CONFIG_PRUEBA_ESPNOW_INTERVALO_TRANSMISION_MS,
    },
    .cargas = {
        .dht22 = CONFIG_PRUEBA_SENSOR_DHT22 == 1,
        .veml7700 = CONFIG_PRUEBA_SENSOR_VEML7700 == 1,
        .ads1115 = CONFIG_PRUEBA_SENSOR_ADS1115 == 1,
        .sen0193 = {
            CONFIG_PRUEBA_SENSOR_SEN0193_1 == 1,
            CONFIG_PRUEBA_SENSOR_SEN0193_2 == 1,
            CONFIG_PRUEBA_SENSOR_SEN0193_3 == 1,
            CONFIG_PRUEBA_SENSOR_SEN0193_4 == 1,
        },
        .divisor_bateria = CONFIG_PRUEBA_CARGA_DIVISOR_BATERIA == 1,
        .regulador_tps63020 = CONFIG_PRUEBA_CARGA_TPS63020 == 1,
    },
    .sueno_profundo = {
        .activado = CONFIG_PRUEBA_USAR_SUENO_PROFUNDO == 1,
        .duracion_s = CONFIG_PRUEBA_DURACION_SUENO_PROFUNDO_S,
    },
};

const configuracion_prueba_t *configuracion_prueba_obtener(void)
{
    return &s_config;
}

void configuracion_prueba_mostrar(const configuracion_prueba_t *config)
{
    if (config == NULL) return;

#if CONFIG_IDF_TARGET_ESP32C3
    ESP_LOGI(TAG, "Placa para la prueba: ESP32-C3");
#elif CONFIG_IDF_TARGET_ESP32
    ESP_LOGI(TAG, "Placa para la prueba: ESP32 / perfil WROOM-32");
#endif
    ESP_LOGI(TAG, "Nodo=%u perfil=%u", (unsigned)config->nodo.id_nodo,
             (unsigned)config->nodo.perfil_nodo);
    ESP_LOGI(TAG, "CPU activa=%u; ventana activa=%" PRIu32 " s",
             (unsigned)config->nodo.cpu_activa, config->nodo.ventana_activa_s);
    ESP_LOGI(TAG, "ESP-NOW: recepción=%u, transmisión=%u, canal=%u",
             (unsigned)config->comunicacion.recepcion_activada,
             (unsigned)config->comunicacion.transmision_activada,
             (unsigned)config->comunicacion.canal);
    ESP_LOGI(TAG, "Intervalo de transmisión=%" PRIu32 " ms",
             config->comunicacion.intervalo_transmision_ms);
    ESP_LOGI(TAG, "sueño profundo=%u; duración=%" PRIu32 " s",
             (unsigned)config->sueno_profundo.activado, config->sueno_profundo.duracion_s);
    ESP_LOGI(TAG, "Pausa inicial para conectar el medidor=%" PRIu32 " s",
             config->nodo.espera_inicial_s);
}
