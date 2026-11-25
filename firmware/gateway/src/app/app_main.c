#include <math.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "tecla_protocol.h"
#include "command_queue.h"
#include "rssi_monitor.h"
#include "local_server.h"
#include "gateway_api_url.h"
#include "reading_average.h"
#if defined(CONFIG_TECLA_GATEWAY_WIRE_V3_RADIO) && CONFIG_TECLA_GATEWAY_WIRE_V3_RADIO
#include "tecla_wire_v3.h"
#include "gateway_dispatch.h"
#endif

#if __has_include("gateway_config.local.h")
#include "gateway_config.local.h"
#else
#include "gateway_config.example.h"
#endif

#ifndef TECLA_GATEWAY_API_BASE_URL
#define TECLA_GATEWAY_API_BASE_URL CONFIG_TECLA_GATEWAY_API_BASE_URL
#endif
#ifndef TECLA_GATEWAY_V3_BINDING_COUNT
#define TECLA_GATEWAY_V3_BINDING_COUNT 0U
#endif
#ifndef TECLA_GATEWAY_V3_BINDINGS_INITIALIZER
#define TECLA_GATEWAY_V3_BINDINGS_INITIALIZER \
    { {0U, {0U, 0U, 0U, 0U, 0U, 0U}} }
#endif

#define TECLA_GATEWAY_LEGACY_READINGS_PATH "/lecturas"
#define TECLA_GATEWAY_API_URL_CAPACITY 256U

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_CONNECT_TIMEOUT_MS 60000U
#define GATEWAY_QUEUE_LENGTH 8U
#define GATEWAY_V3_BINDING_LIMIT 8U
#define HTTP_TIMEOUT_MS 5000
#define WIFI_RETRY_DELAY_MS 500U

#ifndef TECLA_GATEWAY_LOCAL_AP_SSID
#define TECLA_GATEWAY_LOCAL_AP_SSID "TECLA-Gateway"
#endif
#ifndef TECLA_GATEWAY_LOCAL_AP_PASSWORD
#define TECLA_GATEWAY_LOCAL_AP_PASSWORD "CAMBIAR_CLAVE_AP_123"
#endif

static const char *TAG = "gateway";
static EventGroupHandle_t wifi_events;
static QueueHandle_t telemetry_queue;
static tecla_rssi_monitor_t rssi_monitor;
static SemaphoreHandle_t rssi_mutex;
#if defined(CONFIG_TECLA_GATEWAY_WIRE_V3_RADIO) && CONFIG_TECLA_GATEWAY_WIRE_V3_RADIO
static tecla_gateway_reading_average_cache_t gateway_average_cache;
#endif
static int wifi_retries;
static bool local_ap_only_mode;
static uint32_t last_sequence[5];
static bool sequence_valid[5];

#if defined(CONFIG_TECLA_GATEWAY_WIRE_V3_RADIO) && CONFIG_TECLA_GATEWAY_WIRE_V3_RADIO
typedef struct {
    uint8_t source_mac[ESP_NOW_ETH_ALEN];
    int8_t rssi_dbm;
    uint64_t captured_monotonic_us;
    uint16_t frame_len;
    uint8_t frame[TECLA_WIRE_V3_MAX_FRAME_SIZE];
} gateway_v3_rx_item_t;

static const tecla_wire_v3_binding_t gateway_v3_bindings[] =
    TECLA_GATEWAY_V3_BINDINGS_INITIALIZER;
static tecla_wire_v3_gateway_dispatch_t gateway_v3_dispatch;
static QueueHandle_t gateway_v3_rx_queue;
static bool gateway_v3_ready;
#endif

typedef struct {
    uint8_t source_mac[ESP_NOW_ETH_ALEN];
    int8_t rssi_dbm;
    uint64_t captured_monotonic_us;
    tecla_telemetria_t telemetry;
} queued_telemetry_t;

static const uint8_t node2_mac[ESP_NOW_ETH_ALEN] = TECLA_GATEWAY_NODE2_MAC;
static const uint8_t node4_mac[ESP_NOW_ETH_ALEN] = TECLA_GATEWAY_NODE4_MAC;

static bool mac_configurada(const uint8_t mac[ESP_NOW_ETH_ALEN])
{
    for (size_t i = 0; i < ESP_NOW_ETH_ALEN; ++i) if (mac[i] != 0U) return true;
    return false;
}

static const uint8_t *mac_autorizada(uint32_t id_nodo)
{
    if (id_nodo == 2U && mac_configurada(node2_mac)) return node2_mac;
    if (id_nodo == 4U && mac_configurada(node4_mac)) return node4_mac;
    return NULL;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_data;
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        if (!local_ap_only_mode) esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(wifi_events, WIFI_CONNECTED_BIT);
        if (!local_ap_only_mode) {
            wifi_retries++;
            ESP_LOGW(TAG, "Wi-Fi STA desconectado; reintento %d", wifi_retries);
            esp_wifi_connect();
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        wifi_retries = 0;
        xEventGroupSetBits(wifi_events, WIFI_CONNECTED_BIT);
    }
}

static esp_err_t iniciar_wifi(void)
{
    local_ap_only_mode = TECLA_GATEWAY_API_BASE_URL[0] == '\0';
    wifi_events = xEventGroupCreate();
    if (wifi_events == NULL) return ESP_ERR_NO_MEM;
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    const wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_config));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               wifi_event_handler, NULL));

    wifi_config_t wifi_config = {0};
    snprintf((char *)wifi_config.sta.ssid, sizeof(wifi_config.sta.ssid), "%s",
             TECLA_GATEWAY_WIFI_SSID);
    snprintf((char *)wifi_config.sta.password, sizeof(wifi_config.sta.password), "%s",
             TECLA_GATEWAY_WIFI_PASSWORD);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config_t ap_config = {0};
    snprintf((char *)ap_config.ap.ssid, sizeof(ap_config.ap.ssid), "%s",
             TECLA_GATEWAY_LOCAL_AP_SSID);
    snprintf((char *)ap_config.ap.password, sizeof(ap_config.ap.password), "%s",
             TECLA_GATEWAY_LOCAL_AP_PASSWORD);
    ap_config.ap.ssid_len = (uint8_t)strlen((const char *)ap_config.ap.ssid);
    ap_config.ap.channel = TECLA_GATEWAY_ESPNOW_CHANNEL;
    ap_config.ap.max_connection = 4U;
    ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    if (strlen(TECLA_GATEWAY_LOCAL_AP_PASSWORD) < 8U) return ESP_ERR_INVALID_ARG;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    uint32_t local_node_ids[2U + GATEWAY_V3_BINDING_LIMIT] = {0U};
    uint8_t local_node_count = 0U;
#if defined(CONFIG_TECLA_GATEWAY_WIRE_V3_RADIO) && CONFIG_TECLA_GATEWAY_WIRE_V3_RADIO
    if (TECLA_GATEWAY_V3_BINDING_COUNT <= 6U) {
        local_node_ids[0] = 2U;
        local_node_ids[1] = 4U;
        local_node_count = 2U;
    }
    for (size_t i = 0U; i < TECLA_GATEWAY_V3_BINDING_COUNT &&
         i < TECLA_WIRE_V3_MAX_BINDINGS; ++i) {
        bool duplicate = false;
        for (uint8_t j = 0U; j < local_node_count; ++j) {
            if (local_node_ids[j] == gateway_v3_bindings[i].firmware_node_id) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate && gateway_v3_bindings[i].firmware_node_id != 0U) {
            local_node_ids[local_node_count++] =
                gateway_v3_bindings[i].firmware_node_id;
        }
    }
#else
    local_node_ids[0] = 2U;
    local_node_ids[1] = 4U;
    local_node_count = 2U;
#endif
    ESP_ERROR_CHECK(tecla_gateway_local_server_start(&rssi_monitor, rssi_mutex,
#if defined(CONFIG_TECLA_GATEWAY_WIRE_V3_RADIO) && CONFIG_TECLA_GATEWAY_WIRE_V3_RADIO
        &gateway_average_cache,
#else
        NULL,
#endif
        local_node_ids, local_node_count));
    ESP_LOGI(TAG, "Página de diagnóstico local disponible en http://192.168.4.1/");

    uint8_t channel = 0U;
    wifi_second_chan_t second_channel = WIFI_SECOND_CHAN_NONE;
    if (local_ap_only_mode) {
        ESP_LOGI(TAG, "API base vacía; gateway en modo local/offline, sin conexión STA ni POST central");
        ESP_ERROR_CHECK(esp_wifi_get_channel(&channel, &second_channel));
        if (channel != TECLA_GATEWAY_ESPNOW_CHANNEL) return ESP_ERR_INVALID_STATE;
        return ESP_OK;
    }
    const EventBits_t bits = xEventGroupWaitBits(wifi_events, WIFI_CONNECTED_BIT,
        pdFALSE, pdTRUE, pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS));
    if ((bits & WIFI_CONNECTED_BIT) == 0U) {
        ESP_LOGW(TAG, "STA sin conexión en timeout; se bloquea reconexión automática para proteger el canal ESP-NOW %u",
                 (unsigned)TECLA_GATEWAY_ESPNOW_CHANNEL);
        local_ap_only_mode = true;
        ESP_ERROR_CHECK(esp_wifi_stop());
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_ERROR_CHECK(esp_wifi_get_channel(&channel, &second_channel));
        if (channel != TECLA_GATEWAY_ESPNOW_CHANNEL) return ESP_ERR_INVALID_STATE;
        return ESP_OK;
    }

    ESP_ERROR_CHECK(esp_wifi_get_channel(&channel, &second_channel));
    if (channel != TECLA_GATEWAY_ESPNOW_CHANNEL) {
        ESP_LOGW(TAG, "Router en canal %u; ESP-NOW requiere %u. Se conserva AP+STA local y ESP-NOW; se pausa reconexión STA/backend",
                 (unsigned)channel, (unsigned)TECLA_GATEWAY_ESPNOW_CHANNEL);
        local_ap_only_mode = true;
        (void)esp_wifi_disconnect();
        ESP_ERROR_CHECK(esp_wifi_stop());
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
        ESP_ERROR_CHECK(esp_wifi_start());
        xEventGroupClearBits(wifi_events, WIFI_CONNECTED_BIT);
        ESP_ERROR_CHECK(esp_wifi_get_channel(&channel, &second_channel));
        if (channel != TECLA_GATEWAY_ESPNOW_CHANNEL) return ESP_ERR_INVALID_STATE;
        return ESP_OK;
    }
    ESP_LOGI(TAG, "Wi-Fi conectado; canal ESP-NOW %u", (unsigned)channel);
    return ESP_OK;
}

static bool trama_valida(const tecla_telemetria_t *packet, int len)
{
    if (packet == NULL || len != sizeof(*packet) ||
        packet->encabezado.version_protocolo != TECLA_VERSION_PROTOCOLO ||
        packet->encabezado.tipo != TECLA_TRAMA_TELEMETRIA ||
        packet->encabezado.tamano_carga != sizeof(*packet) - sizeof(packet->encabezado) ||
        (packet->encabezado.id_nodo != 2U && packet->encabezado.id_nodo != 4U) ||
        packet->cantidad_lecturas == 0U || packet->cantidad_lecturas > TECLA_MAX_LECTURAS) {
        return false;
    }
    return true;
}

static void recibir_telemetria(const esp_now_recv_info_t *info,
                               const uint8_t *data, int len)
{
    if (info == NULL || info->src_addr == NULL || data == NULL || len <= 0) return;
#if defined(CONFIG_TECLA_GATEWAY_WIRE_V3_RADIO) && CONFIG_TECLA_GATEWAY_WIRE_V3_RADIO
    if (data[0] == TECLA_WIRE_V3_VERSION) {
        if (!gateway_v3_ready || info->rx_ctrl == NULL ||
            len > (int)TECLA_WIRE_V3_MAX_FRAME_SIZE) return;
        gateway_v3_rx_item_t item = {0};
        memcpy(item.source_mac, info->src_addr, ESP_NOW_ETH_ALEN);
        item.rssi_dbm = (int8_t)info->rx_ctrl->rssi;
        item.captured_monotonic_us = (uint64_t)esp_timer_get_time();
        item.frame_len = (uint16_t)len;
        memcpy(item.frame, data, (size_t)len);
        /* Wi-Fi callback context: enqueue only; parsing/dispatch occurs in a task. */
        (void)xQueueSend(gateway_v3_rx_queue, &item, 0U);
        return;
    }
#endif
    if (info->rx_ctrl == NULL || len != sizeof(tecla_telemetria_t)) return;

    tecla_telemetria_t packet;
    memcpy(&packet, data, sizeof(packet));
    if (!trama_valida(&packet, len)) {
        ESP_LOGW(TAG, "Trama ESP-NOW inválida; descartada");
        return;
    }
    const uint8_t *expected_mac = mac_autorizada(packet.encabezado.id_nodo);
    if (expected_mac == NULL || memcmp(info->src_addr, expected_mac, ESP_NOW_ETH_ALEN) != 0) {
        ESP_LOGW(TAG, "MAC " MACSTR " no autorizada para nodo %lu; agréguela al gateway_config.local.h",
                 MAC2STR(info->src_addr), (unsigned long)packet.encabezado.id_nodo);
        return;
    }

    queued_telemetry_t queued = {0};
    memcpy(queued.source_mac, info->src_addr, ESP_NOW_ETH_ALEN);
    queued.rssi_dbm = (int8_t)info->rx_ctrl->rssi;
    queued.captured_monotonic_us = (uint64_t)esp_timer_get_time();
    queued.telemetry = packet;
    if (xQueueSend(telemetry_queue, &queued, 0) != pdTRUE) {
        ESP_LOGW(TAG, "FIFO llena; secuencia %lu descartada sin ACK",
                 (unsigned long)packet.encabezado.secuencia);
    }
}

#if defined(CONFIG_TECLA_GATEWAY_WIRE_V3_RADIO) && CONFIG_TECLA_GATEWAY_WIRE_V3_RADIO
static bool enviar_ack_v3(void *context, const uint8_t destination_mac[6],
                          const uint8_t *frame, size_t frame_len)
{
    (void)context;
    return esp_now_send(destination_mac, frame, frame_len) == ESP_OK;
}

static void procesar_v3_gateway(void *arg)
{
    (void)arg;
    gateway_v3_rx_item_t item;
    for (;;) {
        if (xQueueReceive(gateway_v3_rx_queue, &item, portMAX_DELAY) != pdTRUE) continue;
        tecla_wire_v3_frame_view_t received;
        const bool decoded = tecla_wire_v3_decode(item.frame, item.frame_len,
                                                   &received) == TECLA_WIRE_V3_OK;
        const tecla_wire_v3_dispatch_status_t result =
            tecla_wire_v3_gateway_dispatch_frame(&gateway_v3_dispatch,
                item.source_mac, item.frame, item.frame_len,
                enviar_ack_v3, NULL);
        if (decoded && (result == TECLA_WIRE_V3_DISPATCH_ACCEPTED ||
                        result == TECLA_WIRE_V3_DISPATCH_DUPLICATE) &&
            xSemaphoreTake(rssi_mutex, portMAX_DELAY) == pdTRUE) {
            (void)tecla_rssi_monitor_record(&rssi_monitor,
                received.header.firmware_node_id, item.rssi_dbm,
                item.captured_monotonic_us);
            const tecla_gateway_average_status_t average_status =
                tecla_gateway_reading_average_record_frame(&gateway_average_cache,
                    item.frame, item.frame_len, item.captured_monotonic_us);
            xSemaphoreGive(rssi_mutex);
            if (average_status != TECLA_AVERAGE_OK &&
                average_status != TECLA_AVERAGE_DUPLICATE &&
                average_status != TECLA_AVERAGE_NO_ELIGIBLE_READINGS) {
                ESP_LOGW(TAG, "Muestra v3 no agregada (%u); RSSI y cola raw permanecen independientes",
                         (unsigned)average_status);
            }
        }
        if (result != TECLA_WIRE_V3_DISPATCH_ACCEPTED &&
            result != TECLA_WIRE_V3_DISPATCH_DUPLICATE) {
            ESP_LOGW(TAG, "Frame v3 descartado/pendiente (%u); no se confirma a central",
                     (unsigned)result);
        }
    }
}
#endif

static esp_err_t agregar_peer(const uint8_t mac[ESP_NOW_ETH_ALEN])
{
    if (!mac_configurada(mac)) return ESP_OK;
    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, mac, ESP_NOW_ETH_ALEN);
    peer.channel = TECLA_GATEWAY_ESPNOW_CHANNEL;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    esp_err_t err = esp_now_add_peer(&peer);
    return err == ESP_ERR_ESPNOW_EXIST ? ESP_OK : err;
}

static esp_err_t iniciar_espnow(void)
{
    telemetry_queue = xQueueCreate(GATEWAY_QUEUE_LENGTH, sizeof(queued_telemetry_t));
    if (telemetry_queue == NULL) return ESP_ERR_NO_MEM;
    ESP_ERROR_CHECK(esp_now_init());
#if defined(CONFIG_TECLA_GATEWAY_WIRE_V3_RADIO) && CONFIG_TECLA_GATEWAY_WIRE_V3_RADIO
    if (TECLA_GATEWAY_V3_BINDING_COUNT <= TECLA_WIRE_V3_MAX_BINDINGS &&
        tecla_wire_v3_gateway_dispatch_init(&gateway_v3_dispatch,
            TECLA_GATEWAY_V3_BINDING_COUNT == 0U ? NULL : gateway_v3_bindings,
            TECLA_GATEWAY_V3_BINDING_COUNT) == TECLA_WIRE_V3_DISPATCH_ACCEPTED) {
        gateway_v3_rx_queue = xQueueCreate(GATEWAY_QUEUE_LENGTH,
                                            sizeof(gateway_v3_rx_item_t));
        bool peers_ready = gateway_v3_rx_queue != NULL;
        for (size_t i = 0U; peers_ready && i < TECLA_GATEWAY_V3_BINDING_COUNT; ++i) {
            if (!mac_configurada(gateway_v3_bindings[i].mac)) {
                peers_ready = false;
                break;
            }
            const esp_err_t peer_err = agregar_peer(gateway_v3_bindings[i].mac);
            if (peer_err != ESP_OK) peers_ready = false;
        }
        if (peers_ready && xTaskCreate(procesar_v3_gateway, "gateway_v3_rx", 4096,
                                       NULL, 6, NULL) == pdPASS) {
            gateway_v3_ready = true;
            ESP_LOGW(TAG, "Receptor v3 experimental activo: bindings locales=%u; ACK solo por caché RAM, sin CCMP/API v1",
                     (unsigned)TECLA_GATEWAY_V3_BINDING_COUNT);
        } else {
            ESP_LOGE(TAG, "Receptor v3 no disponible; la ruta legacy permanece activa");
        }
    } else {
        ESP_LOGE(TAG, "Bindings v3 locales inválidos; se conserva ruta legacy");
    }
#endif
    ESP_ERROR_CHECK(esp_now_register_recv_cb(recibir_telemetria));
    ESP_ERROR_CHECK(agregar_peer(node2_mac));
    ESP_ERROR_CHECK(agregar_peer(node4_mac));
    if (!mac_configurada(node2_mac) || !mac_configurada(node4_mac)) {
        ESP_LOGW(TAG, "Falta configurar la MAC STA de uno o ambos nodos en gateway_config.local.h");
    }
    uint8_t mac_local[ESP_NOW_ETH_ALEN] = {0};
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac_local));
    ESP_LOGI(TAG, "MAC STA del gateway: " MACSTR, MAC2STR(mac_local));
    return ESP_OK;
}

static uint32_t tipo_dato_api(uint8_t tipo_valor)
{
    switch (tipo_valor) {
    case TECLA_TIPO_VALOR_TEMPERATURA: return TECLA_API_TIPO_TEMPERATURA;
    case TECLA_TIPO_VALOR_HUMEDAD: return TECLA_API_TIPO_HUMEDAD;
    case TECLA_TIPO_VALOR_VPD: return TECLA_API_TIPO_VPD;
    case TECLA_TIPO_VALOR_HUMEDAD_SUELO: return TECLA_API_TIPO_HUMEDAD_SUELO;
    case TECLA_TIPO_VALOR_CO2: return TECLA_API_TIPO_CO2;
    case TECLA_TIPO_VALOR_LUZ: return TECLA_API_TIPO_LUZ;
    default: return 0U;
    }
}

static esp_err_t enviar_backend(const tecla_telemetria_t *packet)
{
    char backend_url[TECLA_GATEWAY_API_URL_CAPACITY];
    const tecla_gateway_url_result_t url_result = tecla_gateway_api_url_build(
        TECLA_GATEWAY_API_BASE_URL, TECLA_GATEWAY_LEGACY_READINGS_PATH,
        backend_url, sizeof(backend_url));
    if (url_result != TECLA_GATEWAY_URL_OK) {
        return url_result == TECLA_GATEWAY_URL_DISABLED
            ? ESP_ERR_INVALID_STATE : ESP_ERR_INVALID_ARG;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *readings = cJSON_CreateArray();
    if (root == NULL || readings == NULL) {
        cJSON_Delete(root);
        cJSON_Delete(readings);
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddNumberToObject(root, "dispositivo_id", packet->encabezado.id_nodo);
    cJSON_AddItemToObject(root, "lecturas", readings);
    for (uint8_t i = 0; i < packet->cantidad_lecturas; ++i) {
        const tecla_lectura_t *reading = &packet->lecturas[i];
        const uint32_t tipo_api = tipo_dato_api(reading->tipo_valor);
        if (reading->estado != TECLA_ESTADO_SENSOR_FUNCIONANDO ||
            !isfinite(reading->valor) || tipo_api == 0U) continue;
        cJSON *item = cJSON_CreateObject();
        if (item == NULL) {
            cJSON_Delete(root);
            return ESP_ERR_NO_MEM;
        }
        cJSON_AddNumberToObject(item, "tipo_dato_id", tipo_api);
        cJSON_AddNumberToObject(item, "dato", reading->valor);
        cJSON_AddItemToArray(readings, item);
    }
    if (cJSON_GetArraySize(readings) == 0) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_SIZE;
    }

    char *body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (body == NULL) return ESP_ERR_NO_MEM;
    const esp_http_client_config_t config = {
        .url = backend_url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = HTTP_TIMEOUT_MS,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        cJSON_free(body);
        return ESP_FAIL;
    }
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body, (int)strlen(body));
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        const int status = esp_http_client_get_status_code(client);
        if (status < 200 || status >= 300) {
            ESP_LOGW(TAG, "Backend respondió HTTP %d", status);
            err = ESP_FAIL;
        }
    }
    esp_http_client_cleanup(client);
    cJSON_free(body);
    return err;
}

static esp_err_t enviar_ack(const queued_telemetry_t *queued, bool aceptado)
{
    tecla_confirmacion_t ack = {0};
    ack.encabezado.version_protocolo = TECLA_VERSION_PROTOCOLO;
    ack.encabezado.tipo = TECLA_TRAMA_CONFIRMACION;
    ack.encabezado.tamano_carga = (uint16_t)(sizeof(ack) - sizeof(ack.encabezado));
    ack.encabezado.secuencia = queued->telemetry.encabezado.secuencia;
    ack.encabezado.id_nodo = queued->telemetry.encabezado.id_nodo;
    ack.secuencia_confirmada = queued->telemetry.encabezado.secuencia;
    ack.aceptado = aceptado ? 1U : 0U;
    return esp_now_send(queued->source_mac, (const uint8_t *)&ack, sizeof(ack));
}

static void procesar_cola(void *arg)
{
    (void)arg;
    queued_telemetry_t queued;
    for (;;) {
        if (xQueueReceive(telemetry_queue, &queued, portMAX_DELAY) != pdTRUE) continue;
        const uint32_t id = queued.telemetry.encabezado.id_nodo;
        const uint32_t sequence = queued.telemetry.encabezado.secuencia;
        tecla_rssi_snapshot_t rssi = {0};
        if (xSemaphoreTake(rssi_mutex, portMAX_DELAY) == pdTRUE) {
            (void)tecla_rssi_monitor_record(&rssi_monitor, id, queued.rssi_dbm,
                                            queued.captured_monotonic_us);
            const bool have_rssi = tecla_rssi_monitor_snapshot(&rssi_monitor, id,
                (uint64_t)esp_timer_get_time(), UINT64_C(60000000), &rssi);
            xSemaphoreGive(rssi_mutex);
            if (have_rssi) {
                ESP_LOGI(TAG, "RSSI recibido por gateway nodo %lu: último %d dBm, ventana %u (min %d/prom %.1f/máx %d), edad %llu ms%s",
                         (unsigned long)id, (int)rssi.last_dbm,
                         (unsigned)rssi.sample_count, (int)rssi.min_dbm,
                         (double)rssi.average_dbm, (int)rssi.max_dbm,
                         (unsigned long long)(rssi.age_us / 1000U),
                         rssi.stale ? " obsoleto" : "");
            }
        }
        if (id < 5U && sequence_valid[id] && last_sequence[id] == sequence) {
            ESP_LOGI(TAG, "Secuencia repetida %lu del nodo %lu; ACK sin volver a insertar",
                     (unsigned long)sequence, (unsigned long)id);
            (void)enviar_ack(&queued, true);
            continue;
        }

        esp_err_t result = ESP_FAIL;
        for (unsigned attempt = 1; attempt <= 3U; ++attempt) {
            if ((xEventGroupGetBits(wifi_events) & WIFI_CONNECTED_BIT) != 0U) {
                result = enviar_backend(&queued.telemetry);
                if (result == ESP_OK) break;
            } else {
                result = ESP_ERR_INVALID_STATE;
            }
            ESP_LOGW(TAG, "POST fallido para nodo %lu, secuencia %lu (intento %u/3)",
                     (unsigned long)id, (unsigned long)sequence, attempt);
            vTaskDelay(pdMS_TO_TICKS(WIFI_RETRY_DELAY_MS));
        }
        const bool accepted = result == ESP_OK;
        if (accepted && id < 5U) {
            last_sequence[id] = sequence;
            sequence_valid[id] = true;
        }
        const esp_err_t ack_err = enviar_ack(&queued, accepted);
        ESP_LOGI(TAG, "Nodo %lu seq %lu: backend %s; ACK %s",
                 (unsigned long)id, (unsigned long)sequence,
                 accepted ? "aceptó" : "falló", esp_err_to_name(ack_err));
    }
}

void app_main(void)
{
    tecla_rssi_monitor_init(&rssi_monitor);
#if defined(CONFIG_TECLA_GATEWAY_WIRE_V3_RADIO) && CONFIG_TECLA_GATEWAY_WIRE_V3_RADIO
    tecla_gateway_reading_average_init(&gateway_average_cache);
#endif
    rssi_mutex = xSemaphoreCreateMutex();
    ESP_ERROR_CHECK(rssi_mutex == NULL ? ESP_ERR_NO_MEM : ESP_OK);
    esp_err_t nvs_resultado = nvs_flash_init();
    if (nvs_resultado == ESP_ERR_NVS_NO_FREE_PAGES || nvs_resultado == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGE(TAG, "NVS requiere recuperación (%s); no se borrará porque puede contener órdenes pendientes",
                 esp_err_to_name(nvs_resultado));
        ESP_ERROR_CHECK(nvs_resultado);
    }
    ESP_ERROR_CHECK(nvs_resultado);
    uint8_t comandos_pendientes = 0U;
    uint8_t comandos_recuperados = 0U;
    const esp_err_t command_queue_result =
        tecla_gateway_command_queue_init(&comandos_pendientes, &comandos_recuperados);
    if (command_queue_result == ESP_OK) {
        ESP_LOGI(TAG, "Cola durable de órdenes: %u pendientes; %u en vuelo recuperadas para reintento",
                 (unsigned)comandos_pendientes, (unsigned)comandos_recuperados);
    } else {
        ESP_LOGE(TAG, "Cola de órdenes no disponible (%s); se conserva NVS y el gateway seguirá solo con telemetría",
                 esp_err_to_name(command_queue_result));
    }
    ESP_ERROR_CHECK(iniciar_wifi());
    ESP_ERROR_CHECK(iniciar_espnow());
    ESP_ERROR_CHECK(xTaskCreate(procesar_cola, "procesar_telemetria", 8192,
                                NULL, 5, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_LOGI(TAG, "Gateway listo; envío central %s",
             TECLA_GATEWAY_API_BASE_URL[0] == '\0' ? "deshabilitado (offline)" : "configurado");
}
