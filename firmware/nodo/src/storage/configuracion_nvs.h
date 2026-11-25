#ifndef TECLA_CONFIGURACION_NVS_H
#define TECLA_CONFIGURACION_NVS_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "../configuracion/configuracion_remota.h"

esp_err_t tecla_configuracion_nvs_iniciar(void);
esp_err_t tecla_configuracion_nvs_cargar(
    tecla_configuracion_nodo_t *configuracion,
    uint32_t id_nodo_local,
    uint8_t mascara_sensores_disponibles,
    uint8_t mascara_actuadores_disponibles,
    bool *encontrada);
esp_err_t tecla_configuracion_nvs_guardar(
    const tecla_configuracion_nodo_t *configuracion);

#endif
