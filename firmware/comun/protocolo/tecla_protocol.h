#ifndef TECLA_PROTOCOLO_H
#define TECLA_PROTOCOLO_H

#include <stdint.h>

#define TECLA_VERSION_PROTOCOLO 2U
#define TECLA_MAX_LECTURAS 8U
#define TECLA_MAX_ACTUADORES 4U
#define TECLA_MAX_CAPACIDADES 8U

// Tipos de mensaje que pueden viajar en una trama del protocolo.
typedef enum {
    TECLA_TRAMA_TELEMETRIA = 1,
    TECLA_TRAMA_COMANDO_ACTUADOR = 2,
    TECLA_TRAMA_COMANDO_CONFIGURACION = 3,
    TECLA_TRAMA_CONFIRMACION = 4,
    TECLA_TRAMA_CAPACIDADES = 5
} tecla_tipo_trama_t;

// Estados operativos del nodo que se transmiten o configuran.
typedef enum {
    TECLA_NODO_OPERATIVO = 0,
    TECLA_NODO_EN_MANTENIMIENTO = 1
} tecla_estado_operativo_t;

// Destinos de telemetría que pueden combinarse como una máscara.
typedef enum {
    TECLA_DESTINO_NINGUNO = 0,
    TECLA_DESTINO_PAGINA = 1U << 0,
    TECLA_DESTINO_DIAGNOSTICO = 1U << 1
} tecla_destino_lectura_t;

// Los bits de capacidad y habilitación corresponden a IDs de sensor del 1 al 8.
#define TECLA_MASCARA_SENSOR(id) ((uint8_t)(1U << ((id) - 1U)))

// Clases de medición que puede transportar una lectura.
typedef enum {
    TECLA_TIPO_VALOR_TEMPERATURA = 1,
    TECLA_TIPO_VALOR_HUMEDAD = 2,
    TECLA_TIPO_VALOR_VPD = 3,
    TECLA_TIPO_VALOR_HUMEDAD_SUELO = 4,
    TECLA_TIPO_VALOR_CO2 = 5,
    TECLA_TIPO_VALOR_PAR = 6,
    TECLA_TIPO_VALOR_LUZ = 7
} tecla_tipo_valor_t;

// Resumen de estado general que informa el nodo.
typedef enum {
    TECLA_ESTADO_NODO_OPERATIVO = 1,
    TECLA_ESTADO_NODO_CON_FALLAS = 2,
    TECLA_ESTADO_NODO_BATERIA_BAJA = 3,
    TECLA_ESTADO_NODO_CONFIGURACION_PENDIENTE = 4
} tecla_estado_nodo_t;

// Estados posibles de una medición individual del sensor.
typedef enum {
    TECLA_ESTADO_SENSOR_FUNCIONANDO = 1,
    TECLA_ESTADO_SENSOR_SIN_LECTURA = 2,
    TECLA_ESTADO_SENSOR_LECTURA_INVALIDA = 3,
    TECLA_ESTADO_SENSOR_DESCONECTADO = 4,
    TECLA_ESTADO_SENSOR_ERROR = 5
} tecla_estado_sensor_t;

// Encabezado común con versión, tipo, secuencia e identidad del nodo.
typedef struct __attribute__((packed)) {
    uint8_t version_protocolo;
    uint8_t tipo;
    uint16_t tamano_carga;
    uint32_t secuencia;
    uint32_t id_nodo;
} tecla_encabezado_trama_t;

// Valor de un sensor, su tipo de dato y estado de validez.
typedef struct __attribute__((packed)) {
    uint8_t id_sensor;
    uint8_t tipo_valor;
    uint8_t estado;
    uint8_t reservado;
    float valor;
} tecla_lectura_t;

// Lote de telemetría de un nodo, con hasta ocho lecturas.
typedef struct __attribute__((packed)) {
    tecla_encabezado_trama_t encabezado;
    uint8_t estado_nodo;
    uint8_t cantidad_lecturas;
    uint8_t bateria_porcentaje;
    uint16_t intervalo_lectura_s;
    uint16_t reservado;
    tecla_lectura_t lecturas[TECLA_MAX_LECTURAS];
} tecla_telemetria_t;

// Orden para cambiar la salida de un actuador identificado.
typedef struct __attribute__((packed)) {
    tecla_encabezado_trama_t encabezado;
    uint8_t id_actuador;
    uint8_t habilitado;
    uint8_t estado_salida;
    uint8_t reservado;
} tecla_comando_actuador_t;

// Configuración remota de intervalo, sensores, actuadores y destinos.
typedef struct __attribute__((packed)) {
    tecla_encabezado_trama_t encabezado;
    uint16_t intervalo_lectura_s;
    uint8_t mascara_sensores_habilitados;
    uint8_t mascara_actuadores_habilitados;
    uint8_t estado_operativo;
    uint8_t destinos_lectura;
} tecla_comando_configuracion_t;

// Capacidades de sensores y actuadores disponibles en el nodo.
typedef struct __attribute__((packed)) {
    tecla_encabezado_trama_t encabezado;
    uint8_t cantidad_sensores;
    uint8_t mascara_actuadores_disponibles;
    uint8_t sensores[TECLA_MAX_CAPACIDADES];
} tecla_anuncio_capacidades_t;

// Rechaza configuraciones mal formadas o incompatibles con este nodo.
static inline uint8_t tecla_configuracion_es_valida(
    const tecla_comando_configuracion_t *configuracion,
    uint32_t id_nodo_local,
    uint8_t mascara_sensores_disponibles,
    uint8_t mascara_actuadores_disponibles)
{
    if (configuracion == 0 ||
        configuracion->encabezado.version_protocolo != TECLA_VERSION_PROTOCOLO ||
        configuracion->encabezado.tipo != TECLA_TRAMA_COMANDO_CONFIGURACION ||
        configuracion->encabezado.id_nodo != id_nodo_local ||
        configuracion->encabezado.tamano_carga !=
            sizeof(tecla_comando_configuracion_t) - sizeof(tecla_encabezado_trama_t)) {
        return 0U;
    }

    if ((configuracion->mascara_sensores_habilitados &
         (uint8_t)~mascara_sensores_disponibles) != 0U ||
        (configuracion->mascara_actuadores_habilitados &
         (uint8_t)~mascara_actuadores_disponibles) != 0U) {
        return 0U;
    }

    if (configuracion->estado_operativo > TECLA_NODO_EN_MANTENIMIENTO ||
        (configuracion->destinos_lectura &
         (uint8_t)~(TECLA_DESTINO_PAGINA | TECLA_DESTINO_DIAGNOSTICO)) != 0U ||
        (configuracion->intervalo_lectura_s == 0U &&
         configuracion->estado_operativo == TECLA_NODO_OPERATIVO)) {
        return 0U;
    }

    return 1U;
}

// Respuesta del nodo que confirma o rechaza una secuencia recibida.
typedef struct __attribute__((packed)) {
    tecla_encabezado_trama_t encabezado;
    uint32_t secuencia_confirmada;
    uint8_t aceptado;
    uint8_t reservado[3];
} tecla_confirmacion_t;

#endif
