#ifndef TECLA_CICLO_NODO_H
#define TECLA_CICLO_NODO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECLA_CICLO_BOOT_ID_BYTES 16U
#define TECLA_CICLO_MAX_FRAME_BYTES 250U

// Modos administrativos que determinan si el nodo toma muestras.
typedef enum {
    TECLA_CICLO_MODO_OPERATIVO = 1,
    TECLA_CICLO_MODO_INACTIVO = 2,
    TECLA_CICLO_MODO_MANTENIMIENTO = 3
} tecla_ciclo_modo_t;

// Estados por los que avanza una ejecución del ciclo.
typedef enum {
    TECLA_CICLO_IDLE = 0,
    TECLA_CICLO_RENDEZVOUS,
    TECLA_CICLO_MEASURE,
    TECLA_CICLO_WAIT_ACK,
    TECLA_CICLO_RETRY_WAIT,
    TECLA_CICLO_SLEEP_DECISION,
    TECLA_CICLO_SLEEPING,
    TECLA_CICLO_AWAKE_WAIT
} tecla_ciclo_estado_t;

// Eventos que la aplicación comunica al controlador puro.
typedef enum {
    TECLA_CICLO_EVENT_WAKE = 1,
    TECLA_CICLO_EVENT_MANAGEMENT_DONE,
    TECLA_CICLO_EVENT_MANAGEMENT_TIMEOUT,
    TECLA_CICLO_EVENT_SAMPLE_READY,
    TECLA_CICLO_EVENT_SAMPLE_FAILED,
    TECLA_CICLO_EVENT_TX_CALLBACK_OK,
    TECLA_CICLO_EVENT_TX_CALLBACK_FAILED,
    TECLA_CICLO_EVENT_APP_ACK,
    TECLA_CICLO_EVENT_APP_NACK,
    TECLA_CICLO_EVENT_ACK_TIMEOUT,
    TECLA_CICLO_EVENT_RETRY_DELAY_DONE,
    TECLA_CICLO_EVENT_SLEEP_AUTHORIZED,
    TECLA_CICLO_EVENT_SLEEP_DENIED,
    TECLA_CICLO_EVENT_NEXT_MANAGEMENT_DUE
} tecla_ciclo_evento_t;

// Acciones que la aplicación debe ejecutar al recibir una salida.
typedef enum {
    TECLA_CICLO_ACCION_NONE = 0,
    TECLA_CICLO_ACCION_OPEN_MANAGEMENT,
    TECLA_CICLO_ACCION_READ_SENSORS,
    TECLA_CICLO_ACCION_SEND_FRAME,
    TECLA_CICLO_ACCION_WAIT_RETRY_BACKOFF,
    TECLA_CICLO_ACCION_ENTER_DEEP_SLEEP,
    TECLA_CICLO_ACCION_ARM_BOUNDED_MANAGEMENT_WAIT
} tecla_ciclo_accion_t;

// Conserva el estado y los datos de una transmisión durante el ciclo.
typedef struct {
    tecla_ciclo_estado_t estado;
    tecla_ciclo_modo_t modo;
    uint8_t reintentos_usados;
    uint8_t boot_id_activo[TECLA_CICLO_BOOT_ID_BYTES];
    uint32_t seq_activo;
    uint16_t frame_len;
    uint8_t frame_bytes[TECLA_CICLO_MAX_FRAME_BYTES];
} tecla_ciclo_contexto_t;

// Entrada de un evento con los datos necesarios para correlacionarlo.
typedef struct {
    tecla_ciclo_evento_t evento;
    bool muestreo_debido;
    const uint8_t *boot_id;
    uint32_t seq;
    uint16_t intento_radio;
    const uint8_t *frame_bytes;
    size_t frame_len;
} tecla_ciclo_entrada_t;

// Resultado del evento: nuevo estado, acción y datos de reintento.
typedef struct {
    tecla_ciclo_estado_t estado;
    tecla_ciclo_accion_t accion;
    uint8_t reintentos_usados;
    uint16_t intento_radio_siguiente;
    bool evento_ignorado;
} tecla_ciclo_salida_t;

// Inicializa el contexto en reposo con un modo válido.
bool tecla_ciclo_iniciar(tecla_ciclo_contexto_t *contexto, tecla_ciclo_modo_t modo_inicial);
// Cambia el modo solo mientras el ciclo aún permite reconfiguración.
bool tecla_ciclo_definir_modo(tecla_ciclo_contexto_t *contexto,tecla_ciclo_modo_t modo);
// Procesa un evento y devuelve la acción que debe ejecutar la aplicación.
tecla_ciclo_salida_t tecla_ciclo_procesar( tecla_ciclo_contexto_t *contexto, const tecla_ciclo_entrada_t *entrada, uint8_t max_reintentos);

#endif
