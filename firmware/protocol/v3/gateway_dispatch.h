#ifndef TECLA_WIRE_V3_GATEWAY_DISPATCH_H
#define TECLA_WIRE_V3_GATEWAY_DISPATCH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "tecla_wire_v3.h"

#define TECLA_WIRE_V3_MAC_BYTES 6U
#define TECLA_WIRE_V3_MAX_BINDINGS 8U
#define TECLA_WIRE_V3_VOLATILE_QUEUE_CAPACITY 8U

typedef struct {
    uint32_t firmware_node_id;
    uint8_t mac[TECLA_WIRE_V3_MAC_BYTES];
} tecla_wire_v3_binding_t;

typedef struct {
    uint8_t source_mac[TECLA_WIRE_V3_MAC_BYTES];
    size_t frame_len;
    uint8_t frame[TECLA_WIRE_V3_MAX_FRAME_SIZE];
} tecla_wire_v3_buffered_telemetry_t;

typedef enum {
    TECLA_WIRE_V3_DISPATCH_ACCEPTED = 0,
    TECLA_WIRE_V3_DISPATCH_DUPLICATE,
    TECLA_WIRE_V3_DISPATCH_UNAUTHORIZED,
    TECLA_WIRE_V3_DISPATCH_NOT_TELEMETRY,
    TECLA_WIRE_V3_DISPATCH_MALFORMED,
    TECLA_WIRE_V3_DISPATCH_SEQUENCE_CONFLICT,
    TECLA_WIRE_V3_DISPATCH_CACHE_FULL,
    TECLA_WIRE_V3_DISPATCH_ACK_TX_FAILED,
    TECLA_WIRE_V3_DISPATCH_INVALID_ARGUMENT,
    TECLA_WIRE_V3_DISPATCH_CONFIG_ERROR,
    TECLA_WIRE_V3_DISPATCH_QUEUE_EMPTY,
    TECLA_WIRE_V3_DISPATCH_HEAD_MISMATCH
} tecla_wire_v3_dispatch_status_t;

typedef bool (*tecla_wire_v3_ack_send_fn)(void *context,
                                          const uint8_t destination_mac[6],
                                          const uint8_t *frame,
                                          size_t frame_len);

typedef struct {
    tecla_wire_v3_binding_t bindings[TECLA_WIRE_V3_MAX_BINDINGS];
    uint8_t binding_count;
    tecla_wire_v3_buffered_telemetry_t queue[TECLA_WIRE_V3_VOLATILE_QUEUE_CAPACITY];
    uint8_t queue_count;
    uint32_t dropped_untrusted;
    uint32_t malformed_count;
    uint32_t overflow_count;
    uint32_t conflicting_duplicate_count;
} tecla_wire_v3_gateway_dispatch_t;

/* Binding is an explicit routing allowlist only; a MAC is not authentication.
 * No binding is inferred from profile, RSSI, SQL ID, or broadcast. */
tecla_wire_v3_dispatch_status_t tecla_wire_v3_gateway_dispatch_init(
    tecla_wire_v3_gateway_dispatch_t *gateway,
    const tecla_wire_v3_binding_t *bindings,
    uint8_t binding_count);

/* Buffers complete frames in RAM before sending LOCAL_VOLATILE_BUFFERED.
 * NACK/CACHE_FULL is sent when the RAM queue is full. ACK callback only reports
 * that the radio adapter accepted the send request; the node still must receive
 * and validate the application ACK. No HTTP/central commit is implied. */
tecla_wire_v3_dispatch_status_t tecla_wire_v3_gateway_dispatch_frame(
    tecla_wire_v3_gateway_dispatch_t *gateway,
    const uint8_t source_mac[TECLA_WIRE_V3_MAC_BYTES],
    const uint8_t *frame,
    size_t frame_len,
    tecla_wire_v3_ack_send_fn send_ack,
    void *send_context);

/* Read-only queue snapshot for a future local UI/consumer. */
tecla_wire_v3_dispatch_status_t tecla_wire_v3_gateway_queue_snapshot(
    const tecla_wire_v3_gateway_dispatch_t *gateway,
    tecla_wire_v3_buffered_telemetry_t *out,
    uint8_t out_capacity,
    uint8_t *count_out);

tecla_wire_v3_dispatch_status_t tecla_wire_v3_gateway_queue_peek_head(
    const tecla_wire_v3_gateway_dispatch_t *gateway,
    tecla_wire_v3_buffered_telemetry_t *out);

/* Call only after the downstream consumer has durably handled the head item.
 * Identity matching prevents releasing a different or not-yet-consumed frame. */
tecla_wire_v3_dispatch_status_t tecla_wire_v3_gateway_queue_ack_head(
    tecla_wire_v3_gateway_dispatch_t *gateway,
    uint32_t firmware_node_id,
    const uint8_t boot_id[16],
    uint32_t seq);

#endif
