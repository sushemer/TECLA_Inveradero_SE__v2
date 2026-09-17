#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {

    uint32_t sequence;

    int adc_raw;
    float voltage;

    float temperature;
    float humidity;
    float vpd;

    bool dht_ok;

} node_packet_t;


// Confirmación enviada: GATEWAY -> NODO
typedef struct {

    uint32_t sequence;

} ack_packet_t;

#endif