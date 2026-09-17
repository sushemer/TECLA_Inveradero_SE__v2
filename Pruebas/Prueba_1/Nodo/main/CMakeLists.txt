idf_component_register(
    SRCS "espnow_example_main.c"
    PRIV_REQUIRES
        esp_wifi
        nvs_flash
        esp_event
        esp_netif
    INCLUDE_DIRS "."
)