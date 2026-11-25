#ifndef TECLA_NODE_IDENTITY_CLI_H
#define TECLA_NODE_IDENTITY_CLI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "node_identity.h"

#define TECLA_IDENTITY_CLI_MAX_LINE 48U

typedef enum {
    TECLA_IDENTITY_CLI_COMMAND_STATUS = 0,
    TECLA_IDENTITY_CLI_COMMAND_PROVISION,
    TECLA_IDENTITY_CLI_COMMAND_REPROVISION,
    TECLA_IDENTITY_CLI_COMMAND_CONFIRM_REPROVISION,
    TECLA_IDENTITY_CLI_COMMAND_CANCEL,
} tecla_identity_cli_command_type_t;

typedef struct {
    tecla_identity_cli_command_type_t type;
    uint32_t firmware_node_id;
} tecla_identity_cli_command_t;

typedef enum {
    TECLA_IDENTITY_CLI_OK = 0,
    TECLA_IDENTITY_CLI_CONFIRMATION_REQUIRED,
    TECLA_IDENTITY_CLI_CANCELLED,
    TECLA_IDENTITY_CLI_NO_PENDING_CONFIRMATION,
    TECLA_IDENTITY_CLI_CONFIRMATION_MISMATCH,
    TECLA_IDENTITY_CLI_ALREADY_PROVISIONED,
    TECLA_IDENTITY_CLI_ID_CONFLICT,
    TECLA_IDENTITY_CLI_NOT_PROVISIONED,
    TECLA_IDENTITY_CLI_INVALID_ID,
    TECLA_IDENTITY_CLI_SYNTAX_ERROR,
    TECLA_IDENTITY_CLI_LINE_TOO_LONG,
    TECLA_IDENTITY_CLI_CORRUPT,
    TECLA_IDENTITY_CLI_STORAGE_ERROR,
} tecla_identity_cli_result_t;

typedef struct {
    bool reprovision_pending;
    uint32_t pending_firmware_node_id;
} tecla_identity_cli_state_t;

/* Grammar: status | provision <positive uint32> |
 * reprovision <positive uint32> | confirm-reprovision <same uint32> | cancel.
 * Leading/trailing ASCII whitespace and repeated spaces/tabs are accepted. */
tecla_identity_cli_result_t tecla_identity_cli_parse(
    const char *line, size_t line_length, tecla_identity_cli_command_t *command);

/* Call from one serial-console task only. A pending reprovision confirmation
 * lives in RAM, is cancelled by reset or `cancel`, and never writes NVS until
 * the distinct matching confirm command arrives. */
tecla_identity_cli_result_t tecla_identity_cli_execute(
    tecla_identity_cli_state_t *state,
    const tecla_identity_store_t *store,
    const char *line,
    size_t line_length,
    uint32_t *firmware_node_id_out);

const char *tecla_identity_cli_result_name(tecla_identity_cli_result_t result);

#endif
