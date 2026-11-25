idf_component_register(
    SRCS "app/app_main.c" "queue/command_queue.c" "diagnostics/rssi_monitor.c" "diagnostics/reading_average.c" "network/gateway_api_url.c" "network/web_server/local_server.c" "../../protocol/v3/tecla_wire_v3.c" "../../protocol/v3/gateway_dispatch.c"
    INCLUDE_DIRS "." "../../comun/protocolo" "../../protocol/v3" "config" "queue" "diagnostics" "network" "network/web_server"
    REQUIRES esp_wifi esp_event esp_netif esp_http_client esp_http_server esp_timer json nvs_flash
)
