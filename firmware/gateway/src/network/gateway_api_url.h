#ifndef TECLA_GATEWAY_API_URL_H
#define TECLA_GATEWAY_API_URL_H

#include <stddef.h>

typedef enum {
    TECLA_GATEWAY_URL_OK = 0,
    TECLA_GATEWAY_URL_DISABLED,
    TECLA_GATEWAY_URL_INVALID_ARGUMENT,
    TECLA_GATEWAY_URL_INVALID_BASE,
    TECLA_GATEWAY_URL_TOO_LONG,
} tecla_gateway_url_result_t;

/* base_url is an API root (normally .../api); path is a relative API route. */
tecla_gateway_url_result_t tecla_gateway_api_url_build(
    const char *base_url, const char *path, char *output, size_t output_capacity);

#endif
