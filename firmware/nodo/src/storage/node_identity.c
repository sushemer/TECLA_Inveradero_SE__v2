#include "node_identity.h"

#include <string.h>

#define IDENTITY_MAGIC_0 ((uint8_t)'T')
#define IDENTITY_MAGIC_1 ((uint8_t)'N')
#define IDENTITY_MAGIC_2 ((uint8_t)'I')
#define IDENTITY_MAGIC_3 ((uint8_t)'D')
#define IDENTITY_SCHEMA_VERSION 1U
#define IDENTITY_CRC_OFFSET 9U

static uint32_t crc32_ieee(const uint8_t *data, size_t length)
{
    uint32_t crc = UINT32_C(0xFFFFFFFF);
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8U; ++bit) {
            const uint32_t mask = (uint32_t)-(int32_t)(crc & 1U);
            crc = (crc >> 1U) ^ (UINT32_C(0xEDB88320) & mask);
        }
    }
    return ~crc;
}

static void write_u32_le(uint8_t *output, uint32_t value)
{
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8U);
    output[2] = (uint8_t)(value >> 16U);
    output[3] = (uint8_t)(value >> 24U);
}

static uint32_t read_u32_le(const uint8_t *input)
{
    return (uint32_t)input[0] |
           ((uint32_t)input[1] << 8U) |
           ((uint32_t)input[2] << 16U) |
           ((uint32_t)input[3] << 24U);
}

static bool store_is_valid(const tecla_identity_store_t *store)
{
    return store != NULL && store->read != NULL && store->write != NULL;
}

tecla_identity_result_t tecla_node_identity_encode(
    uint32_t firmware_node_id, uint8_t record[TECLA_NODE_IDENTITY_RECORD_SIZE])
{
    if (record == NULL || firmware_node_id == 0U) {
        return TECLA_IDENTITY_INVALID_ARGUMENT;
    }

    record[0] = IDENTITY_MAGIC_0;
    record[1] = IDENTITY_MAGIC_1;
    record[2] = IDENTITY_MAGIC_2;
    record[3] = IDENTITY_MAGIC_3;
    record[4] = IDENTITY_SCHEMA_VERSION;
    write_u32_le(&record[5], firmware_node_id);
    write_u32_le(&record[IDENTITY_CRC_OFFSET], crc32_ieee(record, IDENTITY_CRC_OFFSET));
    return TECLA_IDENTITY_OK;
}

tecla_identity_result_t tecla_node_identity_decode(
    const uint8_t *record, size_t length, uint32_t *firmware_node_id)
{
    if (record == NULL || firmware_node_id == NULL) {
        return TECLA_IDENTITY_INVALID_ARGUMENT;
    }
    if (length != TECLA_NODE_IDENTITY_RECORD_SIZE ||
        record[0] != IDENTITY_MAGIC_0 || record[1] != IDENTITY_MAGIC_1 ||
        record[2] != IDENTITY_MAGIC_2 || record[3] != IDENTITY_MAGIC_3 ||
        record[4] != IDENTITY_SCHEMA_VERSION ||
        read_u32_le(&record[IDENTITY_CRC_OFFSET]) != crc32_ieee(record, IDENTITY_CRC_OFFSET)) {
        return TECLA_IDENTITY_CORRUPT;
    }

    const uint32_t decoded = read_u32_le(&record[5]);
    if (decoded == 0U) return TECLA_IDENTITY_CORRUPT;
    *firmware_node_id = decoded;
    return TECLA_IDENTITY_OK;
}

tecla_identity_result_t tecla_node_identity_load(
    const tecla_identity_store_t *store, uint32_t *firmware_node_id)
{
    if (!store_is_valid(store) || firmware_node_id == NULL) {
        return TECLA_IDENTITY_INVALID_ARGUMENT;
    }
    uint8_t record[TECLA_NODE_IDENTITY_RECORD_SIZE];
    size_t length = 0U;
    const tecla_identity_io_result_t io =
        store->read(store->context, record, sizeof(record), &length);
    if (io == TECLA_IDENTITY_IO_NOT_FOUND) return TECLA_IDENTITY_NOT_PROVISIONED;
    if (io != TECLA_IDENTITY_IO_OK) return TECLA_IDENTITY_STORAGE_ERROR;
    return tecla_node_identity_decode(record, length, firmware_node_id);
}

tecla_identity_result_t tecla_node_identity_provision_once(
    const tecla_identity_store_t *store, uint32_t firmware_node_id)
{
    if (!store_is_valid(store) || firmware_node_id == 0U) {
        return TECLA_IDENTITY_INVALID_ARGUMENT;
    }
    uint32_t existing = 0U;
    const tecla_identity_result_t loaded = tecla_node_identity_load(store, &existing);
    if (loaded == TECLA_IDENTITY_OK) {
        return existing == firmware_node_id ? TECLA_IDENTITY_OK
                                            : TECLA_IDENTITY_ALREADY_DIFFERENT;
    }
    if (loaded == TECLA_IDENTITY_CORRUPT || loaded == TECLA_IDENTITY_STORAGE_ERROR ||
        loaded == TECLA_IDENTITY_INVALID_ARGUMENT) {
        return loaded;
    }

    uint8_t record[TECLA_NODE_IDENTITY_RECORD_SIZE];
    if (tecla_node_identity_encode(firmware_node_id, record) != TECLA_IDENTITY_OK) {
        return TECLA_IDENTITY_INVALID_ARGUMENT;
    }
    return store->write(store->context, record, sizeof(record)) == TECLA_IDENTITY_IO_OK
        ? TECLA_IDENTITY_OK : TECLA_IDENTITY_STORAGE_ERROR;
}

tecla_identity_result_t tecla_node_identity_reprovision(
    const tecla_identity_store_t *store, uint32_t firmware_node_id,
    bool explicit_physical_confirmation)
{
    if (!store_is_valid(store) || firmware_node_id == 0U) {
        return TECLA_IDENTITY_INVALID_ARGUMENT;
    }
    if (!explicit_physical_confirmation) return TECLA_IDENTITY_CONFIRMATION_REQUIRED;

    uint8_t record[TECLA_NODE_IDENTITY_RECORD_SIZE];
    if (tecla_node_identity_encode(firmware_node_id, record) != TECLA_IDENTITY_OK) {
        return TECLA_IDENTITY_INVALID_ARGUMENT;
    }
    return store->write(store->context, record, sizeof(record)) == TECLA_IDENTITY_IO_OK
        ? TECLA_IDENTITY_OK : TECLA_IDENTITY_STORAGE_ERROR;
}
