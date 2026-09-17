#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nvs_flash.h"
#include "esp_log.h"

#include "sensores.h"
#include "espnow_nodo.h"
#include "packet.h"

static const char *TAG = "NODO_MAIN";

void app_main(void){
    // NVS
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    // Sensores
    ESP_ERROR_CHECK( sensores_init() );
    // ESP-NOW
    ESP_ERROR_CHECK( espnow_nodo_init());

    uint32_t sequence = 1;
    while (1){
        node_packet_t packet = {0};
        packet.sequence = sequence++;

        // Leer sensores
        sensores_read(&packet);
        ESP_LOGI( TAG,"Paquete #%lu | Temp: %.1f C | Hum: %.1f %% | "
            "VPD: %.2f kPa | ADC: %d | %.2f V",
            packet.sequence,packet.temperature,packet.humidity,
            packet.vpd, packet.adc_raw,packet.voltage );


        // Enviar
        ret = espnow_nodo_send(&packet);
        if (ret != ESP_OK) {
            ESP_LOGE( TAG,"Error enviando: %s",esp_err_to_name(ret));
        }
        vTaskDelay( pdMS_TO_TICKS(3000));
    }
}