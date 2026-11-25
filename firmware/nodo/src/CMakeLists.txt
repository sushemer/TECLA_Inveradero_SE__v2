set(tecla_node_sources
    "app/app_main.c" "sensors/lectura_sensores.c" "sensors/sensor_validation.c"
    "power/battery_policy.c" "sensors/veml7700.c" "configuracion/configuracion_remota.c"
    "storage/configuracion_nvs.c" "storage/node_identity.c" "storage/node_identity_nvs.c"
    "../../comun/operacion/ciclo_nodo.c" "../../protocol/v3/tecla_wire_v3.c"
)
set(tecla_node_requires dht driver nvs_flash)
if(CONFIG_TECLA_NODE_IDENTITY_CONSOLE)
    list(APPEND tecla_node_sources "storage/node_identity_cli.c" "storage/node_identity_console.c")
    list(APPEND tecla_node_requires console)
endif()
if(CONFIG_TECLA_NODE_WIRE_V3_RADIO)
    list(APPEND tecla_node_sources "radio/node_wire_v3_espnow.c")
    list(APPEND tecla_node_requires esp_event esp_netif esp_timer esp_wifi)
endif()

idf_component_register(
    SRCS ${tecla_node_sources}
    INCLUDE_DIRS "." "../../comun/protocolo" "../../comun/operacion" "../../protocol/v3" "sensors" "configuracion" "power" "radio"
    REQUIRES ${tecla_node_requires}
)
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    TECLA_ID_NODO=${TECLA_NODO_ID}
    TECLA_SENSOR_PROFILE_ID=${TECLA_SENSOR_PROFILE_ID}
)
