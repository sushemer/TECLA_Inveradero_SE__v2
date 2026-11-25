#pragma once

#include "esp_err.h"
#include "configuracion/configuracion_prueba.h"

void prueba_espnow_iniciar(const prueba_comunicacion_config_t *config);
void prueba_espnow_detener(void);
esp_err_t prueba_espnow_transmitir(void);
