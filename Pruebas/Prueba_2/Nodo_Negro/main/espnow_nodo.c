#include "espnow_nodo.h"

#include <string.h>
#include <stdbool.h>

#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_mac.h"
#include "esp_timer.h"


// CONFIGURACIÓN
#define ESPNOW_CHANNEL 1
static const char *TAG = "ESPNOW_NODO";
// MAC Wi-Fi STA del gateway rojo
static const uint8_t gateway_mac[ESP_NOW_ETH_ALEN] = {
    0xE0, 0x8C, 0xFE, 0x5D, 0x04, 0xEC
};


// VARIABLES PARA MEDIR RTT
// Momento justo antes de solicitar el envío
static int64_t tiempo_envio_us = 0;
// Secuencia cuyo ACK estamos esperando
static uint32_t secuencia_pendiente = 0;


// CALLBACK DE ENVÍO
static void espnow_send_cb(const esp_now_send_info_t *tx_info,esp_now_send_status_t status){
    if (tx_info == NULL) {
        ESP_LOGE( TAG,"Informacion de envio invalida");
        return;
    }

    ESP_LOGI(TAG,"Envio a " MACSTR " -> %s", MAC2STR(tx_info->des_addr), 
        status == ESP_NOW_SEND_SUCCESS ? "EXITOSO" : "FALLIDO" );

}


// CALLBACK DE RECEPCIÓN
static void espnow_recv_cb(const esp_now_recv_info_t *recv_info,const uint8_t *data,int data_len){
    if (recv_info == NULL || data == NULL) {
        ESP_LOGE(TAG, "Datos de recepcion invalidos");
        return;
    }

    // Verificar que venga del gateway
    if (memcmp(recv_info->src_addr,gateway_mac,ESP_NOW_ETH_ALEN) != 0) {
        ESP_LOGW( TAG, "Respuesta recibida de dispositivo desconocido" );
        return;
    }


    // Verificar que sea un ACK válido
    if (data_len != sizeof(ack_packet_t)) {
        ESP_LOGW( TAG, "Tamano de ACK incorrecto: %d bytes", data_len);
        return;
    }


    ack_packet_t ack = {0};

    memcpy(&ack,data, sizeof(ack_packet_t));


    if (ack.sequence != secuencia_pendiente) {
        ESP_LOGW(TAG, "ACK incorrecto. Esperado #%lu, recibido #%lu",
            (unsigned long)secuencia_pendiente, (unsigned long)ack.sequence);
        return;
    }


    // CALCULAR RTT
    int64_t tiempo_recepcion_us = esp_timer_get_time();
    int64_t rtt_us = tiempo_recepcion_us - tiempo_envio_us;
    float rtt_ms = (float)rtt_us / 1000.0f;

    ESP_LOGI(TAG,"ACK #%lu recibido | RTT: %.3f ms",
        (unsigned long)ack.sequence,rtt_ms);
}


// INICIALIZACIÓN WI-FI
static esp_err_t wifi_init(void){
    esp_err_t ret;
    ret = esp_netif_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG,"Error inicializando esp_netif: %s",esp_err_to_name(ret));
        return ret;
    }
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error creando event loop: %s",esp_err_to_name(ret));
        return ret;
    }
    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();

    ret = esp_wifi_init(&wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE( TAG, "Error inicializando Wi-Fi: %s", esp_err_to_name(ret) );
        return ret;
    }

    ret = esp_wifi_set_storage( WIFI_STORAGE_RAM );
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_wifi_set_mode(
        WIFI_MODE_STA
    );
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE( TAG,"Error iniciando Wi-Fi: %s",esp_err_to_name(ret));
        return ret;
    }


    ret = esp_wifi_set_channel(ESPNOW_CHANNEL,WIFI_SECOND_CHAN_NONE );
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error configurando canal: %s", esp_err_to_name(ret));
        return ret;
    }


    ESP_LOGI(TAG, "Wi-Fi iniciado en modo STA");
    ESP_LOGI(TAG,"Canal ESP-NOW: %d",ESPNOW_CHANNEL);

    return ESP_OK;
}


// INICIALIZACIÓN ESP-NOW
esp_err_t espnow_nodo_init(void){
    esp_err_t ret;
    // Wi-Fi
    ret = wifi_init();
    if (ret != ESP_OK) {
        return ret;
    }

    // ESP-NOW
    ret = esp_now_init();
    if (ret != ESP_OK) {
        ESP_LOGE( TAG, "Error inicializando ESP-NOW: %s", esp_err_to_name(ret) );
        return ret;
    }

    // Callback de envío
    ret = esp_now_register_send_cb(
        espnow_send_cb
    );
    if (ret != ESP_OK) {
        return ret;
    }

    // callback para recibir ACK del gateway
    ret = esp_now_register_recv_cb(espnow_recv_cb);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error registrando callback de recepcion: %s",esp_err_to_name(ret));
        return ret;
    }


    // Agregar gateway como peer
    if (!esp_now_is_peer_exist(gateway_mac)) {
        esp_now_peer_info_t peer = {0};
        memcpy( peer.peer_addr, gateway_mac,  ESP_NOW_ETH_ALEN );
        peer.channel = ESPNOW_CHANNEL;
        peer.ifidx = WIFI_IF_STA;
        peer.encrypt = false;

        ret = esp_now_add_peer(&peer);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG,"Error agregando gateway: %s", esp_err_to_name(ret));
            return ret;
        }
    }

    ESP_LOGI( TAG, "Gateway agregado: " MACSTR, MAC2STR(gateway_mac)  );
    ESP_LOGI(TAG,"ESP-NOW inicializado correctamente");
    return ESP_OK;
}


// ENVÍO
esp_err_t espnow_nodo_send(const node_packet_t *packet){
    if (packet == NULL) {
        ESP_LOGE( TAG, "Paquete invalido" );
        return ESP_ERR_INVALID_ARG;
    }

    // INICIAR MEDICIÓN DEL RTT
    secuencia_pendiente = packet->sequence;
    tiempo_envio_us = esp_timer_get_time();

    // ENVIAR
    esp_err_t ret = esp_now_send( gateway_mac, (const uint8_t *)packet,sizeof(node_packet_t));
    if (ret != ESP_OK) {
        ESP_LOGE( TAG, "Error solicitando envio: %s", esp_err_to_name(ret) );
        return ret;
    }

    ESP_LOGI( TAG, "Paquete #%lu enviado | esperando ACK", (unsigned long)packet->sequence);
    return ESP_OK;
}