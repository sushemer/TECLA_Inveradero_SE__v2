#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_idf_version.h"

#include "espnow_example.h"
// CONFIGURACIÓN
#define ESPNOW_CHANNEL 1
static const char *TAG = "ESP_NOW_NODO";

// MAC Wi-Fi STA del gateway rojo
static const uint8_t gateway_mac[ESP_NOW_ETH_ALEN] = {
    0xE0, 0x8C, 0xFE, 0x5D, 0x04, 0xEC
};


// ESTRUCTURA DEL PAQUETE
// Por ahora solamente enviamos un contador.
typedef struct {
    uint32_t sequence;
} node_packet_t;


// CALLBACK DE ENVÍO
static void espnow_send_cb(const esp_now_send_info_t *tx_info,esp_now_send_status_t status){
    if (tx_info == NULL) {
        ESP_LOGE(TAG, "Información de envío inválida");
        return;
    }
    ESP_LOGI(TAG, "Envío a " MACSTR " -> %s", MAC2STR(tx_info->des_addr),  status == ESP_NOW_SEND_SUCCESS ? "EXITOSO" : "FALLIDO");
}



// INICIALIZACIÓN DE WI-FI
static void wifi_init(void){
    // Inicializa la pila de red
    ESP_ERROR_CHECK(esp_netif_init());

    // Crea el event loop por defecto
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Configuración Wi-Fi por defecto
    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&wifi_config));

    // Guardamos la configuración únicamente en RAM
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    // ESP-NOW utilizará la interfaz Wi-Fi Station
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // Inicia Wi-Fi
    ESP_ERROR_CHECK(esp_wifi_start());

    // Nodo y gateway deben utilizar el mismo canal
    ESP_ERROR_CHECK( esp_wifi_set_channel( ESPNOW_CHANNEL,WIFI_SECOND_CHAN_NONE ) );

    ESP_LOGI(TAG, "Wi-Fi iniciado en modo STA");
    ESP_LOGI(TAG, "Canal ESP-NOW: %d", ESPNOW_CHANNEL);
}


// INICIALIZACIÓN DE ESP-NOW
static void espnow_init(void){
    // Inicializar ESP-NOW
    ESP_ERROR_CHECK(esp_now_init());

    // Registrar callback para saber si el envío funcionó
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));

    // Información del gateway que será nuestro peer
    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, gateway_mac,ESP_NOW_ETH_ALEN );

    peer.channel = ESPNOW_CHANNEL;
    peer.ifidx = WIFI_IF_STA;

    // Por ahora SIN cifrado
    peer.encrypt = false;

    // Evita intentar agregarlo dos veces
    if (!esp_now_is_peer_exist(gateway_mac)) {
        ESP_ERROR_CHECK(esp_now_add_peer(&peer));
        ESP_LOGI(TAG,"Gateway agregado: " MACSTR, MAC2STR(gateway_mac));
    }

    ESP_LOGI(TAG, "ESP-NOW inicializado");
}


// APP MAIN
void app_main(void){
    // 1. Inicializar NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Inicializar Wi-Fi
    wifi_init();

    // 3. Inicializar ESP-NOW
    espnow_init();

    // 4. Enviar datos
    uint32_t contador = 1;
    ESP_LOGI(TAG, "Nodo listo para transmitir");
    while (1) {
        node_packet_t paquete = {.sequence = contador};
        ESP_LOGI(TAG,"Enviando paquete #%" PRIu32,paquete.sequence);
        ret = esp_now_send( gateway_mac, (uint8_t *)&paquete,sizeof(paquete));

        if (ret != ESP_OK) {
            ESP_LOGE(TAG,"Error al solicitar envío: %s",esp_err_to_name(ret));
        }
        contador++;

        // 2 segundos
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}