#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

esp_err_t bus_i2c_iniciar(void);
esp_err_t bus_i2c_escribir_registro(uint8_t direccion, uint8_t registro,
                                     const uint8_t *datos, size_t cantidad);
esp_err_t bus_i2c_leer_registro(uint8_t direccion, uint8_t registro,
                                 uint8_t *datos, size_t cantidad);
