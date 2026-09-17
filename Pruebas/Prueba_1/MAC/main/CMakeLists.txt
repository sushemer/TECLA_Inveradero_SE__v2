idf_component_register(
    SRCS "station_example_main.c"
    PRIV_REQUIRES esp_wifi nvs_flash esp_hw_support
    INCLUDE_DIRS "."
)