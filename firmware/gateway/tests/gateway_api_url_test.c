#include <assert.h>
#include <string.h>

#include "../src/network/gateway_api_url.h"

int main(void)
{
    char output[128];
    assert(tecla_gateway_api_url_build("", "/lecturas", output, sizeof(output)) ==
           TECLA_GATEWAY_URL_DISABLED);
    assert(tecla_gateway_api_url_build("https://api.example.test/api", "/lecturas",
                                       output, sizeof(output)) == TECLA_GATEWAY_URL_OK);
    assert(strcmp(output, "https://api.example.test/api/lecturas") == 0);
    assert(tecla_gateway_api_url_build("http://192.0.2.10:4001/api/", "/lecturas",
                                       output, sizeof(output)) == TECLA_GATEWAY_URL_OK);
    assert(strcmp(output, "http://192.0.2.10:4001/api/lecturas") == 0);
    assert(tecla_gateway_api_url_build("https://user:secret@example.test/api", "/lecturas",
                                       output, sizeof(output)) == TECLA_GATEWAY_URL_INVALID_BASE);
    assert(tecla_gateway_api_url_build("https://api.example.test/api?token=x", "/lecturas",
                                       output, sizeof(output)) == TECLA_GATEWAY_URL_INVALID_BASE);
    assert(tecla_gateway_api_url_build("api.example.test/api", "/lecturas",
                                       output, sizeof(output)) == TECLA_GATEWAY_URL_INVALID_BASE);
    assert(tecla_gateway_api_url_build("https://api.example.test/api", "lecturas",
                                       output, sizeof(output)) == TECLA_GATEWAY_URL_INVALID_BASE);
    assert(tecla_gateway_api_url_build("https://api.example.test/api", "/lecturas",
                                       output, 8U) == TECLA_GATEWAY_URL_TOO_LONG);
    assert(output[0] == '\0');
    return 0;
}
