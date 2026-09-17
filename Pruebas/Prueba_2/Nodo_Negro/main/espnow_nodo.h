#ifndef ESPNOW_NODO_H
#define ESPNOW_NODO_H

#include "esp_err.h"
#include "packet.h"


esp_err_t espnow_nodo_init(void);

esp_err_t espnow_nodo_send(const node_packet_t *packet);


#endif // ESPNOW_NODO_H