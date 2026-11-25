#ifndef TECLA_MODO_OPERACION_H
#define TECLA_MODO_OPERACION_H

#include <stdbool.h>

// Rol lógico que puede ejecutar el firmware compilado.
typedef enum {
    TECLA_MODO_NODO = 0,
    TECLA_MODO_NODO_GATEWAY = 1,
    TECLA_MODO_GATEWAY = 2
} tecla_modo_operacion_t;

// Transporte usado para enviar la telemetría propia.
typedef enum {
    TECLA_RUTA_DIRECTA_API = 0,
    TECLA_RUTA_GATEWAY_ESPNOW = 1
} tecla_ruta_telemetria_t;

// Capacidades generales habilitadas para el rol elegido.
typedef struct {
    tecla_modo_operacion_t modo;
    tecla_ruta_telemetria_t ruta_telemetria;
    bool leer_sensores_propios;
    bool enviar_telemetria_propia;
    bool recibir_telemetria_de_otros_nodos;
} tecla_configuracion_modo_t;

// La primera versión del firmware se compila con el rol de nodo.
#define TECLA_MODO_ACTUAL TECLA_MODO_NODO

// Devuelve la configuración del rol actual sin inicializar hardware.
static inline tecla_configuracion_modo_t tecla_configuracion_modo_actual(void){
    return (tecla_configuracion_modo_t){
        .modo = TECLA_MODO_ACTUAL,
        .ruta_telemetria = TECLA_RUTA_DIRECTA_API,
        .leer_sensores_propios = true,
        .enviar_telemetria_propia = true,
        .recibir_telemetria_de_otros_nodos = false
    };
}

#endif
