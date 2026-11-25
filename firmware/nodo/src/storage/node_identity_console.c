#include "node_identity_console.h"

#include <stdio.h>
#include <string.h>

#include "esp_console.h"
#include "node_identity_cli.h"
#include "node_identity_nvs.h"

static tecla_identity_cli_state_t identity_cli_state;
static esp_console_repl_t *identity_repl;
static bool identity_console_started;

static bool command_returns_identity(const char *line)
{
    return strcmp(line, "status") == 0 ||
           strncmp(line, "provision ", 10U) == 0 ||
           strncmp(line, "confirm-reprovision ", 20U) == 0;
}

static int identity_command(int argc, char **argv)
{
    if (argc < 2 || argc > 3) {
        printf("ERR SYNTAX_ERROR\r\n");
        return 1;
    }

    char command_line[TECLA_IDENTITY_CLI_MAX_LINE + 1U];
    size_t used = 0U;
    for (int index = 1; index < argc; ++index) {
        if (index != 1) {
            if (used >= TECLA_IDENTITY_CLI_MAX_LINE) {
                printf("ERR LINE_TOO_LONG\r\n");
                return 1;
            }
            command_line[used++] = ' ';
        }
        const size_t token_length = strlen(argv[index]);
        if (token_length > TECLA_IDENTITY_CLI_MAX_LINE - used) {
            printf("ERR LINE_TOO_LONG\r\n");
            return 1;
        }
        memcpy(command_line + used, argv[index], token_length);
        used += token_length;
    }
    command_line[used] = '\0';

    uint32_t resulting_id = 0U;
    const tecla_identity_store_t store = tecla_node_identity_nvs_store();
    const tecla_identity_cli_result_t result = tecla_identity_cli_execute(
        &identity_cli_state, &store, command_line, used, &resulting_id);
    if (result == TECLA_IDENTITY_CLI_CONFIRMATION_REQUIRED) {
        printf("CONFIRM firmware_node_id=%lu; use identity confirm-reprovision %lu or identity cancel\r\n",
               (unsigned long)identity_cli_state.pending_firmware_node_id,
               (unsigned long)identity_cli_state.pending_firmware_node_id);
        return 0;
    }
    if (result == TECLA_IDENTITY_CLI_OK &&
        command_returns_identity(command_line)) {
        printf("OK firmware_node_id=%lu\r\n", (unsigned long)resulting_id);
        return 0;
    }
    if (result == TECLA_IDENTITY_CLI_OK) {
        printf("OK\r\n");
        return 0;
    }
    if (result == TECLA_IDENTITY_CLI_CANCELLED) {
        printf("OK CANCELLED\r\n");
        return 0;
    }
    printf("ERR %s\r\n", tecla_identity_cli_result_name(result));
    return 1;
}

esp_err_t tecla_node_identity_console_start(void)
{
    if (identity_console_started) return ESP_ERR_INVALID_STATE;

    const esp_console_dev_uart_config_t uart_config =
        ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "tecla-id> ";
    repl_config.max_cmdline_length = TECLA_IDENTITY_CLI_MAX_LINE;
    repl_config.max_cmdline_args = 4U;
    repl_config.history_save_path = NULL;

    esp_err_t err = esp_console_new_repl_uart(&uart_config, &repl_config,
                                               &identity_repl);
    if (err != ESP_OK) return err;

    const esp_console_cmd_t command = {
        .command = "identity",
        .help = "Read or provision firmware_node_id in local NVS",
        .hint = "<status|provision ID|reprovision ID|confirm-reprovision ID|cancel>",
        .func = identity_command,
    };
    err = esp_console_cmd_register(&command);
    if (err != ESP_OK) return err;
    err = esp_console_start_repl(identity_repl);
    if (err != ESP_OK) return err;
    identity_console_started = true;
    return ESP_OK;
}
