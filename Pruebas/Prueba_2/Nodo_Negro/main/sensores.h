#define SENSORES_H

#include "esp_err.h"
#include "packet.h"

esp_err_t sensores_init(void);

void sensores_read(node_packet_t *packet);