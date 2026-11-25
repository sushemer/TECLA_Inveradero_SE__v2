#include "../src/storage/node_identity_cli.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint8_t record[TECLA_NODE_IDENTITY_RECORD_SIZE];
    size_t length;
    bool present;
    unsigned writes;
} mock_store_t;

static tecla_identity_io_result_t mock_read(void *context, uint8_t *bytes,
                                             size_t capacity, size_t *length)
{
    mock_store_t *mock = context;
    if (!mock->present) return TECLA_IDENTITY_IO_NOT_FOUND;
    if (capacity < mock->length) return TECLA_IDENTITY_IO_ERROR;
    memcpy(bytes, mock->record, mock->length);
    *length = mock->length;
    return TECLA_IDENTITY_IO_OK;
}

static tecla_identity_io_result_t mock_write(void *context, const uint8_t *bytes,
                                              size_t length)
{
    mock_store_t *mock = context;
    if (length != sizeof(mock->record)) return TECLA_IDENTITY_IO_ERROR;
    memcpy(mock->record, bytes, length);
    mock->length = length;
    mock->present = true;
    mock->writes++;
    return TECLA_IDENTITY_IO_OK;
}

static tecla_identity_cli_result_t run(tecla_identity_cli_state_t *state,
                                        const tecla_identity_store_t *store,
                                        const char *command,
                                        uint32_t *id_out)
{
    return tecla_identity_cli_execute(state, store, command, strlen(command), id_out);
}

int tecla_identity_cli_test_run(void)
{
    tecla_identity_cli_command_t parsed;
    assert(tecla_identity_cli_parse(" \tprovision   42\r\n", 18U, &parsed) ==
           TECLA_IDENTITY_CLI_OK);
    assert(parsed.type == TECLA_IDENTITY_CLI_COMMAND_PROVISION &&
           parsed.firmware_node_id == 42U);
    assert(tecla_identity_cli_parse("provision +42", 13U, &parsed) ==
           TECLA_IDENTITY_CLI_SYNTAX_ERROR);
    assert(tecla_identity_cli_parse("provision 4x", 12U, &parsed) ==
           TECLA_IDENTITY_CLI_SYNTAX_ERROR);
    assert(tecla_identity_cli_parse("provision 4294967296", 20U, &parsed) ==
           TECLA_IDENTITY_CLI_SYNTAX_ERROR);
    assert(tecla_identity_cli_parse("provision 42 extra", 18U, &parsed) ==
           TECLA_IDENTITY_CLI_SYNTAX_ERROR);
    char too_long[TECLA_IDENTITY_CLI_MAX_LINE + 1U];
    memset(too_long, ' ', sizeof(too_long));
    assert(tecla_identity_cli_parse(too_long, sizeof(too_long), &parsed) ==
           TECLA_IDENTITY_CLI_LINE_TOO_LONG);

    mock_store_t mock = {0};
    const tecla_identity_store_t store = {
        .context = &mock, .read = mock_read, .write = mock_write,
    };
    tecla_identity_cli_state_t state = {0};
    uint32_t id = 0U;
    assert(run(&state, &store, "provision 0", &id) == TECLA_IDENTITY_CLI_INVALID_ID);
    assert(!mock.present && mock.writes == 0U);
    assert(run(&state, &store, "provision 4294967295", &id) == TECLA_IDENTITY_CLI_OK);
    assert(mock.writes == 1U);
    assert(tecla_node_identity_load(&store, &id) == TECLA_IDENTITY_OK && id == UINT32_MAX);
    assert(run(&state, &store, "provision 4294967295", &id) ==
           TECLA_IDENTITY_CLI_ALREADY_PROVISIONED);
    assert(run(&state, &store, "provision 7", &id) ==
           TECLA_IDENTITY_CLI_ID_CONFLICT);
    assert(mock.writes == 1U);

    assert(run(&state, &store, "reprovision 7", &id) ==
           TECLA_IDENTITY_CLI_CONFIRMATION_REQUIRED);
    assert(state.reprovision_pending && state.pending_firmware_node_id == 7U);
    assert(run(&state, &store, "confirm-reprovision 8", &id) ==
           TECLA_IDENTITY_CLI_CONFIRMATION_MISMATCH);
    assert(mock.writes == 1U && state.reprovision_pending);
    assert(run(&state, &store, "cancel", &id) == TECLA_IDENTITY_CLI_CANCELLED);
    assert(!state.reprovision_pending && mock.writes == 1U);
    assert(run(&state, &store, "confirm-reprovision 7", &id) ==
           TECLA_IDENTITY_CLI_NO_PENDING_CONFIRMATION);
    assert(run(&state, &store, "reprovision 7", &id) ==
           TECLA_IDENTITY_CLI_CONFIRMATION_REQUIRED);
    assert(run(&state, &store, "confirm-reprovision 7", &id) == TECLA_IDENTITY_CLI_OK);
    assert(mock.writes == 2U && id == 7U && !state.reprovision_pending);
    assert(tecla_node_identity_load(&store, &id) == TECLA_IDENTITY_OK && id == 7U);

    mock_store_t empty = {0};
    const tecla_identity_store_t empty_store = {
        .context = &empty, .read = mock_read, .write = mock_write,
    };
    assert(run(&state, &empty_store, "reprovision 9", &id) ==
           TECLA_IDENTITY_CLI_NOT_PROVISIONED);
    puts("PASS node_identity_cli_parse_provision_reprovision_cancel");
    return 0;
}

int main(void)
{
    return tecla_identity_cli_test_run();
}
