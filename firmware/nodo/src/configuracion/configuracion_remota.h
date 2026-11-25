#ifndef TECLA_CONFIGURACION_REMOTA_H
#define TECLA_CONFIGURACION_REMOTA_H

#include <stdbool.h>
#include <stdint.h>

#include "tecla_protocol.h"

typedef struct {
    uint16_t intervalo_lectura_s;
    uint8_t mascara_sensores_habilitados;
    uint8_t mascara_actuadores_habilitados;
    uint8_t estado_operativo;
    uint8_t destinos_lectura;
} tecla_configuracion_nodo_t;

/* Rechaza el comando completo si no coincide con el hardware o el nodo local. */
bool tecla_configuracion_remota_aplicar(
    tecla_configuracion_nodo_t *actual,
    const tecla_comando_configuracion_t *comando,
    uint32_t id_nodo_local,
    uint8_t mascara_sensores_disponibles,
    uint8_t mascara_actuadores_disponibles);

bool tecla_configuracion_remota_validar(
    const tecla_comando_configuracion_t *comando,
    uint32_t id_nodo_local,
    uint8_t mascara_sensores_disponibles,
    uint8_t mascara_actuadores_disponibles);

#endif
