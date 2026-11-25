#include "lectura_dht22.h"

#include <stdint.h>
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"

#define PIN_DATOS_DHT22 GPIO_NUM_4

esp_err_t lectura_dht22_iniciar(void)
{
    const gpio_config_t configuracion = {
        .pin_bit_mask = 1ULL << GPIO_NUM_4,
        .mode = GPIO_MODE_INPUT_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    return gpio_config(&configuracion);
}

esp_err_t lectura_dht22_obtener(float *temperatura, float *humedad)
{
    if (temperatura == NULL || humedad == NULL) return ESP_ERR_INVALID_ARG;
    uint8_t datos[5] = {0};
    esp_err_t resultado = gpio_set_direction(PIN_DATOS_DHT22, GPIO_MODE_OUTPUT);
    if (resultado != ESP_OK) return resultado;
    resultado = gpio_set_level(PIN_DATOS_DHT22, 0);
    if (resultado != ESP_OK) return resultado;
    esp_rom_delay_us(2000);
    resultado = gpio_set_level(PIN_DATOS_DHT22, 1);
    if (resultado != ESP_OK) return resultado;
    esp_rom_delay_us(30);
    resultado = gpio_set_direction(PIN_DATOS_DHT22, GPIO_MODE_INPUT);
    if (resultado != ESP_OK) return resultado;
    resultado = gpio_pullup_en(PIN_DATOS_DHT22);
    if (resultado != ESP_OK) return resultado;

    int64_t inicio = esp_timer_get_time();
    while (gpio_get_level(PIN_DATOS_DHT22) != 0) {
        if (esp_timer_get_time() - inicio > 200) return ESP_ERR_TIMEOUT;
    }
    inicio = esp_timer_get_time();
    while (gpio_get_level(PIN_DATOS_DHT22) != 1) {
        if (esp_timer_get_time() - inicio > 200) return ESP_ERR_TIMEOUT;
    }
    inicio = esp_timer_get_time();
    while (gpio_get_level(PIN_DATOS_DHT22) != 0) {
        if (esp_timer_get_time() - inicio > 200) return ESP_ERR_TIMEOUT;
    }

    for (int bit = 0; bit < 40; bit++) {
        inicio = esp_timer_get_time();
        while (gpio_get_level(PIN_DATOS_DHT22) != 1) {
            if (esp_timer_get_time() - inicio > 200) return ESP_ERR_TIMEOUT;
        }
        esp_rom_delay_us(40);
        datos[bit / 8] <<= 1;
        if (gpio_get_level(PIN_DATOS_DHT22)) datos[bit / 8] |= 1;
        if (bit < 39) {
            inicio = esp_timer_get_time();
            while (gpio_get_level(PIN_DATOS_DHT22) != 0) {
                if (esp_timer_get_time() - inicio > 200) return ESP_ERR_TIMEOUT;
            }
        }
    }

    if ((uint8_t)(datos[0] + datos[1] + datos[2] + datos[3]) != datos[4]) {
        return ESP_ERR_INVALID_CRC;
    }

    *humedad = (float)(((uint16_t)datos[0] << 8) | datos[1]) / 10.0f;
    const uint16_t lectura_temperatura = ((uint16_t)(datos[2] & 0x7f) << 8) | datos[3];
    *temperatura = (float)lectura_temperatura / 10.0f;
    if (datos[2] & 0x80) *temperatura = -*temperatura;
    return ESP_OK;
}

