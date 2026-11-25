#include "../src/diagnostics/reading_average.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static size_t make_frame(uint8_t *out, uint32_t node_id, uint8_t boot_seed,
                         uint32_t seq, uint8_t mode, uint8_t sensor_health,
                         uint8_t slot, uint8_t measurement, float value,
                         uint8_t revision_seed)
{
    tecla_wire_v3_header_t header = {0};
    header.firmware_node_id = node_id;
    header.type = TECLA_WIRE_V3_TELEMETRY;
    header.seq = seq;
    for (size_t i = 0U; i < sizeof(header.boot_id); ++i)
        header.boot_id[i] = (uint8_t)(boot_seed + i);
    header.boot_id[6] = (uint8_t)((header.boot_id[6] & 0x0fU) | 0x40U);
    header.boot_id[8] = (uint8_t)((header.boot_id[8] & 0x3fU) | 0x80U);

    tecla_wire_v3_message_t message = {0};
    message.telemetry.effective_mode = mode;
    message.telemetry.sample_interval_s = 600U;
    message.telemetry.sample_uptime_ms = (uint64_t)seq * 600000U;
    message.telemetry.time_quality = 0U;
    message.telemetry.battery_percent = UINT8_MAX;
    message.telemetry.reading_count = 1U;
    memset(message.telemetry.manifest_revision, revision_seed, 32U);
    memset(message.telemetry.assignment_revision, (uint8_t)(revision_seed + 1U), 32U);
    message.telemetry.readings[0] = (tecla_wire_v3_reading_t){
        .slot_code = slot,
        .measurement_code = measurement,
        .sensor_health = sensor_health,
        .value = value,
    };

    size_t length = 0U;
    assert(tecla_wire_v3_encode_typed(&header, &message, out,
        TECLA_WIRE_V3_MAX_FRAME_SIZE, &length) == TECLA_WIRE_V3_OK);
    return length;
}

static uint16_t snapshot(tecla_gateway_reading_average_cache_t *cache,
                         uint64_t now_us,
                         tecla_gateway_reading_average_t *out,
                         uint16_t capacity)
{
    uint16_t count = 0U;
    assert(tecla_gateway_reading_average_snapshot(cache, now_us, out,
        capacity, &count) == TECLA_AVERAGE_OK);
    return count;
}

static void test_three_unique_samples_and_raw_frame_preserved(void)
{
    tecla_gateway_reading_average_cache_t cache;
    tecla_gateway_reading_average_init(&cache);
    const uint64_t minute = UINT64_C(60000000);
    uint8_t frames[3][TECLA_WIRE_V3_MAX_FRAME_SIZE];
    const float values[] = {20.0f, 22.0f, 24.0f};
    for (uint32_t i = 0U; i < 3U; ++i) {
        const size_t length = make_frame(frames[i], 42U, 7U, i + 1U,
            1U, 1U, 1U, 1U, values[i], 3U);
        uint8_t before[TECLA_WIRE_V3_MAX_FRAME_SIZE];
        memcpy(before, frames[i], length);
        assert(tecla_gateway_reading_average_record_frame(&cache, frames[i],
            length, (uint64_t)(i + 1U) * minute) == TECLA_AVERAGE_OK);
        assert(memcmp(before, frames[i], length) == 0);
    }

    tecla_gateway_reading_average_t averages[4];
    assert(snapshot(&cache, 3U * minute, averages, 4U) == 1U);
    assert(averages[0].ready && averages[0].sample_count == 3U);
    assert(fabsf(averages[0].average - 22.0f) < 0.001f);
    assert(averages[0].firmware_node_id == 42U && averages[0].slot_code == 1U);
    assert(averages[0].measurement_code == 1U);
    assert(averages[0].window_start_monotonic_us == minute);
    assert(averages[0].window_end_monotonic_us == 3U * minute);
}

static void test_duplicate_retry_and_conflicting_sequence(void)
{
    tecla_gateway_reading_average_cache_t cache;
    tecla_gateway_reading_average_init(&cache);
    uint8_t frame[TECLA_WIRE_V3_MAX_FRAME_SIZE];
    const size_t length = make_frame(frame, 42U, 9U, 17U, 1U, 1U, 1U, 1U,
                                     25.0f, 3U);
    assert(tecla_gateway_reading_average_record_frame(&cache, frame, length,
        100U) == TECLA_AVERAGE_OK);
    assert(tecla_gateway_reading_average_record_frame(&cache, frame, length,
        200U) == TECLA_AVERAGE_DUPLICATE);

    uint8_t conflict[TECLA_WIRE_V3_MAX_FRAME_SIZE];
    const size_t conflict_len = make_frame(conflict, 42U, 9U, 17U, 1U, 1U,
        1U, 1U, 26.0f, 3U);
    assert(tecla_gateway_reading_average_record_frame(&cache, conflict,
        conflict_len, 300U) == TECLA_AVERAGE_SEQUENCE_CONFLICT);
    assert(cache.duplicate_frames == 1U && cache.conflicting_frames == 1U);

    tecla_gateway_reading_average_t averages[2];
    assert(snapshot(&cache, 300U, averages, 2U) == 1U);
    assert(averages[0].sample_count == 1U && !averages[0].ready);
}

static void test_underfilled_expiry_and_restart(void)
{
    tecla_gateway_reading_average_cache_t cache;
    tecla_gateway_reading_average_init(&cache);
    uint8_t frame[TECLA_WIRE_V3_MAX_FRAME_SIZE];
    const size_t length = make_frame(frame, 42U, 11U, 1U, 1U, 1U, 1U,
                                     2U, 50.0f, 4U);
    assert(tecla_gateway_reading_average_record_frame(&cache, frame, length,
        1U) == TECLA_AVERAGE_OK);
    tecla_gateway_reading_average_t averages[2];
    assert(snapshot(&cache, 1U, averages, 2U) == 1U);
    assert(averages[0].sample_count == 1U && !averages[0].ready);
    assert(snapshot(&cache, 1U + TECLA_GATEWAY_AVERAGE_WINDOW_US,
                    averages, 2U) == 0U);

    /* The gateway cache has no persistence: init models a power restart. */
    assert(tecla_gateway_reading_average_record_frame(&cache, frame, length,
        2U) == TECLA_AVERAGE_OK);
    tecla_gateway_reading_average_init(&cache);
    assert(snapshot(&cache, 3U, averages, 2U) == 0U);
}

static void test_invalid_health_and_nonoperative_node_are_excluded(void)
{
    tecla_gateway_reading_average_cache_t cache;
    tecla_gateway_reading_average_init(&cache);
    uint8_t frame[TECLA_WIRE_V3_MAX_FRAME_SIZE];
    size_t length = make_frame(frame, 42U, 12U, 1U, 1U, 3U, 1U, 1U,
                               20.0f, 3U);
    assert(tecla_gateway_reading_average_record_frame(&cache, frame, length,
        10U) == TECLA_AVERAGE_NO_ELIGIBLE_READINGS);
    length = make_frame(frame, 42U, 12U, 2U, 2U, 1U, 1U, 1U,
                        21.0f, 3U);
    assert(tecla_gateway_reading_average_record_frame(&cache, frame, length,
        20U) == TECLA_AVERAGE_NO_ELIGIBLE_READINGS);
    tecla_gateway_reading_average_t averages[2];
    assert(snapshot(&cache, 20U, averages, 2U) == 0U);

    length = make_frame(frame, 42U, 12U, 3U, 1U, 1U, 1U, 6U,
                        22.0f, 3U);
    assert(tecla_gateway_reading_average_record_frame(&cache, frame, length,
        30U) == TECLA_AVERAGE_NO_ELIGIBLE_READINGS);
    assert(snapshot(&cache, 30U, averages, 2U) == 0U);

    length = make_frame(frame, 42U, 12U, 4U, 1U, 1U, 1U, 1U, 22.0f, 3U);
    const float nan_value = NAN;
    memcpy(frame + TECLA_WIRE_V3_HEADER_SIZE + 83U + 3U,
           &nan_value, sizeof(nan_value));
    assert(tecla_gateway_reading_average_record_frame(&cache, frame, length,
        40U) == TECLA_AVERAGE_INVALID_FRAME);
    assert(snapshot(&cache, 40U, averages, 2U) == 0U);
}

static void test_series_separation_and_latest_sixteen(void)
{
    tecla_gateway_reading_average_cache_t cache;
    tecla_gateway_reading_average_init(&cache);
    uint8_t frame[TECLA_WIRE_V3_MAX_FRAME_SIZE];
    const uint64_t minute = UINT64_C(60000000);
    for (uint32_t i = 1U; i <= 17U; ++i) {
        const size_t length = make_frame(frame, 42U, 13U, i, 1U, 1U, 1U, 1U,
            (float)i, 3U);
        assert(tecla_gateway_reading_average_record_frame(&cache, frame,
            length, (uint64_t)i * minute) == TECLA_AVERAGE_OK);
    }
    /* Same sensor slot/measurement but a changed mapping hash is separate. */
    const size_t changed_len = make_frame(frame, 42U, 13U, 18U, 1U, 1U,
        1U, 1U, 100.0f, 9U);
    assert(tecla_gateway_reading_average_record_frame(&cache, frame,
        changed_len, 18U * minute) == TECLA_AVERAGE_OK);

    tecla_gateway_reading_average_t averages[4];
    assert(snapshot(&cache, 18U * minute, averages, 4U) == 2U);
    bool found_old_mapping = false;
    bool found_new_mapping = false;
    for (uint16_t i = 0U; i < 2U; ++i) {
        if (averages[i].manifest_revision[0] == 3U) {
            found_old_mapping = true;
            assert(averages[i].sample_count == 16U && averages[i].ready);
            assert(fabsf(averages[i].average - 9.5f) < 0.001f);
        } else if (averages[i].manifest_revision[0] == 9U) {
            found_new_mapping = true;
            assert(averages[i].sample_count == 1U && !averages[i].ready);
        }
    }
    assert(found_old_mapping && found_new_mapping);
    assert(cache.evicted_samples == 1U);
}

static void test_ten_nodes_and_two_hundred_forty_series_capacity(void)
{
    tecla_gateway_reading_average_cache_t cache;
    tecla_gateway_reading_average_init(&cache);
    uint8_t frame[TECLA_WIRE_V3_MAX_FRAME_SIZE];
    uint32_t sequences[10] = {0};
    for (uint8_t sample = 1U; sample <= 3U; ++sample) {
        for (uint32_t node = 1U; node <= 10U; ++node) {
            for (uint8_t slot = 1U; slot <= 8U; ++slot) {
                const uint8_t metrics[] = {1U, 2U, 7U};
                for (size_t metric_index = 0U; metric_index < 3U; ++metric_index) {
                    const size_t length = make_frame(frame, node,
                        (uint8_t)(20U + node), ++sequences[node - 1U], 1U,
                        1U, slot, metrics[metric_index],
                        (float)sample + (float)metric_index, 3U);
                    assert(tecla_gateway_reading_average_record_frame(&cache,
                        frame, length, (uint64_t)sample * UINT64_C(60000000)) ==
                        TECLA_AVERAGE_OK);
                }
            }
        }
    }
    tecla_gateway_reading_average_t averages[TECLA_GATEWAY_AVERAGE_MAX_SERIES];
    assert(snapshot(&cache, UINT64_C(180000000), averages,
                    TECLA_GATEWAY_AVERAGE_MAX_SERIES) == 240U);
    assert(cache.sample_count == 720U && cache.evicted_samples == 0U);
    for (uint16_t i = 0U; i < 240U; ++i)
        assert(averages[i].ready && averages[i].sample_count == 3U);
}

static void test_global_sample_capacity_evicts_oldest_only(void)
{
    tecla_gateway_reading_average_cache_t cache;
    tecla_gateway_reading_average_init(&cache);
    uint8_t frame[TECLA_WIRE_V3_MAX_FRAME_SIZE];
    uint32_t sequences[3] = {0};
    uint64_t now_us = 0U;
    for (uint8_t sample = 0U; sample < TECLA_GATEWAY_AVERAGE_MAX_PER_SERIES; ++sample) {
        for (uint32_t node = 1U; node <= 3U; ++node) {
            for (uint8_t slot = 1U; slot <= 8U; ++slot) {
                const uint8_t metrics[] = {1U, 2U, 7U};
                for (size_t metric_index = 0U; metric_index < 3U; ++metric_index) {
                    const size_t length = make_frame(frame, node,
                        (uint8_t)(40U + node), ++sequences[node - 1U], 1U,
                        1U, slot, metrics[metric_index], (float)sample, 3U);
                    now_us += UINT64_C(1000000);
                    assert(tecla_gateway_reading_average_record_frame(&cache,
                        frame, length, now_us) == TECLA_AVERAGE_OK);
                }
            }
        }
    }
    assert(cache.sample_count == TECLA_GATEWAY_AVERAGE_SAMPLE_CAPACITY);
    assert(cache.evicted_samples == 1152U - TECLA_GATEWAY_AVERAGE_SAMPLE_CAPACITY);
}

int main(void)
{
    test_three_unique_samples_and_raw_frame_preserved();
    test_duplicate_retry_and_conflicting_sequence();
    test_underfilled_expiry_and_restart();
    test_invalid_health_and_nonoperative_node_are_excluded();
    test_series_separation_and_latest_sixteen();
    test_ten_nodes_and_two_hundred_forty_series_capacity();
    test_global_sample_capacity_evicts_oldest_only();
    puts("PASS gateway_reading_average_window_dedupe_expiry_invalid_restart_raw");
    return 0;
}
