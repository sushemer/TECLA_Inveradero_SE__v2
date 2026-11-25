#ifndef TECLA_SENSOR_VALIDATION_H
#define TECLA_SENSOR_VALIDATION_H

#include <stdbool.h>
#include <stdint.h>

#include "tecla_protocol.h"

/* Local plausibility checks only; these do not replace calibration. */
bool tecla_medicion_es_plausible(uint8_t tipo_valor, float valor);

#endif
