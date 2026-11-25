#include "lectura_ads1115.h"

#include <stdint.h>
#include "esp_timer.h"
#include "bus_i2c.h"
#include "freertos/task.h"

#define DIRECCION_ADS1115 0x48
#define ADS1115_CONVERSION_LISTA 0x8000U
#define ADS1115_TIEMPO_LIMITE_US 20000

esp_err_t lectura_ads1115_obtener(uint8_t canal, int16_t *lectura)
{
    if (canal >= CANALES_ADS1115 || lectura == NULL) return ESP_ERR_INVALID_ARG;
    const uint16_t seleccion_canal = (uint16_t)(4U + canal) << 12;
    /* AINx contra GND, escala ±4.096 V, una muestra y 128 SPS. */
    const uint16_t configuracion = (uint16_t)(0x8000U | seleccion_canal | 0x0200U |
                                               0x0100U | 0x0080U | 0x0003U);
    const uint8_t bytes_configuracion[] = {
        (uint8_t)(configuracion >> 8), (uint8_t)configuracion
    };
    esp_err_t resultado = bus_i2c_escribir_registro(DIRECCION_ADS1115, 0x01,
                                                  bytes_configuracion, sizeof(bytes_configuracion));
    if (resultado != ESP_OK) return resultado;
    const int64_t limite = esp_timer_get_time() + ADS1115_TIEMPO_LIMITE_US;
    uint8_t datos[2];
    do {
        resultado = bus_i2c_leer_registro(DIRECCION_ADS1115, 0x01, datos, sizeof(datos));
        if (resultado != ESP_OK) return resultado;
        const uint16_t estado = (uint16_t)(((uint16_t)datos[0] << 8) | datos[1]);
        if ((estado & ADS1115_CONVERSION_LISTA) != 0U) break;
        vTaskDelay(pdMS_TO_TICKS(1));
    } while (esp_timer_get_time() < limite);

    if ((esp_timer_get_time() >= limite) &&
        ((((uint16_t)datos[0] << 8) | datos[1]) & ADS1115_CONVERSION_LISTA) == 0U) {
        return ESP_ERR_TIMEOUT;
    }

    resultado = bus_i2c_leer_registro(DIRECCION_ADS1115, 0x00, datos, sizeof(datos));
    if (resultado == ESP_OK) {
        *lectura = (int16_t)(((uint16_t)datos[0] << 8) | datos[1]);
    }
    return resultado;
}

esp_err_t lectura_ads1115_a_milivoltios(int16_t lectura, uint16_t *milivoltios)
{
    if (milivoltios == NULL || lectura < 0) return ESP_ERR_INVALID_ARG;
    /* PGA ±4.096 V: 125 µV por cuenta. Redondeo al mV más cercano. */
    *milivoltios = (uint16_t)(((uint32_t)lectura * 125U + 500U) / 1000U);
    return ESP_OK;
}

