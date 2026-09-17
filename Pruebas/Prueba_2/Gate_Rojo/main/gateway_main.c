#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nvs_flash.h"

#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_mac.h"

#include "packet.h"

// CONFIGURACIÓN
#define ESPNOW_CHANNEL 1
static const char *TAG = "GATEWAY";

// MAC Wi-Fi STA del nodo negro
static const uint8_t node_mac[ESP_NOW_ETH_ALEN] = {
    0xD4, 0xE9, 0xF4, 0x66, 0xB3, 0xC4
};


// CALLBACK DE ENVÍO
static void espnow_send_cb(const esp_now_send_info_t *tx_info,esp_now_send_status_t status){
    if (tx_info == NULL) {
        return;
    }

    ESP_LOGI(TAG,"ACK enviado a " MACSTR " -> %s", MAC2STR(tx_info->des_addr),
        status == ESP_NOW_SEND_SUCCESS? "EXITOSO" : "FALLIDO");
}


// CALLBACK DE RECEPCIÓN
static void espnow_recv_cb(const esp_now_recv_info_t *recv_info,const uint8_t *data,int data_len){
    if (recv_info == NULL || data == NULL) {
        ESP_LOGE( TAG,"Datos de recepcion invalidos");
        return;
    }

    // Verificar origen
    if (memcmp( recv_info->src_addr,node_mac,ESP_NOW_ETH_ALEN ) != 0) {
        ESP_LOGW(TAG,"Paquete recibido de dispositivo desconocido");
        return;
    }

    // Verificar tamaño
    if (data_len != sizeof(node_packet_t)) {
        ESP_LOGW( TAG, "Tamano incorrecto: recibido %d, esperado %d",
            data_len,(int)sizeof(node_packet_t));
        return;
    }

    // Copiar paquete
    node_packet_t paquete = {0};
    memcpy(&paquete,data,sizeof(node_packet_t));

    // MOSTRAR DATOS
    ESP_LOGI( TAG,"--------------------------------" );
    ESP_LOGI( TAG,"Paquete #%lu recibido desde " MACSTR,(unsigned long)paquete.sequence,MAC2STR(recv_info->src_addr));
    ESP_LOGI(TAG,"ADC: %d | Voltaje: %.2f V", paquete.adc_raw,paquete.voltage);

    if (paquete.dht_ok) {
        ESP_LOGI(TAG, "Temperatura: %.1f C | Humedad: %.1f %% | VPD: %.2f kPa",paquete.temperature, paquete.humidity,paquete.vpd);
    }
    else {
        ESP_LOGW( TAG,"Lectura DHT22 invalida" );
    }

    // CREAR Y ENVIAR ACK
    ack_packet_t ack = {.sequence = paquete.sequence};
    esp_err_t ret = esp_now_send( node_mac,(const uint8_t *)&ack, sizeof(ack_packet_t) );
    if (ret != ESP_OK) {
        ESP_LOGE(TAG,"Error enviando ACK #%lu: %s",(unsigned long)ack.sequence, esp_err_to_name(ret));
    }
    else {
        ESP_LOGI( TAG,"ACK #%lu solicitado",(unsigned long)ack.sequence );
    }

    ESP_LOGI(TAG,"--------------------------------");
}

// WI-FI
static esp_err_t wifi_init(void){
    esp_err_t ret;
    ret = esp_netif_init();
    if (ret != ESP_OK) {
        return ret;
    }
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK) {
        return ret;
    }

    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&wifi_config);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_wifi_set_storage( WIFI_STORAGE_RAM );
    if (ret != ESP_OK) {
        return ret;
    }
    ret = esp_wifi_set_mode( WIFI_MODE_STA );
    if (ret != ESP_OK) {
        return ret;
    }
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_wifi_set_channel(
        ESPNOW_CHANNEL,
        WIFI_SECOND_CHAN_NONE
    );
    if (ret != ESP_OK) {
        return ret;
    }

    ESP_LOGI( TAG,"Wi-Fi iniciado en modo STA");
    ESP_LOGI(TAG, "Canal ESP-NOW: %d", ESPNOW_CHANNEL);
    return ESP_OK;
}


// ESP-NOW
static esp_err_t espnow_init(void){
    esp_err_t ret;
    ret = esp_now_init();
    if (ret != ESP_OK) {
        return ret;
    }

    // Callback recepción
    ret = esp_now_register_recv_cb( espnow_recv_cb);
    if (ret != ESP_OK) {
        return ret;
    }

    // callback para verificar envío del ACK
    ret = esp_now_register_send_cb(espnow_send_cb );
    if (ret != ESP_OK) {
        return ret;
    }

    // AGREGAR NODO
    if (!esp_now_is_peer_exist(node_mac)) {
        esp_now_peer_info_t peer = {0};
        memcpy(  peer.peer_addr, node_mac, ESP_NOW_ETH_ALEN);
        peer.channel = ESPNOW_CHANNEL;
        peer.ifidx = WIFI_IF_STA;
        peer.encrypt = false;

        ret = esp_now_add_peer(&peer);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG,"Error agregando nodo como peer: %s",esp_err_to_name(ret));
            return ret;
        }
    }

    ESP_LOGI(TAG,"Nodo agregado como peer: " MACSTR,MAC2STR(node_mac));
    ESP_LOGI(TAG, "ESP-NOW inicializado");
    return ESP_OK;
}


// MAIN
void app_main(void){
    esp_err_t ret;

    // NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Wi-Fi
    ESP_ERROR_CHECK( wifi_init());
    // ESP-NOW
    ESP_ERROR_CHECK( espnow_init() );
    ESP_LOGI( TAG, "Gateway listo para recibir y responder ACK");
}