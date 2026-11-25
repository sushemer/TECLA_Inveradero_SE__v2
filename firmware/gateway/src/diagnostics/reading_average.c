#include "reading_average.h"

#include <math.h>
#include <string.h>

#define MODE_OPERATIVE 1U
#define SENSOR_HEALTH_FUNCTIONING 1U

static bool bytes_nonzero(const uint8_t *bytes, size_t length)
{
    uint8_t any = 0U;
    for (size_t i = 0U; i < length; ++i) any |= bytes[i];
    return any != 0U;
}

static uint64_t fingerprint(const uint8_t *bytes, size_t length)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t i = 0U; i < length; ++i) {
        hash ^= bytes[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static bool expired(uint64_t captured_us, uint64_t now_us)
{
    return now_us < captured_us ||
           now_us - captured_us >= TECLA_GATEWAY_AVERAGE_WINDOW_US;
}

static bool environmental_measurement(uint8_t code)
{
    /* Current confirmed/active inventory only: DHT22 temperature/humidity and
     * VEML7700 illuminance. SEN0193 has no active calibrated driver yet. */
    return code == 1U || code == 2U || code == 7U;
}

static void prune(tecla_gateway_reading_average_cache_t *cache, uint64_t now_us)
{
    for (size_t i = 0U; i < TECLA_GATEWAY_AVERAGE_SAMPLE_CAPACITY; ++i) {
        if (cache->samples[i].valid && expired(cache->samples[i].captured_monotonic_us, now_us)) {
            cache->samples[i].valid = false;
            cache->sample_count--;
        }
    }
    for (size_t i = 0U; i < TECLA_GATEWAY_AVERAGE_FRAME_DEDUPE_CAPACITY; ++i) {
        if (cache->seen_frames[i].valid &&
            expired(cache->seen_frames[i].captured_monotonic_us, now_us)) {
            cache->seen_frames[i].valid = false;
        }
    }
}

static bool same_frame(const tecla_gateway_average_frame_id_t *seen,
                       const tecla_wire_v3_header_t *header)
{
    return seen->valid && seen->firmware_node_id == header->firmware_node_id &&
           seen->seq == header->seq &&
           memcmp(seen->boot_id, header->boot_id, sizeof(seen->boot_id)) == 0;
}

static bool same_series_key(const tecla_gateway_average_series_key_t *key,
                            uint32_t node_id, const uint8_t manifest[32],
                            const uint8_t assignment[32], uint8_t slot,
                            uint8_t measurement)
{
    return key->used && key->firmware_node_id == node_id &&
           key->slot_code == slot && key->measurement_code == measurement &&
           memcmp(key->manifest_revision, manifest, 32U) == 0 &&
           memcmp(key->assignment_revision, assignment, 32U) == 0;
}

static bool series_has_samples(const tecla_gateway_reading_average_cache_t *cache,
                               uint16_t series_index)
{
    for (size_t i = 0U; i < TECLA_GATEWAY_AVERAGE_SAMPLE_CAPACITY; ++i) {
        if (cache->samples[i].valid && cache->samples[i].series_index == series_index)
            return true;
    }
    return false;
}

static uint16_t find_or_create_series(tecla_gateway_reading_average_cache_t *cache,
                                      uint32_t node_id, const uint8_t manifest[32],
                                      const uint8_t assignment[32], uint8_t slot,
                                      uint8_t measurement)
{
    uint16_t free_index = TECLA_GATEWAY_AVERAGE_MAX_SERIES;
    for (uint16_t i = 0U; i < TECLA_GATEWAY_AVERAGE_MAX_SERIES; ++i) {
        tecla_gateway_average_series_key_t *key = &cache->series[i];
        if (same_series_key(key, node_id, manifest, assignment, slot, measurement))
            return i;
        if (free_index == TECLA_GATEWAY_AVERAGE_MAX_SERIES &&
            (!key->used || !series_has_samples(cache, i))) free_index = i;
    }
    if (free_index == TECLA_GATEWAY_AVERAGE_MAX_SERIES) return free_index;
    tecla_gateway_average_series_key_t *key = &cache->series[free_index];
    memset(key, 0, sizeof(*key));
    key->firmware_node_id = node_id;
    key->slot_code = slot;
    key->measurement_code = measurement;
    memcpy(key->manifest_revision, manifest, 32U);
    memcpy(key->assignment_revision, assignment, 32U);
    key->used = true;
    return free_index;
}

static size_t find_oldest_for_series(
    const tecla_gateway_reading_average_cache_t *cache, uint16_t series_index,
    uint8_t *series_count)
{
    size_t oldest_index = TECLA_GATEWAY_AVERAGE_SAMPLE_CAPACITY;
    uint64_t oldest_time = UINT64_MAX;
    *series_count = 0U;
    for (size_t i = 0U; i < TECLA_GATEWAY_AVERAGE_SAMPLE_CAPACITY; ++i) {
        const tecla_gateway_average_sample_t *sample = &cache->samples[i];
        if (!sample->valid || sample->series_index != series_index) continue;
        (*series_count)++;
        if (sample->captured_monotonic_us < oldest_time) {
            oldest_time = sample->captured_monotonic_us;
            oldest_index = i;
        }
    }
    return oldest_index;
}

static size_t find_free_sample(const tecla_gateway_reading_average_cache_t *cache)
{
    for (size_t i = 0U; i < TECLA_GATEWAY_AVERAGE_SAMPLE_CAPACITY; ++i) {
        if (!cache->samples[i].valid) return i;
    }
    return TECLA_GATEWAY_AVERAGE_SAMPLE_CAPACITY;
}

static size_t find_oldest_sample(const tecla_gateway_reading_average_cache_t *cache)
{
    size_t oldest_index = TECLA_GATEWAY_AVERAGE_SAMPLE_CAPACITY;
    uint64_t oldest_time = UINT64_MAX;
    for (size_t i = 0U; i < TECLA_GATEWAY_AVERAGE_SAMPLE_CAPACITY; ++i) {
        if (cache->samples[i].valid && cache->samples[i].captured_monotonic_us < oldest_time) {
            oldest_time = cache->samples[i].captured_monotonic_us;
            oldest_index = i;
        }
    }
    return oldest_index;
}

static bool store_reading(tecla_gateway_reading_average_cache_t *cache,
                          const tecla_wire_v3_header_t *header,
                          const tecla_wire_v3_telemetry_t *telemetry,
                          const tecla_wire_v3_reading_t *reading,
                          uint64_t received_us)
{
    const uint16_t series_index = find_or_create_series(cache,
        header->firmware_node_id, telemetry->manifest_revision,
        telemetry->assignment_revision, reading->slot_code,
        reading->measurement_code);
    if (series_index >= TECLA_GATEWAY_AVERAGE_MAX_SERIES) {
        cache->dropped_series++;
        return false;
    }
    uint8_t current_count = 0U;
    size_t index = find_oldest_for_series(cache, series_index, &current_count);
    if (current_count >= TECLA_GATEWAY_AVERAGE_MAX_PER_SERIES) {
        /* Keep only the latest 16 samples in every physical measurement series. */
        cache->samples[index].valid = false;
        cache->sample_count--;
        cache->evicted_samples++;
    }
    index = find_free_sample(cache);
    if (index == TECLA_GATEWAY_AVERAGE_SAMPLE_CAPACITY) {
        index = find_oldest_sample(cache);
        if (index == TECLA_GATEWAY_AVERAGE_SAMPLE_CAPACITY) return false;
        cache->samples[index].valid = false;
        cache->sample_count--;
        cache->evicted_samples++;
    }

    tecla_gateway_average_sample_t *sample = &cache->samples[index];
    memset(sample, 0, sizeof(*sample));
    sample->series_index = series_index;
    sample->captured_monotonic_us = received_us;
    sample->value = reading->value;
    sample->valid = true;
    cache->sample_count++;
    return true;
}

void tecla_gateway_reading_average_init(tecla_gateway_reading_average_cache_t *cache)
{
    if (cache != NULL) memset(cache, 0, sizeof(*cache));
}

tecla_gateway_average_status_t tecla_gateway_reading_average_record_frame(
    tecla_gateway_reading_average_cache_t *cache, const uint8_t *frame,
    size_t frame_len, uint64_t gateway_received_monotonic_us)
{
    if (cache == NULL || frame == NULL || frame_len == 0U)
        return TECLA_AVERAGE_INVALID_ARGUMENT;

    tecla_wire_v3_header_t header;
    tecla_wire_v3_message_t message;
    if (tecla_wire_v3_decode_typed(frame, frame_len, &header, &message) != TECLA_WIRE_V3_OK ||
        header.type != TECLA_WIRE_V3_TELEMETRY || header.firmware_node_id == 0U ||
        header.seq == 0U || header.seq == UINT32_MAX ||
        !bytes_nonzero(header.boot_id, sizeof(header.boot_id))) {
        return TECLA_AVERAGE_INVALID_FRAME;
    }

    prune(cache, gateway_received_monotonic_us);
    const uint64_t digest = fingerprint(frame, frame_len);
    for (size_t i = 0U; i < TECLA_GATEWAY_AVERAGE_FRAME_DEDUPE_CAPACITY; ++i) {
        tecla_gateway_average_frame_id_t *seen = &cache->seen_frames[i];
        if (!same_frame(seen, &header)) continue;
        if (seen->frame_fingerprint == digest) {
            cache->duplicate_frames++;
            return TECLA_AVERAGE_DUPLICATE;
        }
        cache->conflicting_frames++;
        return TECLA_AVERAGE_SEQUENCE_CONFLICT;
    }

    const uint16_t frame_slot = cache->next_frame_slot;
    tecla_gateway_average_frame_id_t *seen = &cache->seen_frames[frame_slot];
    if (seen->valid) cache->dedupe_evictions++;
    memset(seen, 0, sizeof(*seen));
    seen->firmware_node_id = header.firmware_node_id;
    memcpy(seen->boot_id, header.boot_id, sizeof(seen->boot_id));
    seen->seq = header.seq;
    seen->captured_monotonic_us = gateway_received_monotonic_us;
    seen->frame_fingerprint = digest;
    seen->valid = true;
    cache->next_frame_slot = (uint16_t)((frame_slot + 1U) % TECLA_GATEWAY_AVERAGE_FRAME_DEDUPE_CAPACITY);

    const tecla_wire_v3_telemetry_t *telemetry = &message.telemetry;
    if (telemetry->effective_mode != MODE_OPERATIVE) return TECLA_AVERAGE_NO_ELIGIBLE_READINGS;
    uint8_t accepted = 0U;
    for (uint8_t i = 0U; i < telemetry->reading_count; ++i) {
        const tecla_wire_v3_reading_t *reading = &telemetry->readings[i];
        if (reading->sensor_health != SENSOR_HEALTH_FUNCTIONING ||
            reading->slot_code == 0U || reading->slot_code == UINT8_MAX ||
            !environmental_measurement(reading->measurement_code) ||
            !isfinite(reading->value)) continue;
        if (store_reading(cache, &header, telemetry, reading,
                          gateway_received_monotonic_us)) accepted++;
    }
    return accepted == 0U ? TECLA_AVERAGE_NO_ELIGIBLE_READINGS : TECLA_AVERAGE_OK;
}

tecla_gateway_average_status_t tecla_gateway_reading_average_snapshot(
    tecla_gateway_reading_average_cache_t *cache, uint64_t now_monotonic_us,
    tecla_gateway_reading_average_t *out, uint16_t out_capacity,
    uint16_t *count_out)
{
    if (cache == NULL || count_out == NULL || (out == NULL && out_capacity != 0U))
        return TECLA_AVERAGE_INVALID_ARGUMENT;
    prune(cache, now_monotonic_us);
    *count_out = 0U;
    for (size_t i = 0U; i < TECLA_GATEWAY_AVERAGE_SAMPLE_CAPACITY; ++i) {
        const tecla_gateway_average_sample_t *sample = &cache->samples[i];
        if (!sample->valid) continue;
        const tecla_gateway_average_series_key_t *key =
            &cache->series[sample->series_index];
        uint16_t result_index = 0U;
        while (result_index < *count_out &&
               !(out[result_index].firmware_node_id == key->firmware_node_id &&
                 out[result_index].slot_code == key->slot_code &&
                 out[result_index].measurement_code == key->measurement_code &&
                 memcmp(out[result_index].manifest_revision, key->manifest_revision, 32U) == 0 &&
                 memcmp(out[result_index].assignment_revision, key->assignment_revision, 32U) == 0))
            result_index++;
        if (result_index == *count_out) {
            if (*count_out >= out_capacity || *count_out >= TECLA_GATEWAY_AVERAGE_MAX_SERIES)
                return TECLA_AVERAGE_BUFFER_TOO_SMALL;
            tecla_gateway_reading_average_t *average = &out[*count_out];
            memset(average, 0, sizeof(*average));
            average->firmware_node_id = key->firmware_node_id;
            average->slot_code = key->slot_code;
            average->measurement_code = key->measurement_code;
            memcpy(average->manifest_revision, key->manifest_revision, 32U);
            memcpy(average->assignment_revision, key->assignment_revision, 32U);
            average->window_start_monotonic_us = sample->captured_monotonic_us;
            average->window_end_monotonic_us = sample->captured_monotonic_us;
            (*count_out)++;
        }
        tecla_gateway_reading_average_t *average = &out[result_index];
        const float current_sum = average->average * (float)average->sample_count;
        average->sample_count++;
        average->average = (current_sum + sample->value) / (float)average->sample_count;
        if (sample->captured_monotonic_us < average->window_start_monotonic_us)
            average->window_start_monotonic_us = sample->captured_monotonic_us;
        if (sample->captured_monotonic_us > average->window_end_monotonic_us)
            average->window_end_monotonic_us = sample->captured_monotonic_us;
    }
    for (uint16_t i = 0U; i < *count_out; ++i)
        out[i].ready = out[i].sample_count >= TECLA_GATEWAY_AVERAGE_MIN_SAMPLES;
    return TECLA_AVERAGE_OK;
}
