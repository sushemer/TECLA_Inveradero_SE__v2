#include "node_identity_nvs.h"

#include "nvs.h"

#include "node_identity.h"

#define IDENTITY_NAMESPACE "tecla_identity"
#define IDENTITY_KEY "node_id_v1"

static tecla_identity_io_result_t nvs_read_identity(void *context,
                                                     uint8_t *bytes,
                                                     size_t capacity,
                                                     size_t *length)
{
    (void)context;
    if (bytes == NULL || length == NULL) return TECLA_IDENTITY_IO_ERROR;
    nvs_handle_t handle;
    esp_err_t err = nvs_open(IDENTITY_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) return TECLA_IDENTITY_IO_NOT_FOUND;
    if (err != ESP_OK) return TECLA_IDENTITY_IO_ERROR;

    size_t stored_length = 0U;
    err = nvs_get_blob(handle, IDENTITY_KEY, NULL, &stored_length);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(handle);
        return TECLA_IDENTITY_IO_NOT_FOUND;
    }
    if (err != ESP_OK) {
        nvs_close(handle);
        return TECLA_IDENTITY_IO_ERROR;
    }
    *length = stored_length;
    if (stored_length > capacity) {
        nvs_close(handle);
        return TECLA_IDENTITY_IO_OK;
    }
    err = nvs_get_blob(handle, IDENTITY_KEY, bytes, &stored_length);
    nvs_close(handle);
    return err == ESP_OK ? TECLA_IDENTITY_IO_OK : TECLA_IDENTITY_IO_ERROR;
}

static tecla_identity_io_result_t nvs_write_identity(void *context,
                                                      const uint8_t *bytes,
                                                      size_t length)
{
    (void)context;
    if (bytes == NULL || length != TECLA_NODE_IDENTITY_RECORD_SIZE) {
        return TECLA_IDENTITY_IO_ERROR;
    }
    nvs_handle_t handle;
    esp_err_t err = nvs_open(IDENTITY_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return TECLA_IDENTITY_IO_ERROR;
    err = nvs_set_blob(handle, IDENTITY_KEY, bytes, length);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return err == ESP_OK ? TECLA_IDENTITY_IO_OK : TECLA_IDENTITY_IO_ERROR;
}

static tecla_identity_store_t identity_store(void)
{
    return (tecla_identity_store_t){
        .context = NULL,
        .read = nvs_read_identity,
        .write = nvs_write_identity,
    };
}

tecla_identity_store_t tecla_node_identity_nvs_store(void)
{
    return identity_store();
}

static esp_err_t map_result(tecla_identity_result_t result)
{
    switch (result) {
    case TECLA_IDENTITY_OK: return ESP_OK;
    case TECLA_IDENTITY_INVALID_ARGUMENT: return ESP_ERR_INVALID_ARG;
    case TECLA_IDENTITY_CORRUPT: return ESP_ERR_INVALID_CRC;
    case TECLA_IDENTITY_ALREADY_DIFFERENT:
    case TECLA_IDENTITY_CONFIRMATION_REQUIRED: return ESP_ERR_INVALID_STATE;
    case TECLA_IDENTITY_NOT_PROVISIONED:
    case TECLA_IDENTITY_STORAGE_ERROR:
    default: return ESP_FAIL;
    }
}

esp_err_t tecla_node_identity_nvs_load(uint32_t *firmware_node_id, bool *found)
{
    if (firmware_node_id == NULL || found == NULL) return ESP_ERR_INVALID_ARG;
    *found = false;
    const tecla_identity_store_t store = identity_store();
    const tecla_identity_result_t result =
        tecla_node_identity_load(&store, firmware_node_id);
    if (result == TECLA_IDENTITY_NOT_PROVISIONED) return ESP_OK;
    if (result == TECLA_IDENTITY_OK) *found = true;
    return map_result(result);
}

esp_err_t tecla_node_identity_nvs_provision_once(uint32_t firmware_node_id)
{
    const tecla_identity_store_t store = identity_store();
    return map_result(tecla_node_identity_provision_once(&store, firmware_node_id));
}

esp_err_t tecla_node_identity_nvs_reprovision(
    uint32_t firmware_node_id, bool explicit_physical_confirmation)
{
    const tecla_identity_store_t store = identity_store();
    return map_result(tecla_node_identity_reprovision(
        &store, firmware_node_id, explicit_physical_confirmation));
}
