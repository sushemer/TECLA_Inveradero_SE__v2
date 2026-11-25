#pragma once

#include "esp_err.h"

esp_err_t lectura_dht22_iniciar(void);
esp_err_t lectura_dht22_obtener(float *temperatura, float *humedad);
