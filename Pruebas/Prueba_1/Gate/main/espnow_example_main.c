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

#define ESPNOW_CHANNEL 1

static const char *TAG = "ESP_NOW_GATEWAY";

// MAC conocida del nodo negro
static const uint8_t node_mac[ESP_NOW_ETH_ALEN] = {
    0xD4, 0xE9, 0xF4, 0x66, 0xB3, 0xC4
};

// Debe ser exactamente igual a la estructura del nodo
typedef struct {
    uint32_t sequence;
} node_packet_t;



// CALLBACK DE RECEPCIÓN
static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int data_len){
    if (recv_info == NULL || data == NULL) {
        ESP_LOGE(TAG, "Datos de recepción inválidos");
        return;
    }

    // Mostrar quién envió el paquete
    ESP_LOGI(TAG,  "Paquete recibido desde " MACSTR,MAC2STR(recv_info->src_addr));

    // Verificar que realmente provenga de nuestro nodo conocido
    if (memcmp(recv_info->src_addr,  node_mac,ESP_NOW_ETH_ALEN) != 0) {
        ESP_LOGW(TAG, "Paquete recibido de un dispositivo desconocido");
        return;
    }


    // Verificar que el tamaño coincida con nuestra estructura
    if (data_len != sizeof(node_packet_t)) {
        ESP_LOGW(TAG,"Tamaño de paquete incorrecto: %d bytes", data_len);
        return;
    }


    // Copiar los datos recibidos
    node_packet_t paquete;
    memcpy(&paquete, data, sizeof(paquete));
    ESP_LOGI(TAG, "Paquete #%" PRIu32 " recibido correctamente", paquete.sequence);
}


// INICIALIZACIÓN WIFI
static void wifi_init(void){
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK( esp_event_loop_create_default() );
    wifi_init_config_t wifi_config =  WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Debe ser el mismo canal utilizado por el nodo
    ESP_ERROR_CHECK( esp_wifi_set_channel( ESPNOW_CHANNEL,WIFI_SECOND_CHAN_NONE) );
    ESP_LOGI(TAG, "Wi-Fi iniciado en modo STA");
    ESP_LOGI(TAG, "Canal ESP-NOW: %d",ESPNOW_CHANNEL);
}


// INICIALIZACIÓN ESP-NOW
static void espnow_init(void){
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK( esp_now_register_recv_cb( espnow_recv_cb ));
    ESP_LOGI(TAG, "ESP-NOW inicializado");
    ESP_LOGI(TAG,"Esperando datos del nodo...");
}


// MAIN
void app_main(void){
    // 1. Inicializar NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Inicializar Wi-Fi
    wifi_init();
    // 3. Inicializar ESP-NOW
    espnow_init();
}