#include "bus_i2c.h"

#include <stdbool.h>

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"

#if CONFIG_IDF_TARGET_ESP32C3
#define PIN_SDA_I2C GPIO_NUM_5
#define PIN_SCL_I2C GPIO_NUM_6
#else
#define PIN_SDA_I2C GPIO_NUM_21
#define PIN_SCL_I2C GPIO_NUM_22
#endif

static bool s_i2c_iniciado;

esp_err_t bus_i2c_iniciar(void)
{
    if (s_i2c_iniciado) return ESP_OK;

    const i2c_config_t configuracion = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_SDA_I2C,
        .scl_io_num = PIN_SCL_I2C,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    esp_err_t resultado = i2c_param_config(I2C_NUM_0, &configuracion);
    if (resultado != ESP_OK) return resultado;

    resultado = i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
    if (resultado == ESP_OK || resultado == ESP_ERR_INVALID_STATE) {
        s_i2c_iniciado = true;
        return ESP_OK;
    }
    return resultado;
}

esp_err_t bus_i2c_escribir_registro(uint8_t direccion, uint8_t registro,
                                     const uint8_t *datos, size_t cantidad)
{
    uint8_t mensaje[3] = {registro, 0, 0};
    if (cantidad > 2) return ESP_ERR_INVALID_SIZE;
    for (size_t indice = 0; indice < cantidad; indice++) {
        mensaje[indice + 1] = datos[indice];
    }
    return i2c_master_write_to_device(I2C_NUM_0, direccion, mensaje, cantidad + 1,
                                      pdMS_TO_TICKS(100));
}

esp_err_t bus_i2c_leer_registro(uint8_t direccion, uint8_t registro,
                                 uint8_t *datos, size_t cantidad)
{
    return i2c_master_write_read_device(I2C_NUM_0, direccion, &registro, 1,
                                        datos, cantidad, pdMS_TO_TICKS(100));
}

