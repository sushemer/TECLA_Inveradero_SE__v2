#ifndef TECLA_NODE_WIRE_V3_ESPNOW_H
#define TECLA_NODE_WIRE_V3_ESPNOW_H

#include <stdint.h>

#include "esp_err.h"
#include "../configuracion/configuracion_remota.h"
#include "../sensors/lectura_sensores.h"

esp_err_t tecla_node_wire_v3_radio_start(uint32_t firmware_node_id);
esp_err_t tecla_node_wire_v3_radio_send_sample(
    const tecla_configuracion_nodo_t *configuration,
    const tecla_resultado_sensores_t *sample);

#endif
