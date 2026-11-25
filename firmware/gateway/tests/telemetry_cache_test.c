#include "../src/diagnostics/telemetry_cache.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static tecla_gateway_local_reading_t reading_at(uint64_t time_us, uint8_t slot)
{
    tecla_gateway_local_reading_t r = {0};
    r.firmware_node_id = 6U;
    r.boot_id[0] = 0xA6U;
    r.seq = slot;
    r.captured_monotonic_us = time_us;
    r.node_mode = TECLA_CACHE_NODE_OPERATIVE;
    r.sensor_state = TECLA_CACHE_SENSOR_MAINTENANCE;
    r.slot_code = slot;
    r.measurement_code = 1U;
    r.sensor_health = 1U;
    r.value = 22.5f;
    r.manifest_revision[0] = 0x11U;
    r.assignment_revision[0] = 0x22U;
    return r;
}

int main(void)
{
    tecla_gateway_telemetry_cache_t cache;
    assert(tecla_gateway_telemetry_cache_init(&cache, 1000U) == TECLA_CACHE_OK);
    assert(tecla_gateway_telemetry_cache_init(&cache, 0U) == TECLA_CACHE_INVALID_ARGUMENT);
    assert(tecla_gateway_telemetry_cache_init(&cache, 1000U) == TECLA_CACHE_OK);

    tecla_gateway_local_reading_t r = reading_at(100U, 1U);
    assert(tecla_gateway_telemetry_cache_store_local_only(&cache, &r, 100U) == TECLA_CACHE_OK);
    tecla_gateway_local_reading_t output[TECLA_GATEWAY_TELEMETRY_CACHE_CAPACITY];
    uint8_t count = 0U;
    assert(tecla_gateway_telemetry_cache_snapshot(&cache, 1099U, output,
        TECLA_GATEWAY_TELEMETRY_CACHE_CAPACITY, &count) == TECLA_CACHE_OK);
    assert(count == 1U && output[0].slot_code == 1U && output[0].value == 22.5f);
    assert(output[0].manifest_revision[0] == 0x11U &&
           output[0].assignment_revision[0] == 0x22U);

    r.sensor_state = TECLA_CACHE_SENSOR_OPERATIVE;
    assert(tecla_gateway_telemetry_cache_store_local_only(&cache, &r, 1099U) ==
           TECLA_CACHE_FILTERED);
    r.sensor_state = TECLA_CACHE_SENSOR_MAINTENANCE;
    r.node_mode = TECLA_CACHE_NODE_MAINTENANCE;
    assert(tecla_gateway_telemetry_cache_store_local_only(&cache, &r, 1099U) ==
           TECLA_CACHE_FILTERED);
    r.node_mode = TECLA_CACHE_NODE_INACTIVE;
    assert(tecla_gateway_telemetry_cache_store_local_only(&cache, &r, 1099U) ==
           TECLA_CACHE_FILTERED);
    assert(cache.filtered_count == 3U && cache.count == 1U);

    r = reading_at(1100U, 2U);
    r.value = NAN;
    assert(tecla_gateway_telemetry_cache_store_local_only(&cache, &r, 1100U) ==
           TECLA_CACHE_INVALID_READING);
    r.value = 23.0f;
    r.captured_monotonic_us = 1101U;
    assert(tecla_gateway_telemetry_cache_store_local_only(&cache, &r, 1100U) ==
           TECLA_CACHE_INVALID_READING);
    r.captured_monotonic_us = 50U;
    assert(tecla_gateway_telemetry_cache_store_local_only(&cache, &r, 1100U) ==
           TECLA_CACHE_EXPIRED);

    /* Expiry is inclusive at the configured age and frees capacity. */
    assert(tecla_gateway_telemetry_cache_snapshot(&cache, 1100U, output,
        TECLA_GATEWAY_TELEMETRY_CACHE_CAPACITY, &count) == TECLA_CACHE_OK);
    assert(count == 0U);

    for (uint8_t i = 1U; i <= TECLA_GATEWAY_TELEMETRY_CACHE_CAPACITY; ++i) {
        r = reading_at(2000U, i);
        assert(tecla_gateway_telemetry_cache_store_local_only(&cache, &r, 2000U) ==
               TECLA_CACHE_OK);
    }
    r = reading_at(2000U, 1U);
    assert(tecla_gateway_telemetry_cache_store_local_only(&cache, &r, 2000U) ==
           TECLA_CACHE_FULL);
    assert(cache.count == TECLA_GATEWAY_TELEMETRY_CACHE_CAPACITY &&
           cache.overflow_count == 1U);
    count = 0U;
    assert(tecla_gateway_telemetry_cache_snapshot(&cache, 2000U, output, 1U,
        &count) == TECLA_CACHE_BUFFER_TOO_SMALL);
    assert(count == TECLA_GATEWAY_TELEMETRY_CACHE_CAPACITY);
    assert(tecla_gateway_telemetry_cache_snapshot(&cache, 3000U, output,
        TECLA_GATEWAY_TELEMETRY_CACHE_CAPACITY, &count) == TECLA_CACHE_OK);
    assert(count == 0U);

    puts("PASS local_maintenance_cache_filter_capacity_ttl_overflow");
    return 0;
}
