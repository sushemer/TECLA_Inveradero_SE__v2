#include "node_identity_cli.h"

#include <stdbool.h>
#include <string.h>

static bool is_space(char value)
{
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

static bool parse_positive_u32(const char *text, size_t length, uint32_t *value_out)
{
    if (text == NULL || value_out == NULL || length == 0U) return false;
    uint32_t value = 0U;
    for (size_t i = 0U; i < length; ++i) {
        const char digit_char = text[i];
        if (digit_char < '0' || digit_char > '9') return false;
        const uint32_t digit = (uint32_t)(digit_char - '0');
        if (value > (UINT32_MAX - digit) / 10U) return false;
        value = value * 10U + digit;
    }
    *value_out = value;
    return true;
}

static bool token_equals(const char *token, size_t length, const char *expected)
{
    return strlen(expected) == length && memcmp(token, expected, length) == 0;
}

tecla_identity_cli_result_t tecla_identity_cli_parse(
    const char *line, size_t line_length, tecla_identity_cli_command_t *command)
{
    if (line == NULL || command == NULL) return TECLA_IDENTITY_CLI_SYNTAX_ERROR;
    if (line_length > TECLA_IDENTITY_CLI_MAX_LINE) return TECLA_IDENTITY_CLI_LINE_TOO_LONG;

    size_t begin = 0U;
    size_t end = line_length;
    while (begin < end && is_space(line[begin])) begin++;
    while (end > begin && is_space(line[end - 1U])) end--;
    if (begin == end) return TECLA_IDENTITY_CLI_SYNTAX_ERROR;

    size_t verb_end = begin;
    while (verb_end < end && !is_space(line[verb_end])) verb_end++;
    const size_t verb_len = verb_end - begin;
    if (token_equals(line + begin, verb_len, "status")) {
        if (verb_end != end) return TECLA_IDENTITY_CLI_SYNTAX_ERROR;
        command->type = TECLA_IDENTITY_CLI_COMMAND_STATUS;
        command->firmware_node_id = 0U;
        return TECLA_IDENTITY_CLI_OK;
    }
    if (token_equals(line + begin, verb_len, "cancel")) {
        if (verb_end != end) return TECLA_IDENTITY_CLI_SYNTAX_ERROR;
        command->type = TECLA_IDENTITY_CLI_COMMAND_CANCEL;
        command->firmware_node_id = 0U;
        return TECLA_IDENTITY_CLI_OK;
    }

    while (verb_end < end && is_space(line[verb_end])) verb_end++;
    if (verb_end == end) return TECLA_IDENTITY_CLI_SYNTAX_ERROR;
    size_t id_end = verb_end;
    while (id_end < end && !is_space(line[id_end])) id_end++;
    for (size_t i = id_end; i < end; ++i) {
        if (!is_space(line[i])) return TECLA_IDENTITY_CLI_SYNTAX_ERROR;
    }
    uint32_t id = 0U;
    if (!parse_positive_u32(line + verb_end, id_end - verb_end, &id))
        return TECLA_IDENTITY_CLI_SYNTAX_ERROR;

    if (token_equals(line + begin, verb_len, "provision")) {
        command->type = TECLA_IDENTITY_CLI_COMMAND_PROVISION;
    } else if (token_equals(line + begin, verb_len, "reprovision")) {
        command->type = TECLA_IDENTITY_CLI_COMMAND_REPROVISION;
    } else if (token_equals(line + begin, verb_len, "confirm-reprovision")) {
        command->type = TECLA_IDENTITY_CLI_COMMAND_CONFIRM_REPROVISION;
    } else {
        return TECLA_IDENTITY_CLI_SYNTAX_ERROR;
    }
    command->firmware_node_id = id;
    return TECLA_IDENTITY_CLI_OK;
}

static tecla_identity_cli_result_t map_identity_result(tecla_identity_result_t result)
{
    switch (result) {
    case TECLA_IDENTITY_OK: return TECLA_IDENTITY_CLI_OK;
    case TECLA_IDENTITY_NOT_PROVISIONED: return TECLA_IDENTITY_CLI_NOT_PROVISIONED;
    case TECLA_IDENTITY_INVALID_ARGUMENT: return TECLA_IDENTITY_CLI_INVALID_ID;
    case TECLA_IDENTITY_CORRUPT: return TECLA_IDENTITY_CLI_CORRUPT;
    case TECLA_IDENTITY_ALREADY_DIFFERENT: return TECLA_IDENTITY_CLI_ID_CONFLICT;
    case TECLA_IDENTITY_STORAGE_ERROR: return TECLA_IDENTITY_CLI_STORAGE_ERROR;
    case TECLA_IDENTITY_CONFIRMATION_REQUIRED:
    default: return TECLA_IDENTITY_CLI_STORAGE_ERROR;
    }
}

tecla_identity_cli_result_t tecla_identity_cli_execute(
    tecla_identity_cli_state_t *state,
    const tecla_identity_store_t *store,
    const char *line,
    size_t line_length,
    uint32_t *firmware_node_id_out)
{
    if (state == NULL || store == NULL) return TECLA_IDENTITY_CLI_STORAGE_ERROR;
    tecla_identity_cli_command_t command;
    const tecla_identity_cli_result_t parsed =
        tecla_identity_cli_parse(line, line_length, &command);
    if (parsed != TECLA_IDENTITY_CLI_OK) return parsed;

    uint32_t current_id = 0U;
    switch (command.type) {
    case TECLA_IDENTITY_CLI_COMMAND_STATUS: {
        const tecla_identity_result_t loaded = tecla_node_identity_load(store, &current_id);
        if (loaded != TECLA_IDENTITY_OK) return map_identity_result(loaded);
        if (firmware_node_id_out != NULL) *firmware_node_id_out = current_id;
        return TECLA_IDENTITY_CLI_OK;
    }
    case TECLA_IDENTITY_CLI_COMMAND_PROVISION: {
        if (command.firmware_node_id == 0U) return TECLA_IDENTITY_CLI_INVALID_ID;
        const tecla_identity_result_t loaded = tecla_node_identity_load(store, &current_id);
        if (loaded == TECLA_IDENTITY_OK) {
            return current_id == command.firmware_node_id
                ? TECLA_IDENTITY_CLI_ALREADY_PROVISIONED
                : TECLA_IDENTITY_CLI_ID_CONFLICT;
        }
        if (loaded != TECLA_IDENTITY_NOT_PROVISIONED) return map_identity_result(loaded);
        return map_identity_result(tecla_node_identity_provision_once(
            store, command.firmware_node_id));
    }
    case TECLA_IDENTITY_CLI_COMMAND_REPROVISION: {
        if (command.firmware_node_id == 0U) return TECLA_IDENTITY_CLI_INVALID_ID;
        const tecla_identity_result_t loaded = tecla_node_identity_load(store, &current_id);
        if (loaded != TECLA_IDENTITY_OK) return map_identity_result(loaded);
        if (current_id == command.firmware_node_id)
            return TECLA_IDENTITY_CLI_ALREADY_PROVISIONED;
        state->reprovision_pending = true;
        state->pending_firmware_node_id = command.firmware_node_id;
        return TECLA_IDENTITY_CLI_CONFIRMATION_REQUIRED;
    }
    case TECLA_IDENTITY_CLI_COMMAND_CONFIRM_REPROVISION:
        if (!state->reprovision_pending) return TECLA_IDENTITY_CLI_NO_PENDING_CONFIRMATION;
        if (command.firmware_node_id != state->pending_firmware_node_id)
            return TECLA_IDENTITY_CLI_CONFIRMATION_MISMATCH;
        {
            const tecla_identity_cli_result_t result = map_identity_result(
                tecla_node_identity_reprovision(store, command.firmware_node_id, true));
            if (result == TECLA_IDENTITY_CLI_OK) {
                state->reprovision_pending = false;
                state->pending_firmware_node_id = 0U;
                if (firmware_node_id_out != NULL)
                    *firmware_node_id_out = command.firmware_node_id;
            }
            return result;
        }
    case TECLA_IDENTITY_CLI_COMMAND_CANCEL:
        state->reprovision_pending = false;
        state->pending_firmware_node_id = 0U;
        return TECLA_IDENTITY_CLI_CANCELLED;
    default:
        return TECLA_IDENTITY_CLI_SYNTAX_ERROR;
    }
}

const char *tecla_identity_cli_result_name(tecla_identity_cli_result_t result)
{
    switch (result) {
    case TECLA_IDENTITY_CLI_OK: return "OK";
    case TECLA_IDENTITY_CLI_CONFIRMATION_REQUIRED: return "CONFIRMATION_REQUIRED";
    case TECLA_IDENTITY_CLI_CANCELLED: return "CANCELLED";
    case TECLA_IDENTITY_CLI_NO_PENDING_CONFIRMATION: return "NO_PENDING_CONFIRMATION";
    case TECLA_IDENTITY_CLI_CONFIRMATION_MISMATCH: return "CONFIRMATION_MISMATCH";
    case TECLA_IDENTITY_CLI_ALREADY_PROVISIONED: return "ALREADY_PROVISIONED";
    case TECLA_IDENTITY_CLI_ID_CONFLICT: return "ID_CONFLICT";
    case TECLA_IDENTITY_CLI_NOT_PROVISIONED: return "NOT_PROVISIONED";
    case TECLA_IDENTITY_CLI_INVALID_ID: return "INVALID_ID";
    case TECLA_IDENTITY_CLI_SYNTAX_ERROR: return "SYNTAX_ERROR";
    case TECLA_IDENTITY_CLI_LINE_TOO_LONG: return "LINE_TOO_LONG";
    case TECLA_IDENTITY_CLI_CORRUPT: return "CORRUPT";
    case TECLA_IDENTITY_CLI_STORAGE_ERROR: return "STORAGE_ERROR";
    default: return "UNKNOWN";
    }
}
