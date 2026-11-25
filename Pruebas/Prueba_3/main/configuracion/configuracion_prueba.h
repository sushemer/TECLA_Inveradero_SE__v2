#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t id_nodo;
    uint8_t perfil_nodo;
    bool cpu_activa;
    uint32_t ventana_activa_s;
    uint32_t espera_inicial_s;
    uint32_t intervalo_lecturas_s;
} prueba_nodo_config_t;

typedef struct {
    bool recepcion_activada;
    bool transmision_activada;
    uint8_t canal;
    uint32_t intervalo_transmision_ms;
} prueba_comunicacion_config_t;

typedef struct {
    bool dht22;
    bool veml7700;
    bool ads1115;
    bool sen0193[4];
    bool divisor_bateria;
    bool regulador_tps63020;
} prueba_cargas_config_t;

typedef struct {
    bool activado;
    uint32_t duracion_s;
} prueba_sueno_profundo_config_t;

typedef struct {
    prueba_nodo_config_t nodo;
    prueba_comunicacion_config_t comunicacion;
    prueba_cargas_config_t cargas;
    prueba_sueno_profundo_config_t sueno_profundo;
} configuracion_prueba_t;

const configuracion_prueba_t *configuracion_prueba_obtener(void);
void configuracion_prueba_mostrar(const configuracion_prueba_t *config);
