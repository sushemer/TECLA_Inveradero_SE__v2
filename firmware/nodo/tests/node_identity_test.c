#include <assert.h>
#include <string.h>

#include "../src/storage/node_identity.h"

typedef struct {
    uint8_t record[TECLA_NODE_IDENTITY_RECORD_SIZE];
    size_t length;
    bool present;
    bool fail_read;
    unsigned writes;
} memory_store_t;

static tecla_identity_io_result_t memory_read(void *context, uint8_t *bytes,
                                               size_t capacity, size_t *length)
{
    memory_store_t *store = context;
    if (store->fail_read) return TECLA_IDENTITY_IO_ERROR;
    if (!store->present) return TECLA_IDENTITY_IO_NOT_FOUND;
    *length = store->length;
    if (store->length > capacity) return TECLA_IDENTITY_IO_ERROR;
    memcpy(bytes, store->record, store->length);
    return TECLA_IDENTITY_IO_OK;
}

static tecla_identity_io_result_t memory_write(void *context, const uint8_t *bytes,
                                                size_t length)
{
    memory_store_t *store = context;
    if (length > sizeof(store->record)) return TECLA_IDENTITY_IO_ERROR;
    memcpy(store->record, bytes, length);
    store->length = length;
    store->present = true;
    ++store->writes;
    return TECLA_IDENTITY_IO_OK;
}

int main(void)
{
    const uint8_t golden[TECLA_NODE_IDENTITY_RECORD_SIZE] = {
        'T', 'N', 'I', 'D', 1U, 0x78U, 0x56U, 0x34U, 0x12U,
        0xA5U, 0xDDU, 0xB3U, 0x03U,
    };
    uint8_t encoded[TECLA_NODE_IDENTITY_RECORD_SIZE];
    uint32_t decoded = 0U;
    assert(tecla_node_identity_encode(0U, encoded) == TECLA_IDENTITY_INVALID_ARGUMENT);
    assert(tecla_node_identity_encode(UINT32_C(0x12345678), encoded) == TECLA_IDENTITY_OK);
    assert(memcmp(encoded, golden, sizeof(golden)) == 0);
    assert(tecla_node_identity_decode(golden, sizeof(golden), &decoded) == TECLA_IDENTITY_OK);
    assert(decoded == UINT32_C(0x12345678));
    assert(tecla_node_identity_decode(golden, sizeof(golden) - 1U, &decoded) == TECLA_IDENTITY_CORRUPT);
    memcpy(encoded, golden, sizeof(golden));
    encoded[5] ^= 1U;
    assert(tecla_node_identity_decode(encoded, sizeof(encoded), &decoded) == TECLA_IDENTITY_CORRUPT);

    memory_store_t memory = {0};
    const tecla_identity_store_t store = {
        .context = &memory,
        .read = memory_read,
        .write = memory_write,
    };
    assert(tecla_node_identity_load(&store, &decoded) == TECLA_IDENTITY_NOT_PROVISIONED);
    assert(tecla_node_identity_provision_once(&store, 0U) == TECLA_IDENTITY_INVALID_ARGUMENT);
    assert(tecla_node_identity_provision_once(&store, 77U) == TECLA_IDENTITY_OK);
    assert(memory.writes == 1U);
    assert(tecla_node_identity_provision_once(&store, 77U) == TECLA_IDENTITY_OK);
    assert(memory.writes == 1U);
    assert(tecla_node_identity_provision_once(&store, 78U) == TECLA_IDENTITY_ALREADY_DIFFERENT);
    assert(tecla_node_identity_reprovision(&store, 78U, false) == TECLA_IDENTITY_CONFIRMATION_REQUIRED);
    assert(tecla_node_identity_load(&store, &decoded) == TECLA_IDENTITY_OK && decoded == 77U);
    assert(tecla_node_identity_reprovision(&store, 78U, true) == TECLA_IDENTITY_OK);
    assert(tecla_node_identity_load(&store, &decoded) == TECLA_IDENTITY_OK && decoded == 78U);

    memory.record[4] = 9U;
    assert(tecla_node_identity_load(&store, &decoded) == TECLA_IDENTITY_CORRUPT);
    assert(tecla_node_identity_provision_once(&store, 99U) == TECLA_IDENTITY_CORRUPT);
    assert(memory.writes == 2U);
    memory.fail_read = true;
    assert(tecla_node_identity_load(&store, &decoded) == TECLA_IDENTITY_STORAGE_ERROR);
    return 0;
}
