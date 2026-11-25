#include "esp_log.h"

#include "modo_operacion.h"
#include "../sensors/lectura_sensores.h"
#include "../configuracion/configuracion_sensores.h"
#include "../configuracion/configuracion_remota.h"
#include "../storage/configuracion_nvs.h"
#include "../storage/node_identity_nvs.h"
#if defined(CONFIG_TECLA_NODE_IDENTITY_CONSOLE) && CONFIG_TECLA_NODE_IDENTITY_CONSOLE
#include "../storage/node_identity_console.h"
#endif
#if defined(CONFIG_TECLA_NODE_WIRE_V3_RADIO) && CONFIG_TECLA_NODE_WIRE_V3_RADIO
#include "../radio/node_wire_v3_espnow.h"
#endif

static const char *TAG = "nodo";

void app_main(void)
{
    ESP_ERROR_CHECK(tecla_configuracion_nvs_iniciar());

#if defined(CONFIG_TECLA_NODE_IDENTITY_CONSOLE) && CONFIG_TECLA_NODE_IDENTITY_CONSOLE
    const esp_err_t console_err = tecla_node_identity_console_start();
    if (console_err != ESP_OK) {
        ESP_LOGE(TAG, "No se pudo iniciar consola de provisioning local: %s",
                 esp_err_to_name(console_err));
    }
#endif

    uint32_t firmware_node_id = 0U;
    bool identidad_provisionada = false;
    const esp_err_t identidad_err =
        tecla_node_identity_nvs_load(&firmware_node_id, &identidad_provisionada);
    if (identidad_err != ESP_OK || !identidad_provisionada) {
        ESP_LOGE(TAG, "UNPROVISIONED: firmware_node_id ausente o inválido (err=%s); nodo detenido en estado seguro",
                 esp_err_to_name(identidad_err));
        return;
    }
    ESP_LOGI(TAG, "Identidad técnica firmware_node_id=%lu cargada de NVS; no es el ID v2 legacy ni un ID SQL",
             (unsigned long)firmware_node_id);

    tecla_configuracion_nodo_t configuracion_remota = {
        .intervalo_lectura_s = 600U,
        .mascara_sensores_habilitados = TECLA_MASCARA_SENSORES_DISPONIBLES,
        .mascara_actuadores_habilitados = 0U,
        .estado_operativo = TECLA_NODO_OPERATIVO,
        .destinos_lectura = TECLA_DESTINO_PAGINA | TECLA_DESTINO_DIAGNOSTICO,
    };
    bool configuracion_guardada = false;
    ESP_ERROR_CHECK(tecla_configuracion_nvs_cargar(
        &configuracion_remota, TECLA_ID_NODO,
        TECLA_MASCARA_SENSORES_DISPONIBLES, 0U, &configuracion_guardada));
    ESP_LOGI(TAG, "Configuración %s; sensores habilitados 0x%02x, intervalo %u s, estado %u",
             configuracion_guardada ? "recuperada de NVS" : "predeterminada",
             (unsigned)configuracion_remota.mascara_sensores_habilitados,
             (unsigned)configuracion_remota.intervalo_lectura_s,
             (unsigned)configuracion_remota.estado_operativo);

#if defined(CONFIG_TECLA_NODE_WIRE_V3_RADIO) && CONFIG_TECLA_NODE_WIRE_V3_RADIO
    bool radio_v3_ready = false;
    if (configuracion_remota.estado_operativo == TECLA_NODO_OPERATIVO) {
        const esp_err_t radio_err = tecla_node_wire_v3_radio_start(firmware_node_id);
        radio_v3_ready = radio_err == ESP_OK;
        if (!radio_v3_ready) {
            ESP_LOGE(TAG, "ESP-NOW v3 no disponible; se mantienen lecturas locales: %s",
                     esp_err_to_name(radio_err));
        }
    }
#endif

    tecla_anuncio_capacidades_t anuncio = {0};
    tecla_capacidades_nodo_actuales(&anuncio);
    ESP_LOGI(TAG, "ID nodo v2 legacy %lu, protocolo %u, sensores disponibles %u",
             (unsigned long)anuncio.encabezado.id_nodo,
             (unsigned)anuncio.encabezado.version_protocolo,
             (unsigned)anuncio.cantidad_sensores);
    for (uint8_t indice = 0; indice < anuncio.cantidad_sensores; indice++) {
        ESP_LOGI(TAG, "Sensor disponible ID %u", (unsigned)anuncio.sensores[indice]);
    }

    const tecla_configuracion_modo_t configuracion =
        tecla_configuracion_modo_actual();
    ESP_LOGI(TAG, "Modo %d; recepción de otros nodos: %s",
             configuracion.modo,
             configuracion.recibir_telemetria_de_otros_nodos ? "sí" : "no");

    ESP_ERROR_CHECK(tecla_inicializar_sensores());

    tecla_resultado_sensores_t resultado = {0};
    ESP_ERROR_CHECK(tecla_leer_sensores(&resultado, &configuracion_remota));
    ESP_LOGI(TAG, "Nodo inicializado; lecturas preparadas: %u", (unsigned) resultado.cantidad);
#if defined(CONFIG_TECLA_NODE_WIRE_V3_RADIO) && CONFIG_TECLA_NODE_WIRE_V3_RADIO
    if (radio_v3_ready) {
        const esp_err_t radio_err = tecla_node_wire_v3_radio_send_sample(
            &configuracion_remota, &resultado);
        if (radio_err != ESP_OK) {
            ESP_LOGE(TAG, "Muestra v3 sin ACK de aplicación confirmado: %s",
                     esp_err_to_name(radio_err));
        }
    }
#endif
}
