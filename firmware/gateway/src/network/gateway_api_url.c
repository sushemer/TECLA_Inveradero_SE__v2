#include "gateway_api_url.h"

#include <stdbool.h>
#include <string.h>

static bool contains_forbidden_url_char(const char *value)
{
    for (const unsigned char *cursor = (const unsigned char *)value; *cursor != '\0'; ++cursor) {
        if (*cursor <= 0x20U || *cursor == 0x7FU || *cursor == '?' || *cursor == '#') {
            return true;
        }
    }
    return false;
}

tecla_gateway_url_result_t tecla_gateway_api_url_build(
    const char *base_url, const char *path, char *output, size_t output_capacity)
{
    if (base_url == NULL || path == NULL || output == NULL || output_capacity == 0U) {
        return TECLA_GATEWAY_URL_INVALID_ARGUMENT;
    }
    output[0] = '\0';
    if (base_url[0] == '\0') return TECLA_GATEWAY_URL_DISABLED;
    if (path[0] != '/' || contains_forbidden_url_char(path) ||
        strstr(path, "://") != NULL || contains_forbidden_url_char(base_url)) {
        return TECLA_GATEWAY_URL_INVALID_BASE;
    }
    const bool http = strncmp(base_url, "http://", 7U) == 0;
    const bool https = strncmp(base_url, "https://", 8U) == 0;
    if (!http && !https) return TECLA_GATEWAY_URL_INVALID_BASE;

    const char *authority = base_url + (https ? 8U : 7U);
    const char *authority_end = authority + strcspn(authority, "/");
    if (authority_end == authority || memchr(authority, '@', (size_t)(authority_end - authority)) != NULL) {
        return TECLA_GATEWAY_URL_INVALID_BASE;
    }

    size_t base_length = strlen(base_url);
    while (base_length > (size_t)(authority_end - base_url) && base_url[base_length - 1U] == '/') {
        --base_length;
    }
    const size_t path_length = strlen(path);
    if (base_length >= output_capacity ||
        path_length >= output_capacity - base_length) return TECLA_GATEWAY_URL_TOO_LONG;
    memcpy(output, base_url, base_length);
    memcpy(output + base_length, path, path_length + 1U);
    return TECLA_GATEWAY_URL_OK;
}
