#include "configuracion_remota.h"


bool tecla_configuracion_remota_validar(
    const tecla_comando_configuracion_t *comando,
    uint32_t id_nodo_local,
    uint8_t mascara_sensores_disponibles,
    uint8_t mascara_actuadores_disponibles)
{
    return tecla_configuracion_es_valida(comando, id_nodo_local,
                                         mascara_sensores_disponibles,
                                         mascara_actuadores_disponibles) != 0U;
}

bool tecla_configuracion_remota_aplicar(
    tecla_configuracion_nodo_t *actual,
    const tecla_comando_configuracion_t *comando,
    uint32_t id_nodo_local,
    uint8_t mascara_sensores_disponibles,
    uint8_t mascara_actuadores_disponibles)
{
    if (actual == 0 ||
        !tecla_configuracion_remota_validar(comando, id_nodo_local,
                                            mascara_sensores_disponibles,
                                            mascara_actuadores_disponibles)) {
        return false;
    }

    const tecla_configuracion_nodo_t nueva = {
        .intervalo_lectura_s = comando->intervalo_lectura_s,
        .mascara_sensores_habilitados = comando->mascara_sensores_habilitados,
        .mascara_actuadores_habilitados = comando->mascara_actuadores_habilitados,
        .estado_operativo = comando->estado_operativo,
        .destinos_lectura = comando->destinos_lectura,
    };
    *actual = nueva;
    return true;
}

