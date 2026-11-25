#include "node_wire_v3_espnow.h"

#include <math.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_now.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "modo_operacion.h"
#include "tecla_protocol.h"
#include "tecla_wire_v3.h"
#include "ciclo_nodo.h"
#include "../configuracion/configuracion_sensores.h"

#if __has_include("node_config.local.h")
#include "node_config.local.h"
#else
#include "node_config.example.h"
#endif

#define NODE_V3_RX_QUEUE_LENGTH 4U
#define NODE_V3_MAX_RETRIES 2U
#define NODE_V3_RETRY_BACKOFF_MS 250U

typedef struct {
    uint8_t source_mac[ESP_NOW_ETH_ALEN];
    uint16_t frame_len;
    uint8_t frame[TECLA_WIRE_V3_MAX_FRAME_SIZE];
} node_v3_rx_item_t;

static const char *TAG = "node_wire_v3";
static uint32_t s_firmware_node_id;
static uint32_t s_telemetry_seq;
static uint8_t s_boot_id[16];
static uint8_t s_gateway_mac[ESP_NOW_ETH_ALEN];
static QueueHandle_t s_rx_queue;
static bool s_radio_ready;

static bool mac_is_configured_unicast(const uint8_t mac[ESP_NOW_ETH_ALEN])
{
    uint8_t aggregate = 0U;
    for (size_t i = 0U; i < ESP_NOW_ETH_ALEN; ++i) aggregate |= mac[i];
    return aggregate != 0U && (mac[0] & 1U) == 0U;
}

static void receive_callback(const esp_now_recv_info_t *info,
                             const uint8_t *data, int data_len)
{
    if (s_rx_queue == NULL || info == NULL || info->src_addr == NULL ||
        data == NULL || data_len <= 0 ||
        data_len > (int)TECLA_WIRE_V3_MAX_FRAME_SIZE) return;
    node_v3_rx_item_t item = {0};
    memcpy(item.source_mac, info->src_addr, ESP_NOW_ETH_ALEN);
    memcpy(item.frame, data, (size_t)data_len);
    item.frame_len = (uint16_t)data_len;
    (void)xQueueSend(s_rx_queue, &item, 0U);
}

static esp_err_t initialize_radio(uint32_t firmware_node_id)
{
    if (firmware_node_id == 0U) return ESP_ERR_INVALID_ARG;
    if (s_radio_ready) {
        return s_firmware_node_id == firmware_node_id ? ESP_OK : ESP_ERR_INVALID_STATE;
    }
    const uint8_t configured_gateway_mac[ESP_NOW_ETH_ALEN] = TECLA_GATEWAY_STA_MAC;
    if (!mac_is_configured_unicast(configured_gateway_mac) ||
        TECLA_ESPNOW_CHANNEL < 1U || TECLA_ESPNOW_CHANNEL > 14U) {
        ESP_LOGE(TAG, "Radio v3 requiere MAC unicast local del gateway y canal valido");
        return ESP_ERR_INVALID_ARG;
    }
    memcpy(s_gateway_mac, configured_gateway_mac, sizeof(s_gateway_mac));
    s_rx_queue = xQueueCreate(NODE_V3_RX_QUEUE_LENGTH, sizeof(node_v3_rx_item_t));
    if (s_rx_queue == NULL) return ESP_ERR_NO_MEM;

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    if (esp_netif_create_default_wifi_sta() == NULL) return ESP_ERR_NO_MEM;

    const wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    if ((err = esp_wifi_init(&wifi_config)) != ESP_OK) return err;
    if ((err = esp_wifi_set_storage(WIFI_STORAGE_RAM)) != ESP_OK) return err;
    if ((err = esp_wifi_set_mode(WIFI_MODE_STA)) != ESP_OK) return err;
    if ((err = esp_wifi_start()) != ESP_OK) return err;
    if ((err = esp_wifi_set_channel(TECLA_ESPNOW_CHANNEL,
                                    WIFI_SECOND_CHAN_NONE)) != ESP_OK) return err;
    if ((err = esp_now_init()) != ESP_OK) return err;

    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, s_gateway_mac, ESP_NOW_ETH_ALEN);
    peer.channel = TECLA_ESPNOW_CHANNEL;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    if ((err = esp_now_add_peer(&peer)) != ESP_OK) return err;
    if ((err = esp_now_register_recv_cb(receive_callback)) != ESP_OK) return err;

    esp_fill_random(s_boot_id, sizeof(s_boot_id));
    s_boot_id[6] = (uint8_t)((s_boot_id[6] & 0x0FU) | 0x40U);
    s_boot_id[8] = (uint8_t)((s_boot_id[8] & 0x3FU) | 0x80U);
    s_firmware_node_id = firmware_node_id;
    s_radio_ready = true;
    ESP_LOGW(TAG, "ESP-NOW v3 unicast activo sin CCMP (peer encrypt=false)");
    return ESP_OK;
}

esp_err_t tecla_node_wire_v3_radio_start(uint32_t firmware_node_id)
{
    return initialize_radio(firmware_node_id);
}

static uint8_t mode_to_wire(uint8_t mode)
{
    return mode == TECLA_NODO_EN_MANTENIMIENTO ? 3U : 1U;
}

static bool map_reading(uint8_t local_sensor_id, uint8_t local_measurement,
                        uint8_t *slot_code, uint8_t *measurement_code)
{
    if (local_sensor_id == TECLA_ID_SENSOR_DHT22 &&
        local_measurement == TECLA_TIPO_VALOR_TEMPERATURA) {
        *slot_code = 0x01U; *measurement_code = 1U; return true;
    }
    if (local_sensor_id == TECLA_ID_SENSOR_DHT22 &&
        local_measurement == TECLA_TIPO_VALOR_HUMEDAD) {
        *slot_code = 0x01U; *measurement_code = 2U; return true;
    }
    if (local_sensor_id == TECLA_ID_SENSOR_LUZ &&
        local_measurement == TECLA_TIPO_VALOR_LUZ) {
        *slot_code = 0x02U; *measurement_code = 7U; return true;
    }
    return false;
}

static esp_err_t encode_sample(const tecla_configuracion_nodo_t *configuration,
                               const tecla_resultado_sensores_t *sample,
                               uint8_t *frame, size_t frame_capacity,
                               size_t *frame_len, uint32_t seq)
{
    if (configuration == NULL || sample == NULL || frame == NULL ||
        frame_len == NULL || sample->cantidad > TECLA_MAX_LECTURAS) {
        return ESP_ERR_INVALID_ARG;
    }
    if (configuration->estado_operativo != TECLA_NODO_OPERATIVO) {
        return ESP_ERR_INVALID_STATE;
    }
    tecla_wire_v3_header_t header = {
        .firmware_node_id = s_firmware_node_id,
        .seq = seq,
        .type = TECLA_WIRE_V3_TELEMETRY,
    };
    memcpy(header.boot_id, s_boot_id, sizeof(header.boot_id));

    tecla_wire_v3_message_t message = {0};
    tecla_wire_v3_telemetry_t *telemetry = &message.telemetry;
    telemetry->command_config_revision_applied = 0U;
    telemetry->effective_mode = mode_to_wire(configuration->estado_operativo);
    telemetry->sample_interval_s = configuration->intervalo_lectura_s;
    telemetry->sample_uptime_ms = (uint64_t)esp_timer_get_time() / 1000U;
    telemetry->time_quality = 0U;
    telemetry->battery_percent = UINT8_MAX;
    /* Manifest hashes remain zero until the authenticated manifest bridge exists. */
    bool degraded = false;
    for (size_t i = 0U; i < sample->cantidad; ++i) {
        const tecla_lectura_t *reading = &sample->lecturas[i];
        uint8_t slot_code = 0U;
        uint8_t measurement_code = 0U;
        if (!map_reading(reading->id_sensor, reading->tipo_valor,
                         &slot_code, &measurement_code) || !isfinite(reading->valor) ||
            reading->estado < TECLA_ESTADO_SENSOR_FUNCIONANDO ||
            reading->estado > TECLA_ESTADO_SENSOR_ERROR) {
            degraded = true;
            continue;
        }
        if (reading->estado != TECLA_ESTADO_SENSOR_FUNCIONANDO) degraded = true;
        tecla_wire_v3_reading_t *out =
            &telemetry->readings[telemetry->reading_count++];
        out->slot_code = slot_code;
        out->measurement_code = measurement_code;
        out->sensor_health = reading->estado;
        out->value = reading->valor;
    }
    telemetry->node_health = telemetry->reading_count == 0U ? 5U
        : (degraded ? 2U : 1U);
    return tecla_wire_v3_encode_typed(&header, &message, frame,
        frame_capacity, frame_len) == TECLA_WIRE_V3_OK ? ESP_OK : ESP_ERR_INVALID_ARG;
}

static bool wait_for_application_ack(uint32_t seq, uint8_t boot_id[16])
{
    const int64_t deadline_us = esp_timer_get_time() +
        (int64_t)TECLA_ACK_TIMEOUT_MS * 1000;
    while (true) {
        const int64_t remaining_us = deadline_us - esp_timer_get_time();
        if (remaining_us <= 0) return false;
        const uint32_t remaining_ms = (uint32_t)((remaining_us + 999) / 1000);
        node_v3_rx_item_t item;
        if (xQueueReceive(s_rx_queue, &item,
                          pdMS_TO_TICKS(remaining_ms)) != pdTRUE) return false;
        if (memcmp(item.source_mac, s_gateway_mac, ESP_NOW_ETH_ALEN) != 0) continue;
        tecla_wire_v3_header_t header;
        tecla_wire_v3_message_t message;
        if (tecla_wire_v3_decode_typed(item.frame, item.frame_len,
                &header, &message) != TECLA_WIRE_V3_OK ||
            header.type != TECLA_WIRE_V3_TELEMETRY_ACK ||
            header.firmware_node_id != s_firmware_node_id || header.seq != seq ||
            memcmp(header.boot_id, boot_id, sizeof(header.boot_id)) != 0) continue;
        if (message.telemetry_ack.scope == 1U ||
            message.telemetry_ack.scope == 2U) return true;
        if (message.telemetry_ack.scope == 3U) return false;
    }
}

esp_err_t tecla_node_wire_v3_radio_send_sample(
    const tecla_configuracion_nodo_t *configuration,
    const tecla_resultado_sensores_t *sample)
{
    if (!s_radio_ready) return ESP_ERR_INVALID_STATE;
    if (s_telemetry_seq >= UINT32_MAX - 1U) return ESP_ERR_INVALID_STATE;
    const uint32_t seq = ++s_telemetry_seq;
    uint8_t frame[TECLA_WIRE_V3_MAX_FRAME_SIZE];
    size_t frame_len = 0U;
    esp_err_t err = encode_sample(configuration, sample, frame, sizeof(frame),
                                  &frame_len, seq);
    if (err != ESP_OK) return err;

    tecla_ciclo_contexto_t cycle;
    if (!tecla_ciclo_iniciar(&cycle, TECLA_CICLO_MODO_OPERATIVO))
        return ESP_ERR_INVALID_STATE;
    tecla_ciclo_entrada_t input = {.evento = TECLA_CICLO_EVENT_WAKE};
    tecla_ciclo_salida_t output = tecla_ciclo_procesar(&cycle, &input,
                                                        NODE_V3_MAX_RETRIES);
    if (output.accion != TECLA_CICLO_ACCION_OPEN_MANAGEMENT)
        return ESP_ERR_INVALID_STATE;
    input = (tecla_ciclo_entrada_t){.evento = TECLA_CICLO_EVENT_MANAGEMENT_DONE,
                                    .muestreo_debido = true};
    output = tecla_ciclo_procesar(&cycle, &input, NODE_V3_MAX_RETRIES);
    if (output.accion != TECLA_CICLO_ACCION_READ_SENSORS)
        return ESP_ERR_INVALID_STATE;
    input = (tecla_ciclo_entrada_t){.evento = TECLA_CICLO_EVENT_SAMPLE_READY,
        .boot_id = s_boot_id, .seq = seq, .frame_bytes = frame,
        .frame_len = frame_len};
    output = tecla_ciclo_procesar(&cycle, &input, NODE_V3_MAX_RETRIES);
    if (output.accion != TECLA_CICLO_ACCION_SEND_FRAME)
        return ESP_ERR_INVALID_STATE;

    bool acknowledged = false;
    while (!acknowledged) {
        const uint16_t attempt = output.intento_radio_siguiente;
        err = esp_now_send(s_gateway_mac, cycle.frame_bytes, cycle.frame_len);
        if (err != ESP_OK) {
            input = (tecla_ciclo_entrada_t){
                .evento = TECLA_CICLO_EVENT_TX_CALLBACK_FAILED,
                .intento_radio = attempt,
            };
            output = tecla_ciclo_procesar(&cycle, &input, NODE_V3_MAX_RETRIES);
        } else if (wait_for_application_ack(seq, s_boot_id)) {
            input = (tecla_ciclo_entrada_t){.evento = TECLA_CICLO_EVENT_APP_ACK,
                .boot_id = s_boot_id, .seq = seq};
            output = tecla_ciclo_procesar(&cycle, &input, NODE_V3_MAX_RETRIES);
            acknowledged = output.estado == TECLA_CICLO_SLEEP_DECISION;
        } else {
            input = (tecla_ciclo_entrada_t){.evento = TECLA_CICLO_EVENT_ACK_TIMEOUT,
                .intento_radio = attempt};
            output = tecla_ciclo_procesar(&cycle, &input, NODE_V3_MAX_RETRIES);
        }
        if (acknowledged) break;
        if (output.accion == TECLA_CICLO_ACCION_WAIT_RETRY_BACKOFF) {
            const uint32_t backoff_ms = NODE_V3_RETRY_BACKOFF_MS <<
                (output.reintentos_usados > 2U ? 2U : output.reintentos_usados);
            vTaskDelay(pdMS_TO_TICKS(backoff_ms));
            input = (tecla_ciclo_entrada_t){
                .evento = TECLA_CICLO_EVENT_RETRY_DELAY_DONE,
                .intento_radio = output.intento_radio_siguiente,
            };
            output = tecla_ciclo_procesar(&cycle, &input, NODE_V3_MAX_RETRIES);
            if (output.accion != TECLA_CICLO_ACCION_SEND_FRAME)
                return ESP_FAIL;
        } else if (output.estado == TECLA_CICLO_SLEEP_DECISION) {
            return ESP_ERR_TIMEOUT;
        } else {
            return ESP_FAIL;
        }
    }
    ESP_LOGI(TAG, "ACK de aplicación correlacionado fw_node=%lu seq=%lu; gateway solo confirmó buffer local volatile",
             (unsigned long)s_firmware_node_id,
             (unsigned long)seq);
    return ESP_OK;
}
