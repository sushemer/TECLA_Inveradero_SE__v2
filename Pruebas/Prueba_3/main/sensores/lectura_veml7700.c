#include "lectura_veml7700.h"

#include <stdint.h>
#include "bus_i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define DIRECCION_VEML7700 0x10

esp_err_t lectura_veml7700_obtener(float *lux)
{
    const uint8_t configuracion[] = {0x00, 0x00};
    esp_err_t resultado = bus_i2c_escribir_registro(DIRECCION_VEML7700, 0x00,
                                                  configuracion, sizeof(configuracion));
    if (resultado != ESP_OK) return resultado;
    vTaskDelay(pdMS_TO_TICKS(120));

    uint8_t datos[2];
    resultado = bus_i2c_leer_registro(DIRECCION_VEML7700, 0x04, datos, sizeof(datos));
    if (resultado == ESP_OK) {
        const uint16_t cuentas = (uint16_t)datos[0] | ((uint16_t)datos[1] << 8);
        *lux = (float)cuentas * 0.0576f;
    }

    const uint8_t apagar[] = {0x01, 0x00};
    const esp_err_t resultado_apagado = bus_i2c_escribir_registro(
        DIRECCION_VEML7700, 0x00, apagar, sizeof(apagar));
    if (resultado != ESP_OK) return resultado;
    return resultado_apagado;
}

