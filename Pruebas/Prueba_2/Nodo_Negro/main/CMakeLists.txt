idf_component_register(
    SRCS
        "nodo_main.c"
        "sensores.c"
        "espnow_nodo.c"

    PRIV_REQUIRES
        esp_wifi
        nvs_flash
        esp_event
        esp_netif
        esp_adc
        esp_driver_gpio
        esp_timer

    INCLUDE_DIRS "."
)