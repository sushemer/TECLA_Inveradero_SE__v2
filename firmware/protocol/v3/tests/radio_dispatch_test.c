#include "../gateway_dispatch.h"
#include "../../../comun/operacion/ciclo_nodo.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint8_t destination[6];
    uint8_t frame[TECLA_WIRE_V3_MAX_FRAME_SIZE];
    size_t frame_len;
    unsigned sends;
    bool fail_send;
} mock_radio_t;

static bool mock_send(void *context, const uint8_t destination_mac[6],
                      const uint8_t *frame, size_t frame_len)
{
    mock_radio_t *radio = context;
    if (radio->fail_send || frame_len > sizeof(radio->frame)) return false;
    memcpy(radio->destination, destination_mac, sizeof(radio->destination));
    memcpy(radio->frame, frame, frame_len);
    radio->frame_len = frame_len;
    radio->sends++;
    return true;
}

static size_t make_telemetry(uint8_t *out, uint32_t seq)
{
    tecla_wire_v3_header_t header = {
        .firmware_node_id = 42U,
        .seq = seq,
        .type = TECLA_WIRE_V3_TELEMETRY,
    };
    for (uint8_t i = 0U; i < 16U; ++i) header.boot_id[i] = (uint8_t)(i + 1U);
    header.boot_id[6] = (uint8_t)((header.boot_id[6] & 0x0fU) | 0x40U);
    header.boot_id[8] = (uint8_t)((header.boot_id[8] & 0x3fU) | 0x80U);

    tecla_wire_v3_message_t message = {0};
    message.telemetry.effective_mode = 1U;
    message.telemetry.sample_interval_s = 600U;
    message.telemetry.battery_percent = UINT8_MAX;
    message.telemetry.reading_count = 1U;
    message.telemetry.readings[0] = (tecla_wire_v3_reading_t){1U, 1U, 1U, 23.25f};
    size_t frame_len = 0U;
    assert(tecla_wire_v3_encode_typed(&header, &message, out,
        TECLA_WIRE_V3_MAX_FRAME_SIZE, &frame_len) == TECLA_WIRE_V3_OK);
    return frame_len;
}

static void ack_matches(const mock_radio_t *radio, uint32_t seq, uint8_t scope,
                        uint8_t reason)
{
    tecla_wire_v3_frame_view_t view;
    tecla_wire_v3_message_t message;
    tecla_wire_v3_header_t header;
    assert(tecla_wire_v3_decode(radio->frame, radio->frame_len, &view) == TECLA_WIRE_V3_OK);
    assert(view.header.type == TECLA_WIRE_V3_TELEMETRY_ACK &&
           view.header.firmware_node_id == 42U && view.header.seq == seq);
    assert(tecla_wire_v3_decode_typed(radio->frame, radio->frame_len, &header,
                                      &message) == TECLA_WIRE_V3_OK);
    assert(message.telemetry_ack.scope == scope && message.telemetry_ack.reason == reason);
}

static void test_timeout_retry_reuses_identical_frame(void)
{
    static const uint8_t node_mac[6] = {0x02U, 0x10U, 0x20U, 0x30U, 0x40U, 0x50U};
    const tecla_wire_v3_binding_t binding = {.firmware_node_id = 42U,
                                              .mac = {0x02U,0x10U,0x20U,0x30U,0x40U,0x50U}};
    tecla_wire_v3_gateway_dispatch_t gateway;
    mock_radio_t radio = {.fail_send = true};
    uint8_t frame[TECLA_WIRE_V3_MAX_FRAME_SIZE];
    const size_t frame_len = make_telemetry(frame, 77U);
    tecla_ciclo_contexto_t cycle;
    const uint8_t boot_id[16] = {1U,2U,3U,4U,5U,6U,0x47U,8U,0x89U,
                                 10U,11U,12U,13U,14U,15U,16U};

    assert(tecla_wire_v3_gateway_dispatch_init(&gateway, &binding, 1U) ==
           TECLA_WIRE_V3_DISPATCH_ACCEPTED);
    assert(tecla_ciclo_iniciar(&cycle, TECLA_CICLO_MODO_OPERATIVO));
    tecla_ciclo_entrada_t event = {.evento = TECLA_CICLO_EVENT_WAKE};
    assert(tecla_ciclo_procesar(&cycle, &event, 1U).accion ==
           TECLA_CICLO_ACCION_OPEN_MANAGEMENT);
    event = (tecla_ciclo_entrada_t){.evento = TECLA_CICLO_EVENT_MANAGEMENT_DONE,
                                    .muestreo_debido = true};
    assert(tecla_ciclo_procesar(&cycle, &event, 1U).accion ==
           TECLA_CICLO_ACCION_READ_SENSORS);
    event = (tecla_ciclo_entrada_t){.evento = TECLA_CICLO_EVENT_SAMPLE_READY,
        .boot_id = boot_id, .seq = 77U, .frame_bytes = frame, .frame_len = frame_len};
    assert(tecla_ciclo_procesar(&cycle, &event, 1U).accion ==
           TECLA_CICLO_ACCION_SEND_FRAME);
    uint8_t original[TECLA_WIRE_V3_MAX_FRAME_SIZE];
    const uint16_t original_len = cycle.frame_len;
    memcpy(original, cycle.frame_bytes, original_len);

    assert(tecla_wire_v3_gateway_dispatch_frame(&gateway, node_mac, frame, frame_len,
        mock_send, &radio) == TECLA_WIRE_V3_DISPATCH_ACK_TX_FAILED);
    event = (tecla_ciclo_entrada_t){.evento = TECLA_CICLO_EVENT_TX_CALLBACK_OK,
                                    .intento_radio = 1U};
    (void)tecla_ciclo_procesar(&cycle, &event, 1U);
    event = (tecla_ciclo_entrada_t){.evento = TECLA_CICLO_EVENT_ACK_TIMEOUT,
                                    .intento_radio = 1U};
    tecla_ciclo_salida_t action = tecla_ciclo_procesar(&cycle, &event, 1U);
    assert(action.accion == TECLA_CICLO_ACCION_WAIT_RETRY_BACKOFF);
    event = (tecla_ciclo_entrada_t){.evento = TECLA_CICLO_EVENT_RETRY_DELAY_DONE,
                                    .intento_radio = 2U};
    action = tecla_ciclo_procesar(&cycle, &event, 1U);
    assert(action.accion == TECLA_CICLO_ACCION_SEND_FRAME);
    assert(cycle.frame_len == original_len &&
           memcmp(cycle.frame_bytes, original, original_len) == 0);

    radio.fail_send = false;
    assert(tecla_wire_v3_gateway_dispatch_frame(&gateway, node_mac,
        cycle.frame_bytes, cycle.frame_len, mock_send, &radio) ==
        TECLA_WIRE_V3_DISPATCH_DUPLICATE);
    ack_matches(&radio, 77U, 1U, 0U);
    event = (tecla_ciclo_entrada_t){.evento = TECLA_CICLO_EVENT_APP_ACK,
                                    .boot_id = boot_id, .seq = 77U};
    action = tecla_ciclo_procesar(&cycle, &event, 1U);
    assert(action.estado == TECLA_CICLO_SLEEP_DECISION);
}

int main(void)
{
    test_timeout_retry_reuses_identical_frame();
    static const uint8_t node_mac[6] = {0x02U, 0x10U, 0x20U, 0x30U, 0x40U, 0x50U};
    static const uint8_t other_mac[6] = {0x02U, 0x10U, 0x20U, 0x30U, 0x40U, 0x51U};
    const tecla_wire_v3_binding_t binding = {.firmware_node_id = 42U,
                                              .mac = {0x02U,0x10U,0x20U,0x30U,0x40U,0x50U}};
    tecla_wire_v3_gateway_dispatch_t gateway;
    assert(tecla_wire_v3_gateway_dispatch_init(&gateway, &binding, 1U) ==
           TECLA_WIRE_V3_DISPATCH_ACCEPTED);
    mock_radio_t radio = {0};
    uint8_t frame[TECLA_WIRE_V3_MAX_FRAME_SIZE];
    const size_t frame_len = make_telemetry(frame, 1U);

    assert(tecla_wire_v3_gateway_dispatch_frame(&gateway, other_mac, frame, frame_len,
        mock_send, &radio) == TECLA_WIRE_V3_DISPATCH_UNAUTHORIZED);
    assert(radio.sends == 0U && gateway.queue_count == 0U);

    tecla_ciclo_contexto_t node_cycle;
    tecla_ciclo_salida_t action;
    const uint8_t boot_id[16] = {1U,2U,3U,4U,5U,6U,0x47U,8U,0x89U,
                                 10U,11U,12U,13U,14U,15U,16U};
    assert(tecla_ciclo_iniciar(&node_cycle, TECLA_CICLO_MODO_OPERATIVO));
    tecla_ciclo_entrada_t node_event = {.evento = TECLA_CICLO_EVENT_WAKE};
    action = tecla_ciclo_procesar(&node_cycle, &node_event, 1U);
    assert(action.accion == TECLA_CICLO_ACCION_OPEN_MANAGEMENT);
    node_event = (tecla_ciclo_entrada_t){.evento = TECLA_CICLO_EVENT_MANAGEMENT_DONE,
                                         .muestreo_debido = true};
    action = tecla_ciclo_procesar(&node_cycle, &node_event, 1U);
    assert(action.accion == TECLA_CICLO_ACCION_READ_SENSORS);
    node_event = (tecla_ciclo_entrada_t){.evento = TECLA_CICLO_EVENT_SAMPLE_READY,
        .boot_id = boot_id, .seq = 1U, .frame_bytes = frame, .frame_len = frame_len};
    action = tecla_ciclo_procesar(&node_cycle, &node_event, 1U);
    assert(action.accion == TECLA_CICLO_ACCION_SEND_FRAME);

    assert(tecla_wire_v3_gateway_dispatch_frame(&gateway, node_mac, frame, frame_len,
        mock_send, &radio) == TECLA_WIRE_V3_DISPATCH_ACCEPTED);
    ack_matches(&radio, 1U, 1U, 0U); /* LOCAL_VOLATILE_BUFFERED */
    assert(memcmp(radio.destination, node_mac, sizeof(node_mac)) == 0);
    node_event = (tecla_ciclo_entrada_t){.evento = TECLA_CICLO_EVENT_TX_CALLBACK_OK,
                                         .intento_radio = 1U};
    action = tecla_ciclo_procesar(&node_cycle, &node_event, 1U);
    assert(action.estado == TECLA_CICLO_WAIT_ACK); /* MAC callback is not app ACK. */
    tecla_wire_v3_frame_view_t ack_view;
    assert(tecla_wire_v3_decode(radio.frame, radio.frame_len, &ack_view) == TECLA_WIRE_V3_OK);
    node_event = (tecla_ciclo_entrada_t){.evento = TECLA_CICLO_EVENT_APP_ACK,
        .boot_id = ack_view.header.boot_id, .seq = ack_view.header.seq};
    action = tecla_ciclo_procesar(&node_cycle, &node_event, 1U);
    assert(action.estado == TECLA_CICLO_SLEEP_DECISION);

    assert(tecla_wire_v3_gateway_dispatch_frame(&gateway, node_mac, frame, frame_len,
        mock_send, &radio) == TECLA_WIRE_V3_DISPATCH_DUPLICATE);
    ack_matches(&radio, 1U, 1U, 0U);
    assert(gateway.queue_count == 1U && radio.sends == 2U);

    uint8_t altered[TECLA_WIRE_V3_MAX_FRAME_SIZE];
    memcpy(altered, frame, frame_len);
    altered[frame_len - 1U] ^= 1U;
    assert(tecla_wire_v3_gateway_dispatch_frame(&gateway, node_mac, altered, frame_len,
        mock_send, &radio) == TECLA_WIRE_V3_DISPATCH_SEQUENCE_CONFLICT);
    assert(gateway.conflicting_duplicate_count == 1U && radio.sends == 2U);

    for (uint32_t seq = 2U; seq <= TECLA_WIRE_V3_VOLATILE_QUEUE_CAPACITY; ++seq) {
        const size_t next_len = make_telemetry(frame, seq);
        assert(tecla_wire_v3_gateway_dispatch_frame(&gateway, node_mac, frame, next_len,
            mock_send, &radio) == TECLA_WIRE_V3_DISPATCH_ACCEPTED);
    }
    const size_t full_len = make_telemetry(frame, 9U);
    assert(tecla_wire_v3_gateway_dispatch_frame(&gateway, node_mac, frame, full_len,
        mock_send, &radio) == TECLA_WIRE_V3_DISPATCH_CACHE_FULL);
    ack_matches(&radio, 9U, 3U, 11U);
    assert(gateway.queue_count == TECLA_WIRE_V3_VOLATILE_QUEUE_CAPACITY &&
           gateway.overflow_count == 1U);

    tecla_wire_v3_buffered_telemetry_t head;
    assert(tecla_wire_v3_gateway_queue_peek_head(&gateway, &head) ==
           TECLA_WIRE_V3_DISPATCH_ACCEPTED);
    tecla_wire_v3_frame_view_t head_view;
    assert(tecla_wire_v3_decode(head.frame, head.frame_len, &head_view) == TECLA_WIRE_V3_OK);
    assert(head_view.header.seq == 1U);
    const uint8_t wrong_boot[16] = {0};
    assert(tecla_wire_v3_gateway_queue_ack_head(&gateway, 42U, wrong_boot, 1U) ==
           TECLA_WIRE_V3_DISPATCH_HEAD_MISMATCH);
    assert(gateway.queue_count == TECLA_WIRE_V3_VOLATILE_QUEUE_CAPACITY);
    assert(tecla_wire_v3_gateway_queue_ack_head(&gateway, 42U, head_view.header.boot_id,
        head_view.header.seq) == TECLA_WIRE_V3_DISPATCH_ACCEPTED);
    assert(gateway.queue_count == TECLA_WIRE_V3_VOLATILE_QUEUE_CAPACITY - 1U);

    const size_t retry_len = make_telemetry(frame, 9U);
    assert(tecla_wire_v3_gateway_dispatch_frame(&gateway, node_mac, frame, retry_len,
        mock_send, &radio) == TECLA_WIRE_V3_DISPATCH_ACCEPTED);
    ack_matches(&radio, 9U, 1U, 0U);
    tecla_wire_v3_buffered_telemetry_t snapshot[TECLA_WIRE_V3_VOLATILE_QUEUE_CAPACITY] = {0};
    uint8_t snapshot_count = 0U;
    assert(tecla_wire_v3_gateway_queue_snapshot(&gateway, snapshot,
        TECLA_WIRE_V3_VOLATILE_QUEUE_CAPACITY, &snapshot_count) ==
        TECLA_WIRE_V3_DISPATCH_ACCEPTED);
    assert(snapshot_count == TECLA_WIRE_V3_VOLATILE_QUEUE_CAPACITY);
    assert(tecla_wire_v3_gateway_queue_peek_head(&gateway, &head) ==
           TECLA_WIRE_V3_DISPATCH_ACCEPTED);
    assert(tecla_wire_v3_decode(head.frame, head.frame_len, &head_view) == TECLA_WIRE_V3_OK);
    assert(head_view.header.seq == 2U); /* FIFO retained original order after pop. */

    puts("PASS v3_encode_dispatch_mock_radio_correlated_ack_dedupe_capacity");
    return 0;
}
