#ifndef TECLA_GATEWAY_READING_AVERAGE_H
#define TECLA_GATEWAY_READING_AVERAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "tecla_wire_v3.h"

#define TECLA_GATEWAY_AVERAGE_WINDOW_US UINT64_C(10800000000)
#define TECLA_GATEWAY_AVERAGE_MIN_SAMPLES 3U
#define TECLA_GATEWAY_AVERAGE_MAX_PER_SERIES 16U
#define TECLA_GATEWAY_AVERAGE_SAMPLE_CAPACITY 1024U
#define TECLA_GATEWAY_AVERAGE_FRAME_DEDUPE_CAPACITY 512U
#define TECLA_GATEWAY_AVERAGE_MAX_SERIES 256U

typedef struct {
    uint32_t firmware_node_id;
    uint8_t slot_code;
    uint8_t measurement_code;
    uint8_t manifest_revision[32];
    uint8_t assignment_revision[32];
    uint8_t sample_count;
    bool ready;
    float average;
    uint64_t window_start_monotonic_us;
    uint64_t window_end_monotonic_us;
} tecla_gateway_reading_average_t;

typedef struct {
    uint32_t firmware_node_id;
    uint8_t slot_code;
    uint8_t measurement_code;
    uint8_t manifest_revision[32];
    uint8_t assignment_revision[32];
    bool used;
} tecla_gateway_average_series_key_t;

typedef struct {
    uint32_t firmware_node_id;
    uint8_t boot_id[16];
    uint32_t seq;
    uint64_t captured_monotonic_us;
    uint64_t frame_fingerprint;
    bool valid;
} tecla_gateway_average_frame_id_t;

typedef struct {
    uint16_t series_index;
    uint64_t captured_monotonic_us;
    float value;
    bool valid;
} tecla_gateway_average_sample_t;

typedef enum {
    TECLA_AVERAGE_OK = 0,
    TECLA_AVERAGE_DUPLICATE,
    TECLA_AVERAGE_SEQUENCE_CONFLICT,
    TECLA_AVERAGE_NO_ELIGIBLE_READINGS,
    TECLA_AVERAGE_INVALID_ARGUMENT,
    TECLA_AVERAGE_INVALID_FRAME,
    TECLA_AVERAGE_BUFFER_TOO_SMALL
} tecla_gateway_average_status_t;

typedef struct {
    tecla_gateway_average_series_key_t series[TECLA_GATEWAY_AVERAGE_MAX_SERIES];
    tecla_gateway_average_sample_t samples[TECLA_GATEWAY_AVERAGE_SAMPLE_CAPACITY];
    tecla_gateway_average_frame_id_t seen_frames[TECLA_GATEWAY_AVERAGE_FRAME_DEDUPE_CAPACITY];
    uint32_t duplicate_frames;
    uint32_t conflicting_frames;
    uint32_t dedupe_evictions;
    uint32_t dropped_series;
    uint32_t evicted_samples;
    uint16_t sample_count;
    uint16_t next_frame_slot;
} tecla_gateway_reading_average_cache_t;

/* RAM only. A gateway restart clears all rolling samples and dedupe history. */
void tecla_gateway_reading_average_init(tecla_gateway_reading_average_cache_t *cache);

/* Accepts only v3 TELEMETRY from an OPERATIVE node and FUNCIONANDO sensors.
 * Captured time is gateway monotonic receive time, not a fabricated UTC time.
 * RSSI, node/sensor administrative states and non-finite values are excluded. */
tecla_gateway_average_status_t tecla_gateway_reading_average_record_frame(
    tecla_gateway_reading_average_cache_t *cache,
    const uint8_t *frame,
    size_t frame_len,
    uint64_t gateway_received_monotonic_us);

/* Expire samples and compute one arithmetic mean per node/slot/measurement and
 * exact manifest+assignment hash pair. Underfilled windows remain visible with
 * ready=false and their sample_count; no average is claimed until n >= 3. */
tecla_gateway_average_status_t tecla_gateway_reading_average_snapshot(
    tecla_gateway_reading_average_cache_t *cache,
    uint64_t now_monotonic_us,
    tecla_gateway_reading_average_t *out,
    uint16_t out_capacity,
    uint16_t *count_out);

#endif
