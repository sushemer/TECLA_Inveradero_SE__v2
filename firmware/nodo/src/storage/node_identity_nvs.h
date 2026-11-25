#ifndef TECLA_NODE_IDENTITY_NVS_H
#define TECLA_NODE_IDENTITY_NVS_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "node_identity.h"

esp_err_t tecla_node_identity_nvs_load(uint32_t *firmware_node_id, bool *found);
esp_err_t tecla_node_identity_nvs_provision_once(uint32_t firmware_node_id);
esp_err_t tecla_node_identity_nvs_reprovision(uint32_t firmware_node_id,
                                               bool explicit_physical_confirmation);
tecla_identity_store_t tecla_node_identity_nvs_store(void);

#endif
