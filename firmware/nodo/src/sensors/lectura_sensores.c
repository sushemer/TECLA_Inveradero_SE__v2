#include "lectura_sensores.h"

#include <math.h>

#include "dht.h"
#include "esp_log.h"

#include "../configuracion/configuracion_sensores.h"
#include "veml7700.h"
#include "sensor_validation.h"

static const char *ETIQUETA = "sensores";
#if TECLA_PROFILE_HAS_VEML7700
static esp_err_t s_estado_luz = ESP_ERR_INVALID_STATE;
#endif

static void agregar_lectura(tecla_resultado_sensores_t *resultado,
                            uint8_t id_sensor,
                            uint8_t tipo_valor,
                            uint8_t estado,
                            float valor)
{
    if (resultado->cantidad >= TECLA_MAX_LECTURAS) return;
    tecla_lectura_t *lectura = &resultado->lecturas[resultado->cantidad++];
    lectura->id_sensor = id_sensor;
    lectura->tipo_valor = tipo_valor;
    lectura->estado = estado;
    lectura->reservado = 0U;
    lectura->valor = valor;
}

esp_err_t tecla_inicializar_sensores(void)
{
#if TECLA_PROFILE_HAS_VEML7700
    s_estado_luz = tecla_veml7700_iniciar();
#endif
    return ESP_OK;
}

esp_err_t tecla_leer_sensores(
    tecla_resultado_sensores_t *resultado,
    const tecla_configuracion_nodo_t *configuracion)
{
    if (resultado == NULL || configuracion == NULL) return ESP_ERR_INVALID_ARG;
    resultado->cantidad = 0U;
    if (configuracion->estado_operativo == TECLA_NODO_EN_MANTENIMIENTO) {
        return ESP_OK;
    }

#if TECLA_PROFILE_HAS_DHT22
    if ((configuracion->mascara_sensores_habilitados &
         TECLA_MASCARA_SENSOR(TECLA_ID_SENSOR_DHT22)) != 0U) {
        float humedad = NAN;
        float temperatura = NAN;
        const esp_err_t estado = dht_read_float_data(
            DHT_TYPE_AM2301, TECLA_GPIO_DHT22, &humedad, &temperatura);
        const bool lectura_valida = estado == ESP_OK &&
            tecla_medicion_es_plausible(TECLA_TIPO_VALOR_TEMPERATURA, temperatura) &&
            tecla_medicion_es_plausible(TECLA_TIPO_VALOR_HUMEDAD, humedad);
        const uint8_t estado_sensor = lectura_valida
            ? TECLA_ESTADO_SENSOR_FUNCIONANDO
            : TECLA_ESTADO_SENSOR_LECTURA_INVALIDA;
        agregar_lectura(resultado, TECLA_ID_SENSOR_DHT22,
                        TECLA_TIPO_VALOR_TEMPERATURA, estado_sensor,
                        lectura_valida ? temperatura : NAN);
        agregar_lectura(resultado, TECLA_ID_SENSOR_DHT22,
                        TECLA_TIPO_VALOR_HUMEDAD, estado_sensor,
                        lectura_valida ? humedad : NAN);
    }
#endif

#if TECLA_PROFILE_HAS_VEML7700
    if ((configuracion->mascara_sensores_habilitados &
         TECLA_MASCARA_SENSOR(TECLA_ID_SENSOR_LUZ)) != 0U) {
        float lux = NAN;
        const esp_err_t estado = s_estado_luz == ESP_OK
            ? tecla_veml7700_leer_lux(&lux)
            : s_estado_luz;
        const bool lectura_valida = estado == ESP_OK &&
            tecla_medicion_es_plausible(TECLA_TIPO_VALOR_LUZ, lux);
        agregar_lectura(resultado, TECLA_ID_SENSOR_LUZ, TECLA_TIPO_VALOR_LUZ,
                        lectura_valida ? TECLA_ESTADO_SENSOR_FUNCIONANDO
                                       : TECLA_ESTADO_SENSOR_ERROR,
                        lectura_valida ? lux : NAN);
        if (!lectura_valida) {
            ESP_LOGW(ETIQUETA, "No se pudo leer el VEML7700: %s",
                     esp_err_to_name(estado));
        }
    }
#elif TECLA_PROFILE_HAS_SEN0193
    const uint8_t ids_suelo[] = {
        TECLA_ID_SENSOR_SEN0193_1, TECLA_ID_SENSOR_SEN0193_2,
        TECLA_ID_SENSOR_SEN0193_3, TECLA_ID_SENSOR_SEN0193_4,
    };
    for (size_t indice = 0; indice < sizeof(ids_suelo) / sizeof(ids_suelo[0]); indice++) {
        if ((configuracion->mascara_sensores_habilitados &
             TECLA_MASCARA_SENSOR(ids_suelo[indice])) != 0U) {
            agregar_lectura(resultado, ids_suelo[indice],
                            TECLA_TIPO_VALOR_HUMEDAD_SUELO,
                            TECLA_ESTADO_SENSOR_ERROR, NAN);
        }
    }
    if ((configuracion->mascara_sensores_habilitados &
         TECLA_MASCARA_SENSORES_DISPONIBLES) != 0U) {
        ESP_LOGW(ETIQUETA, "SEN0193 habilitado, pero el driver ADS1115 aún no está integrado");
    }
#endif

    return ESP_OK;
}
