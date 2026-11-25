#include "veml7700.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "../configuracion/configuracion_sensores.h"

#define TECLA_I2C_FRECUENCIA_HZ 100000U
#define TECLA_I2C_TIEMPO_ESPERA_MS 100U
#define TECLA_VEML7700_REG_CONFIG 0x00U
#define TECLA_VEML7700_REG_ALS 0x04U

static bool s_i2c_inicializado;

static esp_err_t escribir_registro(uint8_t registro, uint16_t valor)
{
    const uint8_t datos[] = {
        registro, (uint8_t)(valor & 0xffU), (uint8_t)(valor >> 8),
    };
    return i2c_master_write_to_device(I2C_NUM_0, TECLA_DIRECCION_VEML7700,
                                      datos, sizeof(datos),
                                      pdMS_TO_TICKS(TECLA_I2C_TIEMPO_ESPERA_MS));
}

esp_err_t tecla_veml7700_iniciar(void)
{
    if (!s_i2c_inicializado) {
        const i2c_config_t configuracion = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = TECLA_GPIO_I2C_SDA,
            .scl_io_num = TECLA_GPIO_I2C_SCL,
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
            .master.clk_speed = TECLA_I2C_FRECUENCIA_HZ,
        };
        esp_err_t resultado = i2c_param_config(I2C_NUM_0, &configuracion);
        if (resultado != ESP_OK) return resultado;
        resultado = i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
        if (resultado != ESP_OK && resultado != ESP_ERR_INVALID_STATE) return resultado;
        s_i2c_inicializado = true;
    }

    /* Ganancia 1/8 e integración de 100 ms. */
    return escribir_registro(TECLA_VEML7700_REG_CONFIG, 0x0000U);
}

esp_err_t tecla_veml7700_leer_lux(float *lux)
{
    if (lux == NULL) return ESP_ERR_INVALID_ARG;
    if (!s_i2c_inicializado) return ESP_ERR_INVALID_STATE;

    vTaskDelay(pdMS_TO_TICKS(120));
    uint8_t registro = TECLA_VEML7700_REG_ALS;
    uint8_t datos[2] = {0};
    esp_err_t resultado = i2c_master_write_read_device(
        I2C_NUM_0, TECLA_DIRECCION_VEML7700, &registro, sizeof(registro),
        datos, sizeof(datos), pdMS_TO_TICKS(TECLA_I2C_TIEMPO_ESPERA_MS));
    if (resultado != ESP_OK) return resultado;

    const uint16_t cuentas = (uint16_t)((uint16_t)datos[0] |
                                        ((uint16_t)datos[1] << 8));
    *lux = (float)cuentas * 0.0576f;
    return ESP_OK;
}
