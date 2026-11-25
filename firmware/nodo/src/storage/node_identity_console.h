#ifndef TECLA_NODE_IDENTITY_CONSOLE_H
#define TECLA_NODE_IDENTITY_CONSOLE_H

#include "esp_err.h"

/* Starts the optional ESP-IDF UART REPL and registers the `identity` command. */
esp_err_t tecla_node_identity_console_start(void);

#endif
