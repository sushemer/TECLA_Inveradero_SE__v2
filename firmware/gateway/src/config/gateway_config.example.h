#pragma once

#ifndef TECLA_GATEWAY_API_BASE_URL
#define TECLA_GATEWAY_API_BASE_URL CONFIG_TECLA_GATEWAY_API_BASE_URL
#endif

/* Copiar como gateway_config.local.h. No guardar credenciales reales en Git. */
#define TECLA_GATEWAY_WIFI_SSID "CAMBIAR_SSID"
#define TECLA_GATEWAY_WIFI_PASSWORD "CAMBIAR_PASSWORD"
#define TECLA_GATEWAY_LOCAL_AP_SSID "TECLA-Gateway"
/* Reemplazar por una clave WPA2 propia antes de operar el AP local. */
#define TECLA_GATEWAY_LOCAL_AP_PASSWORD "CAMBIAR_CLAVE_AP_123"
#define TECLA_GATEWAY_ESPNOW_CHANNEL 1U
#define TECLA_GATEWAY_NODE2_MAC {0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U}
#define TECLA_GATEWAY_NODE4_MAC {0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U}
/* Optional v3 transport routing allowlist; set locally and explicitly.
 * firmware_node_id is the provisioned technical ID, never a SQL ID.
 * Empty defaults accept no v3 node. MAC is STA transport identity, not auth. */
#define TECLA_GATEWAY_V3_BINDING_COUNT 0U
#define TECLA_GATEWAY_V3_BINDINGS_INITIALIZER \
    { {0U, {0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U}} }
/* IDs del fixture nuevo del servidor local: CO2=1, lux=2, sustrato=3,
 * temperatura=4, humedad=5, VPD=6. */
#define TECLA_API_TIPO_CO2 1U
#define TECLA_API_TIPO_LUZ 2U
#define TECLA_API_TIPO_HUMEDAD_SUELO 3U
#define TECLA_API_TIPO_TEMPERATURA 4U
#define TECLA_API_TIPO_HUMEDAD 5U
#define TECLA_API_TIPO_VPD 6U
