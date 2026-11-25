#ifndef TECLA_CONFIGURACION_SENSORES_H
#define TECLA_CONFIGURACION_SENSORES_H

#include "driver/gpio.h"
#include "tecla_protocol.h"
#include "sensor_profile.h"

/* Perfil físico compilado. Cambiar estos valores requiere actualizar firmware. */
#ifndef TECLA_ID_NODO
#define TECLA_ID_NODO 1U
#endif

#define TECLA_GPIO_DHT22 GPIO_NUM_4
#if CONFIG_IDF_TARGET_ESP32C3
#define TECLA_GPIO_I2C_SDA GPIO_NUM_5
#define TECLA_GPIO_I2C_SCL GPIO_NUM_6
#else
/* GPIO6 y GPIO7 pertenecen a la memoria flash en ESP32-WROOM; usar I2C estándar. */
#define TECLA_GPIO_I2C_SDA GPIO_NUM_21
#define TECLA_GPIO_I2C_SCL GPIO_NUM_22
#endif

#define TECLA_ID_SENSOR_DHT22 1U
#define TECLA_ID_SENSOR_SEN0193_1 2U
#define TECLA_ID_SENSOR_SEN0193_2 3U
#define TECLA_ID_SENSOR_SEN0193_3 4U
#define TECLA_ID_SENSOR_SEN0193_4 5U
#define TECLA_ID_SENSOR_LUZ 6U
#define TECLA_CANAL_SEN0193_1 0U
#define TECLA_CANAL_SEN0193_2 1U
#define TECLA_CANAL_SEN0193_3 2U
#define TECLA_CANAL_SEN0193_4 3U

#define TECLA_DIRECCION_ADS1115 0x48U
#define TECLA_DIRECCION_VEML7700 0x10U
#define TECLA_PERIODO_LECTURA_S 10U

#define TECLA_MASCARA_SENSORES_DISPONIBLES TECLA_PROFILE_SENSOR_MASK

static inline void tecla_capacidades_nodo_actuales(tecla_anuncio_capacidades_t *anuncio)
{
    if (anuncio == 0) {
        return;
    }

    *anuncio = (tecla_anuncio_capacidades_t){0};
    anuncio->encabezado.version_protocolo = TECLA_VERSION_PROTOCOLO;
    anuncio->encabezado.tipo = TECLA_TRAMA_CAPACIDADES;
    anuncio->encabezado.tamano_carga =
        (uint16_t)(sizeof(*anuncio) - sizeof(anuncio->encabezado));
    anuncio->encabezado.id_nodo = TECLA_ID_NODO;

#if TECLA_PROFILE_HAS_DHT22 && !TECLA_PROFILE_HAS_VEML7700
    anuncio->sensores[0] = TECLA_ID_SENSOR_DHT22;
    anuncio->cantidad_sensores = 1U;
#elif TECLA_PROFILE_HAS_DHT22 && TECLA_PROFILE_HAS_VEML7700
    anuncio->sensores[0] = TECLA_ID_SENSOR_DHT22;
    anuncio->sensores[1] = TECLA_ID_SENSOR_LUZ;
    anuncio->cantidad_sensores = 2U;
#elif TECLA_PROFILE_HAS_SEN0193
    anuncio->sensores[0] = TECLA_ID_SENSOR_SEN0193_1;
    anuncio->sensores[1] = TECLA_ID_SENSOR_SEN0193_2;
    anuncio->sensores[2] = TECLA_ID_SENSOR_SEN0193_3;
    anuncio->sensores[3] = TECLA_ID_SENSOR_SEN0193_4;
    anuncio->cantidad_sensores = 4U;
#endif
}

#endif
