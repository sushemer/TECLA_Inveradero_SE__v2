#include "prueba_espnow.h"

#include <stdint.h>
#include <string.h>

#include "esp_err.h"
#include "esp_now.h"
#include "esp_wifi.h"

static const uint8_t s_broadcast_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
static bool s_wifi_iniciado;
static bool s_espnow_iniciado;
static bool s_transmision_activada;
static uint32_t s_secuencia;

typedef struct __attribute__((packed)) {
    uint32_t marca;
    uint32_t secuencia;
} paquete_prueba_espnow_t;

/* La recepción solo mantiene el modo escucha; no procesa ni valida los datos recibidos. */
static void al_recibir(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    (void)info;
    (void)data;
    (void)len;
}

/* Inicia ESP-NOW solo cuando recepción o transmisión están seleccionados. */
void prueba_espnow_iniciar(const prueba_comunicacion_config_t *config)
{
    if (config == NULL || (!config->recepcion_activada && !config->transmision_activada)) return;

    wifi_init_config_t configuracion_wifi = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&configuracion_wifi));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    s_wifi_iniciado = true;
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_channel(config->canal, WIFI_SECOND_CHAN_NONE));

    ESP_ERROR_CHECK(esp_now_init());
    s_espnow_iniciado = true;
    ESP_ERROR_CHECK(esp_now_register_recv_cb(al_recibir));
    s_transmision_activada = config->transmision_activada;

    if (s_transmision_activada) {
        esp_now_peer_info_t peer = {0};
        memcpy(peer.peer_addr, s_broadcast_mac, sizeof(s_broadcast_mac));
        peer.channel = config->canal;
        peer.ifidx = WIFI_IF_STA;
        peer.encrypt = false;
        esp_err_t err = esp_now_add_peer(&peer);
        if (err != ESP_OK && err != ESP_ERR_ESPNOW_EXIST) ESP_ERROR_CHECK(err);
    }
}

/* Detiene radio y controlador antes de entrar en sueño profundo. */
void prueba_espnow_detener(void)
{
    if (s_espnow_iniciado) {
        ESP_ERROR_CHECK(esp_now_deinit());
        s_espnow_iniciado = false;
    }
    if (s_wifi_iniciado) {
        ESP_ERROR_CHECK(esp_wifi_stop());
        ESP_ERROR_CHECK(esp_wifi_deinit());
        s_wifi_iniciado = false;
    }
    s_transmision_activada = false;
}

/* Envía una trama broadcast de laboratorio sin ACK ni confirmación. */
esp_err_t prueba_espnow_transmitir(void)
{
    if (!s_espnow_iniciado || !s_transmision_activada) return ESP_ERR_INVALID_STATE;

    const paquete_prueba_espnow_t paquete = {
        .marca = 0x54455354u,
        .secuencia = s_secuencia++,
    };
    return esp_now_send(s_broadcast_mac, (const uint8_t *)&paquete, sizeof(paquete));
}
