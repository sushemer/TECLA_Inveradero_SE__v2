#include "gateway_dispatch.h"

#include <string.h>

static bool mac_is_zero(const uint8_t mac[TECLA_WIRE_V3_MAC_BYTES])
{
    uint8_t aggregate = 0U;
    for (size_t i = 0U; i < TECLA_WIRE_V3_MAC_BYTES; ++i) aggregate |= mac[i];
    return aggregate == 0U;
}

static bool binding_matches(const tecla_wire_v3_gateway_dispatch_t *gateway,
                            uint32_t firmware_node_id,
                            const uint8_t source_mac[TECLA_WIRE_V3_MAC_BYTES])
{
    for (uint8_t i = 0U; i < gateway->binding_count; ++i) {
        const tecla_wire_v3_binding_t *binding = &gateway->bindings[i];
        if (binding->firmware_node_id == firmware_node_id &&
            memcmp(binding->mac, source_mac, TECLA_WIRE_V3_MAC_BYTES) == 0) return true;
    }
    return false;
}

static bool same_identity(const tecla_wire_v3_frame_view_t *a,
                          const tecla_wire_v3_frame_view_t *b)
{
    return a->header.firmware_node_id == b->header.firmware_node_id &&
           a->header.seq == b->header.seq &&
           memcmp(a->header.boot_id, b->header.boot_id, 16U) == 0;
}

static tecla_wire_v3_dispatch_status_t send_telemetry_ack(
    const tecla_wire_v3_frame_view_t *received,
    uint8_t scope,
    uint8_t reason,
    tecla_wire_v3_ack_send_fn send_ack,
    void *send_context,
    const uint8_t source_mac[TECLA_WIRE_V3_MAC_BYTES])
{
    tecla_wire_v3_header_t header = received->header;
    header.type = TECLA_WIRE_V3_TELEMETRY_ACK;
    tecla_wire_v3_message_t message = {0};
    message.telemetry_ack.scope = scope;
    message.telemetry_ack.reason = reason;
    uint8_t ack_frame[TECLA_WIRE_V3_MAX_FRAME_SIZE];
    size_t ack_len = 0U;
    if (tecla_wire_v3_encode_typed(&header, &message, ack_frame,
                                   sizeof(ack_frame), &ack_len) != TECLA_WIRE_V3_OK) {
        return TECLA_WIRE_V3_DISPATCH_MALFORMED;
    }
    return send_ack(send_context, source_mac, ack_frame, ack_len)
        ? TECLA_WIRE_V3_DISPATCH_ACCEPTED
        : TECLA_WIRE_V3_DISPATCH_ACK_TX_FAILED;
}

tecla_wire_v3_dispatch_status_t tecla_wire_v3_gateway_dispatch_init(
    tecla_wire_v3_gateway_dispatch_t *gateway,
    const tecla_wire_v3_binding_t *bindings,
    uint8_t binding_count)
{
    if (gateway == NULL || (binding_count != 0U && bindings == NULL))
        return TECLA_WIRE_V3_DISPATCH_INVALID_ARGUMENT;
    if (binding_count > TECLA_WIRE_V3_MAX_BINDINGS)
        return TECLA_WIRE_V3_DISPATCH_CONFIG_ERROR;
    memset(gateway, 0, sizeof(*gateway));
    for (uint8_t i = 0U; i < binding_count; ++i) {
        if (bindings[i].firmware_node_id == 0U || mac_is_zero(bindings[i].mac))
            return TECLA_WIRE_V3_DISPATCH_CONFIG_ERROR;
        for (uint8_t j = 0U; j < i; ++j) {
            if (bindings[i].firmware_node_id == bindings[j].firmware_node_id ||
                memcmp(bindings[i].mac, bindings[j].mac,
                       TECLA_WIRE_V3_MAC_BYTES) == 0) {
                return TECLA_WIRE_V3_DISPATCH_CONFIG_ERROR;
            }
        }
    }
    if (binding_count != 0U) {
        memcpy(gateway->bindings, bindings,
               (size_t)binding_count * sizeof(bindings[0]));
    }
    gateway->binding_count = binding_count;
    return TECLA_WIRE_V3_DISPATCH_ACCEPTED;
}

tecla_wire_v3_dispatch_status_t tecla_wire_v3_gateway_dispatch_frame(
    tecla_wire_v3_gateway_dispatch_t *gateway,
    const uint8_t source_mac[TECLA_WIRE_V3_MAC_BYTES],
    const uint8_t *frame,
    size_t frame_len,
    tecla_wire_v3_ack_send_fn send_ack,
    void *send_context)
{
    if (gateway == NULL || source_mac == NULL || frame == NULL || send_ack == NULL)
        return TECLA_WIRE_V3_DISPATCH_INVALID_ARGUMENT;
    tecla_wire_v3_frame_view_t received;
    if (tecla_wire_v3_decode(frame, frame_len, &received) != TECLA_WIRE_V3_OK) {
        gateway->malformed_count++;
        return TECLA_WIRE_V3_DISPATCH_MALFORMED;
    }
    if (received.header.type != TECLA_WIRE_V3_TELEMETRY)
        return TECLA_WIRE_V3_DISPATCH_NOT_TELEMETRY;
    if (!binding_matches(gateway, received.header.firmware_node_id, source_mac)) {
        gateway->dropped_untrusted++;
        return TECLA_WIRE_V3_DISPATCH_UNAUTHORIZED;
    }

    for (uint8_t i = 0U; i < gateway->queue_count; ++i) {
        tecla_wire_v3_frame_view_t prior;
        const tecla_wire_v3_buffered_telemetry_t *record = &gateway->queue[i];
        if (tecla_wire_v3_decode(record->frame, record->frame_len, &prior) != TECLA_WIRE_V3_OK)
            continue;
        if (!same_identity(&received, &prior)) continue;
        if (record->frame_len != frame_len || memcmp(record->frame, frame, frame_len) != 0) {
            gateway->conflicting_duplicate_count++;
            return TECLA_WIRE_V3_DISPATCH_SEQUENCE_CONFLICT;
        }
        const tecla_wire_v3_dispatch_status_t ack = send_telemetry_ack(
            &received, 1U, 0U, send_ack, send_context, source_mac);
        return ack == TECLA_WIRE_V3_DISPATCH_ACCEPTED
            ? TECLA_WIRE_V3_DISPATCH_DUPLICATE : ack;
    }

    if (gateway->queue_count >= TECLA_WIRE_V3_VOLATILE_QUEUE_CAPACITY) {
        gateway->overflow_count++;
        const tecla_wire_v3_dispatch_status_t ack = send_telemetry_ack(
            &received, 3U, 11U, send_ack, send_context, source_mac);
        return ack == TECLA_WIRE_V3_DISPATCH_ACCEPTED
            ? TECLA_WIRE_V3_DISPATCH_CACHE_FULL : ack;
    }

    tecla_wire_v3_buffered_telemetry_t *stored = &gateway->queue[gateway->queue_count];
    memcpy(stored->source_mac, source_mac, TECLA_WIRE_V3_MAC_BYTES);
    memcpy(stored->frame, frame, frame_len);
    stored->frame_len = frame_len;
    gateway->queue_count++;
    return send_telemetry_ack(&received, 1U, 0U, send_ack, send_context,
                              source_mac);
}

tecla_wire_v3_dispatch_status_t tecla_wire_v3_gateway_queue_snapshot(
    const tecla_wire_v3_gateway_dispatch_t *gateway,
    tecla_wire_v3_buffered_telemetry_t *out,
    uint8_t out_capacity,
    uint8_t *count_out)
{
    if (gateway == NULL || count_out == NULL || (out == NULL && out_capacity != 0U))
        return TECLA_WIRE_V3_DISPATCH_INVALID_ARGUMENT;
    *count_out = gateway->queue_count;
    if (out_capacity < gateway->queue_count)
        return TECLA_WIRE_V3_DISPATCH_CONFIG_ERROR;
    if (gateway->queue_count != 0U) {
        memcpy(out, gateway->queue,
               (size_t)gateway->queue_count * sizeof(gateway->queue[0]));
    }
    return TECLA_WIRE_V3_DISPATCH_ACCEPTED;
}

tecla_wire_v3_dispatch_status_t tecla_wire_v3_gateway_queue_peek_head(
    const tecla_wire_v3_gateway_dispatch_t *gateway,
    tecla_wire_v3_buffered_telemetry_t *out)
{
    if (gateway == NULL || out == NULL) return TECLA_WIRE_V3_DISPATCH_INVALID_ARGUMENT;
    if (gateway->queue_count == 0U) return TECLA_WIRE_V3_DISPATCH_QUEUE_EMPTY;
    *out = gateway->queue[0];
    return TECLA_WIRE_V3_DISPATCH_ACCEPTED;
}

tecla_wire_v3_dispatch_status_t tecla_wire_v3_gateway_queue_ack_head(
    tecla_wire_v3_gateway_dispatch_t *gateway,
    uint32_t firmware_node_id,
    const uint8_t boot_id[16],
    uint32_t seq)
{
    if (gateway == NULL || boot_id == NULL || firmware_node_id == 0U || seq == 0U)
        return TECLA_WIRE_V3_DISPATCH_INVALID_ARGUMENT;
    if (gateway->queue_count == 0U) return TECLA_WIRE_V3_DISPATCH_QUEUE_EMPTY;
    tecla_wire_v3_frame_view_t head;
    const tecla_wire_v3_buffered_telemetry_t *record = &gateway->queue[0];
    if (tecla_wire_v3_decode(record->frame, record->frame_len, &head) != TECLA_WIRE_V3_OK ||
        head.header.firmware_node_id != firmware_node_id || head.header.seq != seq ||
        memcmp(head.header.boot_id, boot_id, 16U) != 0) {
        return TECLA_WIRE_V3_DISPATCH_HEAD_MISMATCH;
    }
    if (gateway->queue_count > 1U) {
        memmove(&gateway->queue[0], &gateway->queue[1],
                (size_t)(gateway->queue_count - 1U) * sizeof(gateway->queue[0]));
    }
    gateway->queue_count--;
    memset(&gateway->queue[gateway->queue_count], 0, sizeof(gateway->queue[0]));
    return TECLA_WIRE_V3_DISPATCH_ACCEPTED;
}
