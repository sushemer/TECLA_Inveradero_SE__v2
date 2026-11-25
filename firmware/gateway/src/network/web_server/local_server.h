#ifndef TECLA_GATEWAY_LOCAL_SERVER_H
#define TECLA_GATEWAY_LOCAL_SERVER_H

#include <stdint.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "rssi_monitor.h"
#include "reading_average.h"

esp_err_t tecla_gateway_local_server_start(tecla_rssi_monitor_t *monitor,
                                           SemaphoreHandle_t monitor_mutex,
                                           tecla_gateway_reading_average_cache_t *average_cache,
                                           const uint32_t *firmware_node_ids,
                                           uint8_t node_count);

#endif
