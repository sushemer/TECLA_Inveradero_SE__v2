#include "telemetry_cache.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

static bool uuid_nonzero(const uint8_t id[TECLA_GATEWAY_CACHE_ID_BYTES])
{
    uint8_t any = 0U;
    for (size_t i = 0U; i < TECLA_GATEWAY_CACHE_ID_BYTES; ++i) any |= id[i];
    return any != 0U;
}

static bool valid_reading(const tecla_gateway_local_reading_t *reading)
{
    return reading->firmware_node_id != 0U && uuid_nonzero(reading->boot_id) &&
           reading->seq != 0U && reading->seq != UINT32_MAX &&
           reading->slot_code != 0U && reading->slot_code != UINT8_MAX &&
           reading->measurement_code >= 1U && reading->measurement_code <= 7U &&
           reading->sensor_health >= 1U && reading->sensor_health <= 5U &&
           isfinite(reading->value);
}

static bool expired(const tecla_gateway_local_reading_t *reading,
                    uint64_t now_us, uint64_t retention_us)
{
    return now_us < reading->captured_monotonic_us ||
           now_us - reading->captured_monotonic_us >= retention_us;
}

static void prune_expired(tecla_gateway_telemetry_cache_t *cache, uint64_t now_us)
{
    uint8_t write_index = 0U;
    for (uint8_t read_index = 0U; read_index < cache->count; ++read_index) {
        if (expired(&cache->entries[read_index], now_us, cache->retention_us)) continue;
        if (write_index != read_index) cache->entries[write_index] = cache->entries[read_index];
        write_index++;
    }
    for (uint8_t i = write_index; i < cache->count; ++i) {
        memset(&cache->entries[i], 0, sizeof(cache->entries[i]));
    }
    cache->count = write_index;
}

tecla_gateway_cache_status_t tecla_gateway_telemetry_cache_init(
    tecla_gateway_telemetry_cache_t *cache, uint64_t retention_us)
{
    if (cache == NULL || retention_us == 0U) return TECLA_CACHE_INVALID_ARGUMENT;
    memset(cache, 0, sizeof(*cache));
    cache->retention_us = retention_us;
    return TECLA_CACHE_OK;
}

tecla_gateway_cache_status_t tecla_gateway_telemetry_cache_store_local_only(
    tecla_gateway_telemetry_cache_t *cache,
    const tecla_gateway_local_reading_t *reading,
    uint64_t now_monotonic_us)
{
    if (cache == NULL || reading == NULL || cache->retention_us == 0U)
        return TECLA_CACHE_INVALID_ARGUMENT;
    prune_expired(cache, now_monotonic_us);
    if (reading->node_mode != TECLA_CACHE_NODE_OPERATIVE ||
        reading->sensor_state != TECLA_CACHE_SENSOR_MAINTENANCE) {
        cache->filtered_count++;
        return TECLA_CACHE_FILTERED;
    }
    if (!valid_reading(reading)) return TECLA_CACHE_INVALID_READING;
    if (reading->captured_monotonic_us > now_monotonic_us)
        return TECLA_CACHE_INVALID_READING;
    if (expired(reading, now_monotonic_us, cache->retention_us))
        return TECLA_CACHE_EXPIRED;
    if (cache->count >= TECLA_GATEWAY_TELEMETRY_CACHE_CAPACITY) {
        cache->overflow_count++;
        return TECLA_CACHE_FULL;
    }
    cache->entries[cache->count++] = *reading;
    return TECLA_CACHE_OK;
}

tecla_gateway_cache_status_t tecla_gateway_telemetry_cache_snapshot(
    tecla_gateway_telemetry_cache_t *cache,
    uint64_t now_monotonic_us,
    tecla_gateway_local_reading_t *out,
    uint8_t out_capacity,
    uint8_t *count_out)
{
    if (cache == NULL || count_out == NULL || (out == NULL && out_capacity != 0U))
        return TECLA_CACHE_INVALID_ARGUMENT;
    prune_expired(cache, now_monotonic_us);
    *count_out = cache->count;
    if (out_capacity < cache->count) return TECLA_CACHE_BUFFER_TOO_SMALL;
    if (cache->count != 0U) memcpy(out, cache->entries,
                                   (size_t)cache->count * sizeof(cache->entries[0]));
    return TECLA_CACHE_OK;
}
