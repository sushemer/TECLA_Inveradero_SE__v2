#include "tecla_wire_v3.h"

#include <math.h>
#include <float.h>
#include <string.h>

_Static_assert(sizeof(float) == sizeof(uint32_t), "wire v3 requires binary32 float");
_Static_assert(FLT_RADIX == 2, "wire v3 requires binary floating point");

static uint16_t read_u16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t read_u64(const uint8_t *p)
{
    uint64_t value = 0U;
    for (unsigned i = 0U; i < 8U; ++i) value |= (uint64_t)p[i] << (8U * i);
    return value;
}

static void write_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void write_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void write_u64(uint8_t *p, uint64_t v)
{
    for (unsigned i = 0U; i < 8U; ++i) p[i] = (uint8_t)(v >> (8U * i));
}

static float read_f32(const uint8_t *p)
{
    const uint32_t bits = read_u32(p);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static void write_f32(uint8_t *p, float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    write_u32(p, bits);
}

static bool uuid_is_zero(const uint8_t *p)
{
    uint8_t aggregate = 0U;
    for (size_t i = 0; i < 16U; ++i) aggregate |= p[i];
    return aggregate == 0U;
}

static bool uuid_is_rfc4122(const uint8_t *p)
{
    const uint8_t version = (uint8_t)(p[6] >> 4);
    return !uuid_is_zero(p) && version >= 1U && version <= 5U &&
           (p[8] & 0xc0U) == 0x80U;
}

static bool slot_code_valid(uint8_t code)
{
    return code != 0U && code != UINT8_MAX;
}

static bool mode_valid(uint8_t mode)
{
    return mode >= 1U && mode <= 3U;
}

static bool sample_interval_valid(uint16_t seconds)
{
    return seconds == 600U || seconds == 1200U || seconds == 2400U || seconds == 3600U;
}

static bool bytes_are_zero(const uint8_t *p, size_t n)
{
    uint8_t aggregate = 0U;
    for (size_t i = 0U; i < n; ++i) aggregate |= p[i];
    return aggregate == 0U;
}

static bool validate_capabilities(const uint8_t *p, size_t n)
{
    if (n < 1U || p[0] > TECLA_WIRE_V3_MAX_SLOTS) return false;
    size_t offset = 1U;
    uint8_t seen_slots[TECLA_WIRE_V3_MAX_SLOTS] = {0};
    for (uint8_t i = 0U; i < p[0]; ++i) {
        if (n - offset < 2U) return false;
        const uint8_t slot = p[offset++];
        const uint8_t count = p[offset++];
        if (!slot_code_valid(slot) || count == 0U ||
            count > TECLA_WIRE_V3_MAX_MEASUREMENTS || n - offset < count)
            return false;
        for (uint8_t j = 0U; j < i; ++j) if (seen_slots[j] == slot) return false;
        seen_slots[i] = slot;
        uint8_t seen_measurements[TECLA_WIRE_V3_MAX_MEASUREMENTS] = {0};
        for (uint8_t j = 0U; j < count; ++j) {
            const uint8_t measurement = p[offset++];
            if (measurement == 0U || measurement > 7U) return false;
            for (uint8_t k = 0U; k < j; ++k)
                if (seen_measurements[k] == measurement) return false;
            seen_measurements[j] = measurement;
        }
    }
    return offset == n;
}

static bool validate_slots(const uint8_t *p, uint8_t count)
{
    uint8_t seen[TECLA_WIRE_V3_MAX_SLOTS] = {0};
    for (uint8_t i = 0U; i < count; ++i) {
        const uint8_t slot = p[(size_t)i * 3U];
        const uint8_t admin = p[(size_t)i * 3U + 1U];
        const uint8_t enabled = p[(size_t)i * 3U + 2U];
        if (!slot_code_valid(slot) || admin < 1U || admin > 3U || enabled > 1U)
            return false;
        for (uint8_t j = 0U; j < i; ++j) if (seen[j] == slot) return false;
        seen[i] = slot;
    }
    return true;
}

static bool validate_payload(uint8_t type, const uint8_t *p, size_t n)
{
    if (p == NULL && n != 0U) return false;
    switch (type) {
    case TECLA_WIRE_V3_CAPABILITIES:
        return validate_capabilities(p, n);
    case TECLA_WIRE_V3_HEARTBEAT:
        return n == 86U && mode_valid(p[4]) && p[5] <= 1U &&
               (p[5] == 0U ? uuid_is_zero(p + 6U) : uuid_is_rfc4122(p + 6U)) &&
               (bytes_are_zero(p + 22U, 32U) == bytes_are_zero(p + 54U, 32U));
    case TECLA_WIRE_V3_TELEMETRY: {
        if (n < 83U || !mode_valid(p[4]) || p[5] > 5U || p[16] != 0U ||
            !sample_interval_valid(read_u16(p + 6U)) ||
            (p[17] > 100U && p[17] != UINT8_MAX) || p[18] > TECLA_WIRE_V3_MAX_READINGS)
            return false;
        const uint8_t count = p[18];
        if (n != 83U + (size_t)count * 7U) return false;
        for (uint8_t i = 0U; i < count; ++i) {
            const uint8_t *r = p + 83U + (size_t)i * 7U;
            const float value = read_f32(r + 3U);
            if (!slot_code_valid(r[0]) || r[1] == 0U || r[1] > 7U ||
                r[2] < 1U || r[2] > 5U || !isfinite(value)) return false;
            for (uint8_t j = 0U; j < i; ++j) {
                const uint8_t *previous = p + 83U + (size_t)j * 7U;
                if (previous[0] == r[0] && previous[1] == r[1]) return false;
            }
        }
        return true;
    }
    case TECLA_WIRE_V3_TELEMETRY_ACK:
        return n == 2U && p[0] >= 1U && p[0] <= 3U &&
               ((p[0] == 3U) == (p[1] != 0U));
    case TECLA_WIRE_V3_COMMAND_CONFIG: {
        if (n < 96U) return false;
        const uint8_t count = p[31];
        return count <= TECLA_WIRE_V3_MAX_SLOTS && n == 96U + (size_t)count * 3U &&
               uuid_is_rfc4122(p) && read_u32(p + 20U) > read_u32(p + 16U) &&
               mode_valid(p[24]) && sample_interval_valid(read_u16(p + 25U)) &&
               read_u32(p + 27U) >= 60U && read_u32(p + 27U) <= 86400U &&
               validate_slots(p + 96U, count);
    }
    case TECLA_WIRE_V3_COMMAND_ACK: {
        if (n != 30U || !uuid_is_rfc4122(p) || p[28] < 1U || p[28] > 3U) return false;
        const uint32_t expected = read_u32(p + 16U);
        const uint32_t target = read_u32(p + 20U);
        const uint32_t applied = read_u32(p + 24U);
        if (target <= expected) return false;
        if (p[28] == 1U) return p[29] == 0U;
        if (p[28] == 2U) return applied == target && p[29] == 0U;
        return p[29] != 0U;
    }
    case TECLA_WIRE_V3_MANIFEST_CONFIG: {
        if (n < 72U) return false;
        const uint8_t count = p[7];
        return count <= TECLA_WIRE_V3_MAX_SLOTS && n == 72U + (size_t)count * 3U &&
               mode_valid(p[0]) && sample_interval_valid(read_u16(p + 1U)) &&
               read_u32(p + 3U) >= 60U && read_u32(p + 3U) <= 86400U &&
               validate_slots(p + 72U, count);
    }
    case TECLA_WIRE_V3_MANIFEST_ACK:
        return n == 66U && p[64] >= 1U && p[64] <= 2U &&
               ((p[64] == 1U) == (p[65] == 0U));
    default:
        return false;
    }
}

static void encode_header(const tecla_wire_v3_header_t *h, uint16_t len, uint8_t *out)
{
    out[0] = TECLA_WIRE_V3_VERSION;
    out[1] = h->type;
    write_u16(out + 2U, len);
    write_u32(out + 4U, h->firmware_node_id);
    memcpy(out + 8U, h->boot_id, 16U);
    write_u32(out + 24U, h->seq);
}

tecla_wire_v3_status_t tecla_wire_v3_encode(
    const tecla_wire_v3_header_t *header, const uint8_t *payload,
    size_t payload_len, uint8_t *out, size_t out_capacity, size_t *out_len)
{
    if (header == NULL || out == NULL || out_len == NULL) return TECLA_WIRE_V3_ERR_ARGUMENT;
    *out_len = 0U;
    if (header->firmware_node_id == 0U || header->seq == 0U || header->seq == UINT32_MAX ||
        !uuid_is_rfc4122(header->boot_id)) return TECLA_WIRE_V3_ERR_FIELD;
    if (header->type < TECLA_WIRE_V3_CAPABILITIES ||
        header->type > TECLA_WIRE_V3_MANIFEST_ACK) return TECLA_WIRE_V3_ERR_TYPE;
    if (payload_len > TECLA_WIRE_V3_MAX_FRAME_SIZE - TECLA_WIRE_V3_HEADER_SIZE ||
        payload_len > UINT16_MAX - TECLA_WIRE_V3_HEADER_SIZE)
        return TECLA_WIRE_V3_ERR_LENGTH;
    if (!validate_payload(header->type, payload, payload_len)) return TECLA_WIRE_V3_ERR_FIELD;
    const size_t total = TECLA_WIRE_V3_HEADER_SIZE + payload_len;
    if (total > TECLA_WIRE_V3_MAX_FRAME_SIZE) return TECLA_WIRE_V3_ERR_LENGTH;
    if (out_capacity < total) return TECLA_WIRE_V3_ERR_CAPACITY;
    encode_header(header, (uint16_t)total, out);
    if (payload_len != 0U) memcpy(out + TECLA_WIRE_V3_HEADER_SIZE, payload, payload_len);
    *out_len = total;
    return TECLA_WIRE_V3_OK;
}

tecla_wire_v3_status_t tecla_wire_v3_decode(
    const uint8_t *frame, size_t frame_len, tecla_wire_v3_frame_view_t *out)
{
    if (frame == NULL || out == NULL) return TECLA_WIRE_V3_ERR_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (frame_len < TECLA_WIRE_V3_HEADER_SIZE || frame_len > TECLA_WIRE_V3_MAX_FRAME_SIZE)
        return TECLA_WIRE_V3_ERR_LENGTH;
    if (frame[0] != TECLA_WIRE_V3_VERSION) return TECLA_WIRE_V3_ERR_VERSION;
    if (frame[1] < TECLA_WIRE_V3_CAPABILITIES ||
        frame[1] > TECLA_WIRE_V3_MANIFEST_ACK) return TECLA_WIRE_V3_ERR_TYPE;
    if (read_u16(frame + 2U) != frame_len) return TECLA_WIRE_V3_ERR_LENGTH;
    if (read_u32(frame + 4U) == 0U || !uuid_is_rfc4122(frame + 8U) ||
        read_u32(frame + 24U) == 0U || read_u32(frame + 24U) == UINT32_MAX)
        return TECLA_WIRE_V3_ERR_FIELD;
    const uint8_t *payload = frame + TECLA_WIRE_V3_HEADER_SIZE;
    const size_t payload_len = frame_len - TECLA_WIRE_V3_HEADER_SIZE;
    if (!validate_payload(frame[1], payload, payload_len)) return TECLA_WIRE_V3_ERR_FIELD;
    out->header.type = frame[1];
    out->header.firmware_node_id = read_u32(frame + 4U);
    memcpy(out->header.boot_id, frame + 8U, 16U);
    out->header.seq = read_u32(frame + 24U);
    out->payload = payload;
    out->payload_len = payload_len;
    return TECLA_WIRE_V3_OK;
}

static void encode_slots(const tecla_wire_v3_slot_t *slots, uint8_t count,
                         uint8_t *payload, size_t start)
{
    for (uint8_t i = 0U; i < count; ++i) {
        const size_t offset = start + (size_t)i * 3U;
        payload[offset] = slots[i].slot_code;
        payload[offset + 1U] = slots[i].estado_administrativo;
        payload[offset + 2U] = slots[i].enabled;
    }
}

static void decode_slots(const uint8_t *payload, uint8_t count,
                         tecla_wire_v3_slot_t *slots, size_t start)
{
    for (uint8_t i = 0U; i < count; ++i) {
        const size_t offset = start + (size_t)i * 3U;
        slots[i].slot_code = payload[offset];
        slots[i].estado_administrativo = payload[offset + 1U];
        slots[i].enabled = payload[offset + 2U];
    }
}

tecla_wire_v3_status_t tecla_wire_v3_encode_typed(
    const tecla_wire_v3_header_t *header, const tecla_wire_v3_message_t *message,
    uint8_t *out, size_t out_capacity, size_t *out_len)
{
    if (header == NULL || message == NULL || out == NULL || out_len == NULL)
        return TECLA_WIRE_V3_ERR_ARGUMENT;
    *out_len = 0U;
    uint8_t payload[TECLA_WIRE_V3_MAX_FRAME_SIZE - TECLA_WIRE_V3_HEADER_SIZE] = {0};
    size_t payload_len = 0U;
    switch (header->type) {
    case TECLA_WIRE_V3_CAPABILITIES: {
        const tecla_wire_v3_capabilities_t *m = &message->capabilities;
        if (m->slot_count > TECLA_WIRE_V3_MAX_SLOTS) return TECLA_WIRE_V3_ERR_FIELD;
        payload[0] = m->slot_count;
        size_t offset = 1U;
        for (uint8_t i = 0U; i < m->slot_count; ++i) {
            const tecla_wire_v3_capability_t *slot = &m->slots[i];
            if (slot->measurement_count > TECLA_WIRE_V3_MAX_MEASUREMENTS)
                return TECLA_WIRE_V3_ERR_FIELD;
            payload[offset++] = slot->slot_code;
            payload[offset++] = slot->measurement_count;
            memcpy(payload + offset, slot->measurement_codes, slot->measurement_count);
            offset += slot->measurement_count;
        }
        payload_len = offset;
        break;
    }
    case TECLA_WIRE_V3_HEARTBEAT: {
        const tecla_wire_v3_heartbeat_t *m = &message->heartbeat;
        write_u32(payload, m->command_config_revision_applied);
        payload[4] = m->effective_mode;
        payload[5] = m->last_command_present ? 1U : 0U;
        memcpy(payload + 6U, m->last_applied_command_id, 16U);
        memcpy(payload + 22U, m->manifest_revision_applied, 32U);
        memcpy(payload + 54U, m->assignment_revision_applied, 32U);
        payload_len = 86U;
        break;
    }
    case TECLA_WIRE_V3_TELEMETRY: {
        const tecla_wire_v3_telemetry_t *m = &message->telemetry;
        if (m->reading_count > TECLA_WIRE_V3_MAX_READINGS) return TECLA_WIRE_V3_ERR_FIELD;
        write_u32(payload, m->command_config_revision_applied);
        payload[4] = m->effective_mode;
        payload[5] = m->node_health;
        write_u16(payload + 6U, m->sample_interval_s);
        write_u64(payload + 8U, m->sample_uptime_ms);
        payload[16] = m->time_quality;
        payload[17] = m->battery_percent;
        payload[18] = m->reading_count;
        memcpy(payload + 19U, m->manifest_revision, 32U);
        memcpy(payload + 51U, m->assignment_revision, 32U);
        for (uint8_t i = 0U; i < m->reading_count; ++i) {
            const size_t offset = 83U + (size_t)i * 7U;
            payload[offset] = m->readings[i].slot_code;
            payload[offset + 1U] = m->readings[i].measurement_code;
            payload[offset + 2U] = m->readings[i].sensor_health;
            write_f32(payload + offset + 3U, m->readings[i].value);
        }
        payload_len = 83U + (size_t)m->reading_count * 7U;
        break;
    }
    case TECLA_WIRE_V3_TELEMETRY_ACK:
        payload[0] = message->telemetry_ack.scope;
        payload[1] = message->telemetry_ack.reason;
        payload_len = 2U;
        break;
    case TECLA_WIRE_V3_COMMAND_CONFIG: {
        const tecla_wire_v3_command_config_t *m = &message->command_config;
        if (m->slot_count > TECLA_WIRE_V3_MAX_SLOTS) return TECLA_WIRE_V3_ERR_FIELD;
        memcpy(payload, m->command_id, 16U);
        write_u32(payload + 16U, m->command_config_revision_expected);
        write_u32(payload + 20U, m->command_config_revision_target);
        payload[24] = m->effective_mode;
        write_u16(payload + 25U, m->sample_interval_s);
        write_u32(payload + 27U, m->management_poll_interval_s);
        payload[31] = m->slot_count;
        memcpy(payload + 32U, m->manifest_revision, 32U);
        memcpy(payload + 64U, m->assignment_revision, 32U);
        encode_slots(m->slots, m->slot_count, payload, 96U);
        payload_len = 96U + (size_t)m->slot_count * 3U;
        break;
    }
    case TECLA_WIRE_V3_COMMAND_ACK: {
        const tecla_wire_v3_command_ack_t *m = &message->command_ack;
        memcpy(payload, m->command_id, 16U);
        write_u32(payload + 16U, m->command_config_revision_expected);
        write_u32(payload + 20U, m->command_config_revision_target);
        write_u32(payload + 24U, m->command_config_revision_applied);
        payload[28] = m->stage;
        payload[29] = m->reason;
        payload_len = 30U;
        break;
    }
    case TECLA_WIRE_V3_MANIFEST_CONFIG: {
        const tecla_wire_v3_manifest_config_t *m = &message->manifest_config;
        if (m->slot_count > TECLA_WIRE_V3_MAX_SLOTS) return TECLA_WIRE_V3_ERR_FIELD;
        payload[0] = m->effective_mode;
        write_u16(payload + 1U, m->sample_interval_s);
        write_u32(payload + 3U, m->management_poll_interval_s);
        payload[7] = m->slot_count;
        memcpy(payload + 8U, m->manifest_revision, 32U);
        memcpy(payload + 40U, m->assignment_revision, 32U);
        encode_slots(m->slots, m->slot_count, payload, 72U);
        payload_len = 72U + (size_t)m->slot_count * 3U;
        break;
    }
    case TECLA_WIRE_V3_MANIFEST_ACK:
        memcpy(payload, message->manifest_ack.manifest_revision, 32U);
        memcpy(payload + 32U, message->manifest_ack.assignment_revision, 32U);
        payload[64] = message->manifest_ack.stage;
        payload[65] = message->manifest_ack.reason;
        payload_len = 66U;
        break;
    default:
        return TECLA_WIRE_V3_ERR_TYPE;
    }
    return tecla_wire_v3_encode(header, payload, payload_len, out, out_capacity, out_len);
}

tecla_wire_v3_status_t tecla_wire_v3_decode_typed(
    const uint8_t *frame, size_t frame_len, tecla_wire_v3_header_t *header_out,
    tecla_wire_v3_message_t *message_out)
{
    if (header_out == NULL || message_out == NULL) return TECLA_WIRE_V3_ERR_ARGUMENT;
    tecla_wire_v3_frame_view_t view;
    const tecla_wire_v3_status_t status = tecla_wire_v3_decode(frame, frame_len, &view);
    if (status != TECLA_WIRE_V3_OK) return status;
    memset(message_out, 0, sizeof(*message_out));
    *header_out = view.header;
    const uint8_t *p = view.payload;
    switch (view.header.type) {
    case TECLA_WIRE_V3_CAPABILITIES: {
        tecla_wire_v3_capabilities_t *m = &message_out->capabilities;
        m->slot_count = p[0];
        size_t offset = 1U;
        for (uint8_t i = 0U; i < m->slot_count; ++i) {
            tecla_wire_v3_capability_t *slot = &m->slots[i];
            slot->slot_code = p[offset++];
            slot->measurement_count = p[offset++];
            memcpy(slot->measurement_codes, p + offset, slot->measurement_count);
            offset += slot->measurement_count;
        }
        break;
    }
    case TECLA_WIRE_V3_HEARTBEAT: {
        tecla_wire_v3_heartbeat_t *m = &message_out->heartbeat;
        m->command_config_revision_applied = read_u32(p);
        m->effective_mode = p[4];
        m->last_command_present = p[5] != 0U;
        memcpy(m->last_applied_command_id, p + 6U, 16U);
        memcpy(m->manifest_revision_applied, p + 22U, 32U);
        memcpy(m->assignment_revision_applied, p + 54U, 32U);
        break;
    }
    case TECLA_WIRE_V3_TELEMETRY: {
        tecla_wire_v3_telemetry_t *m = &message_out->telemetry;
        m->command_config_revision_applied = read_u32(p);
        m->effective_mode = p[4];
        m->node_health = p[5];
        m->sample_interval_s = read_u16(p + 6U);
        m->sample_uptime_ms = read_u64(p + 8U);
        m->time_quality = p[16];
        m->battery_percent = p[17];
        m->reading_count = p[18];
        memcpy(m->manifest_revision, p + 19U, 32U);
        memcpy(m->assignment_revision, p + 51U, 32U);
        for (uint8_t i = 0U; i < m->reading_count; ++i) {
            const size_t offset = 83U + (size_t)i * 7U;
            m->readings[i].slot_code = p[offset];
            m->readings[i].measurement_code = p[offset + 1U];
            m->readings[i].sensor_health = p[offset + 2U];
            m->readings[i].value = read_f32(p + offset + 3U);
        }
        break;
    }
    case TECLA_WIRE_V3_TELEMETRY_ACK:
        message_out->telemetry_ack.scope = p[0];
        message_out->telemetry_ack.reason = p[1];
        break;
    case TECLA_WIRE_V3_COMMAND_CONFIG: {
        tecla_wire_v3_command_config_t *m = &message_out->command_config;
        memcpy(m->command_id, p, 16U);
        m->command_config_revision_expected = read_u32(p + 16U);
        m->command_config_revision_target = read_u32(p + 20U);
        m->effective_mode = p[24];
        m->sample_interval_s = read_u16(p + 25U);
        m->management_poll_interval_s = read_u32(p + 27U);
        m->slot_count = p[31];
        memcpy(m->manifest_revision, p + 32U, 32U);
        memcpy(m->assignment_revision, p + 64U, 32U);
        decode_slots(p, m->slot_count, m->slots, 96U);
        break;
    }
    case TECLA_WIRE_V3_COMMAND_ACK: {
        tecla_wire_v3_command_ack_t *m = &message_out->command_ack;
        memcpy(m->command_id, p, 16U);
        m->command_config_revision_expected = read_u32(p + 16U);
        m->command_config_revision_target = read_u32(p + 20U);
        m->command_config_revision_applied = read_u32(p + 24U);
        m->stage = p[28];
        m->reason = p[29];
        break;
    }
    case TECLA_WIRE_V3_MANIFEST_CONFIG: {
        tecla_wire_v3_manifest_config_t *m = &message_out->manifest_config;
        m->effective_mode = p[0];
        m->sample_interval_s = read_u16(p + 1U);
        m->management_poll_interval_s = read_u32(p + 3U);
        m->slot_count = p[7];
        memcpy(m->manifest_revision, p + 8U, 32U);
        memcpy(m->assignment_revision, p + 40U, 32U);
        decode_slots(p, m->slot_count, m->slots, 72U);
        break;
    }
    case TECLA_WIRE_V3_MANIFEST_ACK:
        memcpy(message_out->manifest_ack.manifest_revision, p, 32U);
        memcpy(message_out->manifest_ack.assignment_revision, p + 32U, 32U);
        message_out->manifest_ack.stage = p[64];
        message_out->manifest_ack.reason = p[65];
        break;
    default:
        return TECLA_WIRE_V3_ERR_TYPE;
    }
    return TECLA_WIRE_V3_OK;
}

static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

tecla_wire_v3_status_t tecla_wire_v3_decode_sha256_hex(
    const char *hex, size_t hex_len, uint8_t out[32])
{
    if (hex == NULL || out == NULL) return TECLA_WIRE_V3_ERR_ARGUMENT;
    if (hex_len != 64U) return TECLA_WIRE_V3_ERR_LENGTH;
    for (size_t i = 0U; i < 32U; ++i) {
        const int hi = hex_nibble(hex[i * 2U]);
        const int lo = hex_nibble(hex[i * 2U + 1U]);
        if (hi < 0 || lo < 0) return TECLA_WIRE_V3_ERR_FIELD;
        out[i] = (uint8_t)(((unsigned)hi << 4) | (unsigned)lo);
    }
    return TECLA_WIRE_V3_OK;
}

tecla_wire_v3_status_t tecla_wire_v3_encode_sha256_hex(
    const uint8_t hash[32], char out[65])
{
    static const char digits[] = "0123456789abcdef";
    if (hash == NULL || out == NULL) return TECLA_WIRE_V3_ERR_ARGUMENT;
    for (size_t i = 0U; i < 32U; ++i) {
        out[i * 2U] = digits[hash[i] >> 4];
        out[i * 2U + 1U] = digits[hash[i] & 0x0fU];
    }
    out[64] = '\0';
    return TECLA_WIRE_V3_OK;
}

const char *tecla_wire_v3_status_name(tecla_wire_v3_status_t status)
{
    static const char *const names[] = {
        "ok", "argument", "capacity", "length", "version", "type", "field"
    };
    const size_t count = sizeof(names) / sizeof(names[0]);
    return (unsigned)status < count ? names[status] : "unknown";
}
