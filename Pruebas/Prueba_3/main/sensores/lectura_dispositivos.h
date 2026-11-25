#pragma once

#include "esp_err.h"
#include "configuracion/configuracion_prueba.h"

void lectura_dispositivos_mostrar(const prueba_cargas_config_t *cargas);
esp_err_t lectura_dispositivos_iniciar(const prueba_cargas_config_t *cargas);
void lectura_dispositivos_leer(const prueba_cargas_config_t *cargas);
