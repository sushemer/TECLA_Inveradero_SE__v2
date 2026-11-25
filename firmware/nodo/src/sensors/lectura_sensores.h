#ifndef TECLA_LECTURA_SENSORES_H
#define TECLA_LECTURA_SENSORES_H

#include <stddef.h>

#include "esp_err.h"
#include "tecla_protocol.h"
#include "../configuracion/configuracion_remota.h"

typedef struct {
    tecla_lectura_t lecturas[TECLA_MAX_LECTURAS];
    size_t cantidad;
} tecla_resultado_sensores_t;

esp_err_t tecla_inicializar_sensores(void);
esp_err_t tecla_leer_sensores(
    tecla_resultado_sensores_t *resultado,
    const tecla_configuracion_nodo_t *configuracion);

#endif
