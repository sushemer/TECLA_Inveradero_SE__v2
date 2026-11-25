#ifndef TECLA_WIRE_V3_H
#define TECLA_WIRE_V3_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECLA_WIRE_V3_VERSION 3U
#define TECLA_WIRE_V3_HEADER_SIZE 28U
#define TECLA_WIRE_V3_MAX_FRAME_SIZE 250U
#define TECLA_WIRE_V3_MAX_SLOTS 8U
#define TECLA_WIRE_V3_MAX_MEASUREMENTS 7U
#define TECLA_WIRE_V3_MAX_READINGS 8U

typedef enum {
    TECLA_WIRE_V3_CAPABILITIES = 1,
    TECLA_WIRE_V3_HEARTBEAT = 2,
    TECLA_WIRE_V3_TELEMETRY = 3,
    TECLA_WIRE_V3_TELEMETRY_ACK = 4,
    TECLA_WIRE_V3_COMMAND_CONFIG = 5,
    TECLA_WIRE_V3_COMMAND_ACK = 6,
    TECLA_WIRE_V3_MANIFEST_CONFIG = 7,
    TECLA_WIRE_V3_MANIFEST_ACK = 8
} tecla_wire_v3_type_t;

typedef enum {
    TECLA_WIRE_V3_OK = 0,
    TECLA_WIRE_V3_ERR_ARGUMENT,
    TECLA_WIRE_V3_ERR_CAPACITY,
    TECLA_WIRE_V3_ERR_LENGTH,
    TECLA_WIRE_V3_ERR_VERSION,
    TECLA_WIRE_V3_ERR_TYPE,
    TECLA_WIRE_V3_ERR_FIELD
} tecla_wire_v3_status_t;

typedef struct {
    uint32_t firmware_node_id;
    uint8_t boot_id[16];
    uint32_t seq;
    uint8_t type;
} tecla_wire_v3_header_t;

typedef struct {
    tecla_wire_v3_header_t header;
    const uint8_t *payload;
    size_t payload_len;
} tecla_wire_v3_frame_view_t;

typedef struct {
    uint8_t slot_code;
    uint8_t estado_administrativo;
    uint8_t enabled;
} tecla_wire_v3_slot_t;

typedef struct {
    uint8_t slot_code;
    uint8_t measurement_count;
    uint8_t measurement_codes[TECLA_WIRE_V3_MAX_MEASUREMENTS];
} tecla_wire_v3_capability_t;

typedef struct {
    uint8_t slot_code;
    uint8_t measurement_code;
    uint8_t sensor_health;
    float value;
} tecla_wire_v3_reading_t;

/* Host- and target-independent typed payload views. They are serialized
 * bytewise; their in-memory padding/endianness is never sent over ESP-NOW. */
typedef struct {
    uint8_t slot_count;
    tecla_wire_v3_capability_t slots[TECLA_WIRE_V3_MAX_SLOTS];
} tecla_wire_v3_capabilities_t;

typedef struct {
    uint32_t command_config_revision_applied;
    uint8_t effective_mode;
    bool last_command_present;
    uint8_t last_applied_command_id[16];
    uint8_t manifest_revision_applied[32];
    uint8_t assignment_revision_applied[32];
} tecla_wire_v3_heartbeat_t;

typedef struct {
    uint32_t command_config_revision_applied;
    uint8_t effective_mode;
    uint8_t node_health;
    uint16_t sample_interval_s;
    uint64_t sample_uptime_ms;
    uint8_t time_quality;
    uint8_t battery_percent;
    uint8_t reading_count;
    uint8_t manifest_revision[32];
    uint8_t assignment_revision[32];
    tecla_wire_v3_reading_t readings[TECLA_WIRE_V3_MAX_READINGS];
} tecla_wire_v3_telemetry_t;

typedef struct {
    uint8_t scope;
    uint8_t reason;
} tecla_wire_v3_telemetry_ack_t;

typedef struct {
    uint8_t command_id[16];
    uint32_t command_config_revision_expected;
    uint32_t command_config_revision_target;
    uint8_t effective_mode;
    uint16_t sample_interval_s;
    uint32_t management_poll_interval_s;
    uint8_t slot_count;
    uint8_t manifest_revision[32];
    uint8_t assignment_revision[32];
    tecla_wire_v3_slot_t slots[TECLA_WIRE_V3_MAX_SLOTS];
} tecla_wire_v3_command_config_t;

typedef struct {
    uint8_t command_id[16];
    uint32_t command_config_revision_expected;
    uint32_t command_config_revision_target;
    uint32_t command_config_revision_applied;
    uint8_t stage;
    uint8_t reason;
} tecla_wire_v3_command_ack_t;

typedef struct {
    uint8_t effective_mode;
    uint16_t sample_interval_s;
    uint32_t management_poll_interval_s;
    uint8_t slot_count;
    uint8_t manifest_revision[32];
    uint8_t assignment_revision[32];
    tecla_wire_v3_slot_t slots[TECLA_WIRE_V3_MAX_SLOTS];
} tecla_wire_v3_manifest_config_t;

typedef struct {
    uint8_t manifest_revision[32];
    uint8_t assignment_revision[32];
    uint8_t stage;
    uint8_t reason;
} tecla_wire_v3_manifest_ack_t;

typedef union {
    tecla_wire_v3_capabilities_t capabilities;
    tecla_wire_v3_heartbeat_t heartbeat;
    tecla_wire_v3_telemetry_t telemetry;
    tecla_wire_v3_telemetry_ack_t telemetry_ack;
    tecla_wire_v3_command_config_t command_config;
    tecla_wire_v3_command_ack_t command_ack;
    tecla_wire_v3_manifest_config_t manifest_config;
    tecla_wire_v3_manifest_ack_t manifest_ack;
} tecla_wire_v3_message_t;

/* Encode/decode the 28-byte envelope bytewise; payload layouts are validated
 * by type and remain a byte view so no packed C struct reaches the radio. */
tecla_wire_v3_status_t tecla_wire_v3_encode(
    const tecla_wire_v3_header_t *header, const uint8_t *payload,
    size_t payload_len, uint8_t *out, size_t out_capacity, size_t *out_len);

tecla_wire_v3_status_t tecla_wire_v3_decode(
    const uint8_t *frame, size_t frame_len, tecla_wire_v3_frame_view_t *out);

/* Typed builders/parsers for the eight v3 payloads. */
tecla_wire_v3_status_t tecla_wire_v3_encode_typed(
    const tecla_wire_v3_header_t *header, const tecla_wire_v3_message_t *message,
    uint8_t *out, size_t out_capacity, size_t *out_len);
tecla_wire_v3_status_t tecla_wire_v3_decode_typed(
    const uint8_t *frame, size_t frame_len, tecla_wire_v3_header_t *header_out,
    tecla_wire_v3_message_t *message_out);

/* API revisions are 64 hex chars. Convert them to exactly the 32 raw SHA-256
 * bytes used by v3; this helper validates length and every hex digit. */
tecla_wire_v3_status_t tecla_wire_v3_decode_sha256_hex(
    const char *hex, size_t hex_len, uint8_t out[32]);
tecla_wire_v3_status_t tecla_wire_v3_encode_sha256_hex(
    const uint8_t hash[32], char out[65]);

const char *tecla_wire_v3_status_name(tecla_wire_v3_status_t status);

#endif
