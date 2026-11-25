#include "lectura_dispositivos.h"

#include <stdint.h>
#include "esp_log.h"
#include "bus_i2c.h"
#include "lectura_ads1115.h"
#include "lectura_dht22.h"
#include "lectura_veml7700.h"

static const char *ETIQUETA = "lectura_dispositivos";

void lectura_dispositivos_mostrar(const prueba_cargas_config_t *cargas)
{
    if (cargas == NULL) return;

    ESP_LOGI(ETIQUETA, "Cargas conectadas (0=no, 1=sí):");
    ESP_LOGI(ETIQUETA, "DHT22=%u VEML7700=%u ADS1115=%u",
             (unsigned)cargas->dht22, (unsigned)cargas->veml7700,
             (unsigned)cargas->ads1115);
    ESP_LOGI(ETIQUETA, "SEN0193_1=%u SEN0193_2=%u SEN0193_3=%u SEN0193_4=%u",
             (unsigned)cargas->sen0193[0], (unsigned)cargas->sen0193[1],
             (unsigned)cargas->sen0193[2], (unsigned)cargas->sen0193[3]);
    ESP_LOGI(ETIQUETA, "Divisor_220k_100k=%u TPS63020=%u",
             (unsigned)cargas->divisor_bateria, (unsigned)cargas->regulador_tps63020);
}

esp_err_t lectura_dispositivos_iniciar(const prueba_cargas_config_t *cargas)
{
    if (cargas == NULL) return ESP_ERR_INVALID_ARG;

    if (cargas->dht22) {
        esp_err_t resultado = lectura_dht22_iniciar();
        if (resultado != ESP_OK) return resultado;
    }
    if (cargas->veml7700 || cargas->ads1115) {
        return bus_i2c_iniciar();
    }
    return ESP_OK;
}

void lectura_dispositivos_leer(const prueba_cargas_config_t *cargas)
{
    if (cargas == NULL) return;

    if (cargas->dht22) {
        float temperatura, humedad;
        esp_err_t resultado = lectura_dht22_obtener(&temperatura, &humedad);
        if (resultado == ESP_OK) {
            ESP_LOGI(ETIQUETA, "DHT22: temperatura=%.1f C, humedad=%.1f %%", temperatura, humedad);
        } else {
            ESP_LOGW(ETIQUETA, "No se pudo leer el DHT22 (error %s)", esp_err_to_name(resultado));
        }
    }

    if (cargas->veml7700) {
        float lux;
        esp_err_t resultado = lectura_veml7700_obtener(&lux);
        if (resultado == ESP_OK) {
            ESP_LOGI(ETIQUETA, "VEML7700: %.2f lux", lux);
        } else {
            ESP_LOGW(ETIQUETA, "No se pudo leer el VEML7700 (error %s)",
                     esp_err_to_name(resultado));
        }
    }

    if (cargas->ads1115) {
        for (uint8_t canal = 0; canal < CANALES_ADS1115; canal++) {
            if (!cargas->sen0193[canal]) continue;
            int16_t lectura;
            esp_err_t resultado = lectura_ads1115_obtener(canal, &lectura);
            if (resultado == ESP_OK) {
                uint16_t milivoltios = 0U;
                resultado = lectura_ads1115_a_milivoltios(lectura, &milivoltios);
                if (resultado == ESP_OK) {
                    ESP_LOGI(ETIQUETA, "SEN0193_%u / ADS1115 A%u: ADC=%d, %u mV",
                             (unsigned)(canal + 1), (unsigned)canal, (int)lectura,
                             (unsigned)milivoltios);
                }
            }
            if (resultado != ESP_OK) {
                ESP_LOGW(ETIQUETA, "No se pudo leer ADS1115 A%u (error %s)",
                         (unsigned)canal, esp_err_to_name(resultado));
            }
        }
    } else {
        for (uint8_t canal = 0; canal < CANALES_ADS1115; canal++) {
            if (cargas->sen0193[canal]) {
                ESP_LOGW(ETIQUETA, "SEN0193_%u está marcado, pero falta activar ADS1115",
                         (unsigned)(canal + 1));
            }
        }
    }
}
