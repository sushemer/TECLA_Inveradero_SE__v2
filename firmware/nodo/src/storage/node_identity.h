#ifndef TECLA_NODE_IDENTITY_H
#define TECLA_NODE_IDENTITY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECLA_NODE_IDENTITY_RECORD_SIZE 13U

typedef enum {
    TECLA_IDENTITY_IO_OK = 0,
    TECLA_IDENTITY_IO_NOT_FOUND,
    TECLA_IDENTITY_IO_ERROR,
} tecla_identity_io_result_t;

typedef enum {
    TECLA_IDENTITY_OK = 0,
    TECLA_IDENTITY_NOT_PROVISIONED,
    TECLA_IDENTITY_INVALID_ARGUMENT,
    TECLA_IDENTITY_CORRUPT,
    TECLA_IDENTITY_ALREADY_DIFFERENT,
    TECLA_IDENTITY_CONFIRMATION_REQUIRED,
    TECLA_IDENTITY_STORAGE_ERROR,
} tecla_identity_result_t;

typedef struct {
    void *context;
    tecla_identity_io_result_t (*read)(void *context, uint8_t *bytes,
                                      size_t capacity, size_t *length);
    tecla_identity_io_result_t (*write)(void *context, const uint8_t *bytes,
                                       size_t length);
} tecla_identity_store_t;

/* The record uses magic "TNID", schema 1, uint32 LE and CRC-32/IEEE. */
tecla_identity_result_t tecla_node_identity_encode(
    uint32_t firmware_node_id, uint8_t record[TECLA_NODE_IDENTITY_RECORD_SIZE]);
tecla_identity_result_t tecla_node_identity_decode(
    const uint8_t *record, size_t length, uint32_t *firmware_node_id);
tecla_identity_result_t tecla_node_identity_load(
    const tecla_identity_store_t *store, uint32_t *firmware_node_id);
tecla_identity_result_t tecla_node_identity_provision_once(
    const tecla_identity_store_t *store, uint32_t firmware_node_id);
tecla_identity_result_t tecla_node_identity_reprovision(
    const tecla_identity_store_t *store, uint32_t firmware_node_id,
    bool explicit_physical_confirmation);

#endif
