#include "configuracion_nvs.h"

#include <string.h>

#include "nvs.h"
#include "nvs_flash.h"

#include "tecla_protocol.h"
#include "../configuracion/configuracion_sensores.h"

#define ESPACIO_NVS "tecla_nodo"
#define CLAVE_CONFIGURACION "config_v2"

typedef struct __attribute__((packed)) {
    uint8_t version;
    uint32_t id_nodo;
    tecla_configuracion_nodo_t configuracion;
} tecla_configuracion_guardada_t;

esp_err_t tecla_configuracion_nvs_iniciar(void)
{
    /* Preserve identity/configuration: recovery that erases the partition must
     * be an explicit service operation, never an automatic boot side effect. */
    return nvs_flash_init();
}

esp_err_t tecla_configuracion_nvs_cargar(
    tecla_configuracion_nodo_t *configuracion,
    uint32_t id_nodo_local,
    uint8_t mascara_sensores_disponibles,
    uint8_t mascara_actuadores_disponibles,
    bool *encontrada)
{
    if (configuracion == NULL || encontrada == NULL) return ESP_ERR_INVALID_ARG;
    *encontrada = false;

    nvs_handle_t nvs;
    esp_err_t resultado = nvs_open(ESPACIO_NVS, NVS_READONLY, &nvs);
    if (resultado == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (resultado != ESP_OK) return resultado;

    tecla_configuracion_guardada_t guardada;
    size_t tamano = sizeof(guardada);
    resultado = nvs_get_blob(nvs, CLAVE_CONFIGURACION, &guardada, &tamano);
    nvs_close(nvs);
    if (resultado == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (resultado != ESP_OK) return resultado;
    if (tamano != sizeof(guardada) || guardada.version != TECLA_VERSION_PROTOCOLO ||
        guardada.id_nodo != id_nodo_local) return ESP_OK;

    tecla_comando_configuracion_t comando = {0};
    comando.encabezado.version_protocolo = TECLA_VERSION_PROTOCOLO;
    comando.encabezado.tipo = TECLA_TRAMA_COMANDO_CONFIGURACION;
    comando.encabezado.tamano_carga =
        (uint16_t)(sizeof(comando) - sizeof(comando.encabezado));
    comando.encabezado.id_nodo = id_nodo_local;
    comando.intervalo_lectura_s = guardada.configuracion.intervalo_lectura_s;
    comando.mascara_sensores_habilitados =
        guardada.configuracion.mascara_sensores_habilitados;
    comando.mascara_actuadores_habilitados =
        guardada.configuracion.mascara_actuadores_habilitados;
    comando.estado_operativo = guardada.configuracion.estado_operativo;
    comando.destinos_lectura = guardada.configuracion.destinos_lectura;

    if (!tecla_configuracion_es_valida(&comando, id_nodo_local,
                                       mascara_sensores_disponibles,
                                       mascara_actuadores_disponibles)) {
        return ESP_OK;
    }

    *configuracion = guardada.configuracion;
    *encontrada = true;
    return ESP_OK;
}

esp_err_t tecla_configuracion_nvs_guardar(
    const tecla_configuracion_nodo_t *configuracion)
{
    if (configuracion == NULL) return ESP_ERR_INVALID_ARG;

    tecla_comando_configuracion_t comando = {0};
    comando.encabezado.version_protocolo = TECLA_VERSION_PROTOCOLO;
    comando.encabezado.tipo = TECLA_TRAMA_COMANDO_CONFIGURACION;
    comando.encabezado.tamano_carga =
        (uint16_t)(sizeof(comando) - sizeof(comando.encabezado));
    comando.encabezado.id_nodo = TECLA_ID_NODO;
    comando.intervalo_lectura_s = configuracion->intervalo_lectura_s;
    comando.mascara_sensores_habilitados =
        configuracion->mascara_sensores_habilitados;
    comando.mascara_actuadores_habilitados =
        configuracion->mascara_actuadores_habilitados;
    comando.estado_operativo = configuracion->estado_operativo;
    comando.destinos_lectura = configuracion->destinos_lectura;
    if (!tecla_configuracion_es_valida(&comando, TECLA_ID_NODO,
                                       TECLA_MASCARA_SENSORES_DISPONIBLES, 0U)) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs;
    esp_err_t resultado = nvs_open(ESPACIO_NVS, NVS_READWRITE, &nvs);
    if (resultado != ESP_OK) return resultado;

    const tecla_configuracion_guardada_t guardada = {
        .version = TECLA_VERSION_PROTOCOLO,
        .id_nodo = TECLA_ID_NODO,
        .configuracion = *configuracion,
    };
    resultado = nvs_set_blob(nvs, CLAVE_CONFIGURACION, &guardada, sizeof(guardada));
    if (resultado == ESP_OK) resultado = nvs_commit(nvs);
    nvs_close(nvs);
    return resultado;
}
