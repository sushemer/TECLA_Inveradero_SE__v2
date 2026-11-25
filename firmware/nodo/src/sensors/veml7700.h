#ifndef TECLA_VEML7700_H
#define TECLA_VEML7700_H

#include "esp_err.h"

esp_err_t tecla_veml7700_iniciar(void);
esp_err_t tecla_veml7700_leer_lux(float *lux);

#endif
