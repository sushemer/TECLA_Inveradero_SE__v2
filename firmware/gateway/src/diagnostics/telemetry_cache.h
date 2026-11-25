#ifndef TECLA_GATEWAY_TELEMETRY_CACHE_H
#define TECLA_GATEWAY_TELEMETRY_CACHE_H

#include <stdbool.h>
#include <stdint.h>

#define TECLA_GATEWAY_TELEMETRY_CACHE_CAPACITY 16U
#define TECLA_GATEWAY_CACHE_ID_BYTES 16U
#define TECLA_GATEWAY_CACHE_REVISION_BYTES 32U

typedef enum {
    TECLA_CACHE_NODE_OPERATIVE = 1,
    TECLA_CACHE_NODE_INACTIVE = 2,
    TECLA_CACHE_NODE_MAINTENANCE = 3
} tecla_cache_node_mode_t;

typedef enum {
    TECLA_CACHE_SENSOR_OPERATIVE = 1,
    TECLA_CACHE_SENSOR_INACTIVE = 2,
    TECLA_CACHE_SENSOR_MAINTENANCE = 3,
    TECLA_CACHE_SENSOR_RETIRED = 4
} tecla_cache_sensor_state_t;

typedef struct {
    uint32_t firmware_node_id;
    uint8_t boot_id[TECLA_GATEWAY_CACHE_ID_BYTES];
    uint32_t seq;
    uint64_t captured_monotonic_us;
    uint8_t node_mode;
    uint8_t sensor_state;
    uint8_t slot_code;
    uint8_t measurement_code;
    uint8_t sensor_health;
    float value;
    uint8_t manifest_revision[TECLA_GATEWAY_CACHE_REVISION_BYTES];
    uint8_t assignment_revision[TECLA_GATEWAY_CACHE_REVISION_BYTES];
} tecla_gateway_local_reading_t;

typedef enum {
    TECLA_CACHE_OK = 0,
    TECLA_CACHE_FILTERED,
    TECLA_CACHE_INVALID_ARGUMENT,
    TECLA_CACHE_INVALID_READING,
    TECLA_CACHE_EXPIRED,
    TECLA_CACHE_FULL,
    TECLA_CACHE_BUFFER_TOO_SMALL
} tecla_gateway_cache_status_t;

typedef struct {
    tecla_gateway_local_reading_t entries[TECLA_GATEWAY_TELEMETRY_CACHE_CAPACITY];
    uint64_t retention_us;
    uint32_t filtered_count;
    uint32_t overflow_count;
    uint8_t count;
} tecla_gateway_telemetry_cache_t;

/* RAM-only and single-owner/task API; never call from the ESP-NOW callback.
 * Only an OPERATIVE node + EN_MANTENIMIENTO sensor is accepted. */
tecla_gateway_cache_status_t tecla_gateway_telemetry_cache_init(
    tecla_gateway_telemetry_cache_t *cache, uint64_t retention_us);
tecla_gateway_cache_status_t tecla_gateway_telemetry_cache_store_local_only(
    tecla_gateway_telemetry_cache_t *cache,
    const tecla_gateway_local_reading_t *reading,
    uint64_t now_monotonic_us);
tecla_gateway_cache_status_t tecla_gateway_telemetry_cache_snapshot(
    tecla_gateway_telemetry_cache_t *cache,
    uint64_t now_monotonic_us,
    tecla_gateway_local_reading_t *out,
    uint8_t out_capacity,
    uint8_t *count_out);

#endif
