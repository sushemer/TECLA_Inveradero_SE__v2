#pragma once

#include <stdint.h>
#include "esp_err.h"

#define CANALES_ADS1115 4

esp_err_t lectura_ads1115_obtener(uint8_t canal, int16_t *lectura);
esp_err_t lectura_ads1115_a_milivoltios(int16_t lectura, uint16_t *milivoltios);
