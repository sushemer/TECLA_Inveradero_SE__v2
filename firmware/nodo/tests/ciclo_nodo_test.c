#include "../../comun/operacion/ciclo_nodo.h"

#include <assert.h>
#include <stdio.h>

static tecla_ciclo_salida_t evento(tecla_ciclo_contexto_t *ctx,
                                   tecla_ciclo_evento_t tipo,
                                   bool muestreo,
                                   const uint8_t *boot,
                                   uint32_t seq,
                                   uint16_t intento_radio,
                                   uint8_t max_reintentos)
{
    static const uint8_t serialized_sample[] = {0x03U, 0x01U, 0x02U, 0xA5U, 0x5AU};
    const tecla_ciclo_entrada_t entrada = {
        .evento = tipo,
        .muestreo_debido = muestreo,
        .boot_id = boot,
        .seq = seq,
        .intento_radio = intento_radio,
        .frame_bytes = tipo == TECLA_CICLO_EVENT_SAMPLE_READY ? serialized_sample : NULL,
        .frame_len = tipo == TECLA_CICLO_EVENT_SAMPLE_READY ? sizeof(serialized_sample) : 0U,
    };
    return tecla_ciclo_procesar(ctx, &entrada, max_reintentos);
}

int main(void)
{
    tecla_ciclo_contexto_t ctx;
    uint8_t boot[TECLA_CICLO_BOOT_ID_BYTES] = {0};
    boot[0] = 0xA5U;
    assert(tecla_ciclo_iniciar(&ctx, TECLA_CICLO_MODO_OPERATIVO));
    assert(!tecla_ciclo_iniciar(&ctx, (tecla_ciclo_modo_t)99));
    assert(tecla_ciclo_iniciar(&ctx, TECLA_CICLO_MODO_OPERATIVO));

    tecla_ciclo_salida_t out = evento(&ctx, TECLA_CICLO_EVENT_WAKE, false,
                                      NULL, 0U, 0U, 1U);
    assert(out.estado == TECLA_CICLO_RENDEZVOUS &&
           out.accion == TECLA_CICLO_ACCION_OPEN_MANAGEMENT);
    assert(tecla_ciclo_definir_modo(&ctx, TECLA_CICLO_MODO_MANTENIMIENTO));
    out = evento(&ctx, TECLA_CICLO_EVENT_MANAGEMENT_DONE, true, NULL, 0U, 0U, 1U);
    assert(out.estado == TECLA_CICLO_SLEEP_DECISION &&
           out.accion == TECLA_CICLO_ACCION_NONE);
    out = evento(&ctx, TECLA_CICLO_EVENT_SLEEP_DENIED, false, NULL, 0U, 0U, 1U);
    assert(out.estado == TECLA_CICLO_AWAKE_WAIT &&
           out.accion == TECLA_CICLO_ACCION_ARM_BOUNDED_MANAGEMENT_WAIT);
    out = evento(&ctx, TECLA_CICLO_EVENT_NEXT_MANAGEMENT_DUE, false, NULL, 0U, 0U, 1U);
    assert(out.estado == TECLA_CICLO_RENDEZVOUS &&
           out.accion == TECLA_CICLO_ACCION_OPEN_MANAGEMENT);

    assert(tecla_ciclo_definir_modo(&ctx, TECLA_CICLO_MODO_OPERATIVO));
    out = evento(&ctx, TECLA_CICLO_EVENT_MANAGEMENT_DONE, true, NULL, 0U, 0U, 1U);
    assert(out.estado == TECLA_CICLO_MEASURE &&
           out.accion == TECLA_CICLO_ACCION_READ_SENSORS);
    out = evento(&ctx, TECLA_CICLO_EVENT_SAMPLE_READY, true, boot, 7U, 0U, 1U);
    assert(out.estado == TECLA_CICLO_WAIT_ACK &&
           out.accion == TECLA_CICLO_ACCION_SEND_FRAME);
    assert(ctx.frame_len == 5U && ctx.frame_bytes[3] == 0xA5U);
    assert(!tecla_ciclo_definir_modo(&ctx, TECLA_CICLO_MODO_MANTENIMIENTO));
    out = evento(&ctx, TECLA_CICLO_EVENT_TX_CALLBACK_OK, false, NULL, 0U, 1U, 1U);
    assert(out.estado == TECLA_CICLO_WAIT_ACK &&
           out.accion == TECLA_CICLO_ACCION_NONE);
    out = evento(&ctx, TECLA_CICLO_EVENT_APP_ACK, false, boot, 6U, 0U, 1U);
    assert(out.evento_ignorado && out.estado == TECLA_CICLO_WAIT_ACK);
    uint8_t other_boot[TECLA_CICLO_BOOT_ID_BYTES] = {0};
    other_boot[0] = 0xA4U;
    out = evento(&ctx, TECLA_CICLO_EVENT_APP_ACK, false, other_boot, 7U, 0U, 1U);
    assert(out.evento_ignorado && out.estado == TECLA_CICLO_WAIT_ACK);
    out = evento(&ctx, TECLA_CICLO_EVENT_APP_ACK, false, boot, 7U, 0U, 1U);
    assert(!out.evento_ignorado && out.estado == TECLA_CICLO_SLEEP_DECISION);
    out = evento(&ctx, TECLA_CICLO_EVENT_SLEEP_AUTHORIZED, false, NULL, 0U, 0U, 1U);
    assert(out.estado == TECLA_CICLO_SLEEPING &&
           out.accion == TECLA_CICLO_ACCION_ENTER_DEEP_SLEEP);

    assert(tecla_ciclo_iniciar(&ctx, TECLA_CICLO_MODO_OPERATIVO));
    (void)evento(&ctx, TECLA_CICLO_EVENT_WAKE, false, NULL, 0U, 0U, 1U);
    (void)evento(&ctx, TECLA_CICLO_EVENT_MANAGEMENT_DONE, true, NULL, 0U, 0U, 1U);
    (void)evento(&ctx, TECLA_CICLO_EVENT_SAMPLE_READY, true, boot, 8U, 0U, 1U);
    out = evento(&ctx, TECLA_CICLO_EVENT_ACK_TIMEOUT, false, NULL, 0U, 1U, 1U);
    assert(out.estado == TECLA_CICLO_RETRY_WAIT && out.reintentos_usados == 1U &&
           out.accion == TECLA_CICLO_ACCION_WAIT_RETRY_BACKOFF);
    out = evento(&ctx, TECLA_CICLO_EVENT_RETRY_DELAY_DONE, false, NULL, 0U, 0U, 1U);
    assert(out.evento_ignorado && out.estado == TECLA_CICLO_RETRY_WAIT);
    out = evento(&ctx, TECLA_CICLO_EVENT_RETRY_DELAY_DONE, false, NULL, 0U, 2U, 1U);
    assert(out.estado == TECLA_CICLO_WAIT_ACK &&
           out.accion == TECLA_CICLO_ACCION_SEND_FRAME && ctx.seq_activo == 8U);
    assert(ctx.frame_len == 5U && ctx.frame_bytes[3] == 0xA5U &&
           ctx.boot_id_activo[0] == boot[0]);
    out = evento(&ctx, TECLA_CICLO_EVENT_ACK_TIMEOUT, false, NULL, 0U, 2U, 1U);
    assert(out.estado == TECLA_CICLO_SLEEP_DECISION && out.reintentos_usados == 1U);
    out = evento(&ctx, TECLA_CICLO_EVENT_ACK_TIMEOUT, false, NULL, 0U, 2U, 1U);
    assert(out.evento_ignorado); /* No unbounded retry after terminal timeout. */

    (void)tecla_ciclo_iniciar(&ctx, TECLA_CICLO_MODO_OPERATIVO);
    (void)evento(&ctx, TECLA_CICLO_EVENT_WAKE, false, NULL, 0U, 0U, UINT8_MAX);
    (void)evento(&ctx, TECLA_CICLO_EVENT_MANAGEMENT_DONE, true, NULL, 0U, 0U, UINT8_MAX);
    out = evento(&ctx, TECLA_CICLO_EVENT_SAMPLE_READY, true, boot, 9U, 0U, UINT8_MAX);
    assert(out.intento_radio_siguiente == 1U);
    for (uint16_t attempt = 1U; attempt <= UINT8_MAX; ++attempt) {
        out = evento(&ctx, TECLA_CICLO_EVENT_ACK_TIMEOUT, false, NULL, 0U,
                     attempt, UINT8_MAX);
        assert(out.estado == TECLA_CICLO_RETRY_WAIT &&
               out.intento_radio_siguiente == (uint16_t)(attempt + 1U));
        out = evento(&ctx, TECLA_CICLO_EVENT_RETRY_DELAY_DONE, false, NULL, 0U,
                     (uint16_t)(attempt + 1U), UINT8_MAX);
        assert(out.estado == TECLA_CICLO_WAIT_ACK);
    }
    out = evento(&ctx, TECLA_CICLO_EVENT_ACK_TIMEOUT, false, NULL, 0U, 256U, UINT8_MAX);
    assert(out.estado == TECLA_CICLO_SLEEP_DECISION && out.reintentos_usados == UINT8_MAX);

    assert(tecla_ciclo_iniciar(&ctx, TECLA_CICLO_MODO_OPERATIVO));
    (void)evento(&ctx, TECLA_CICLO_EVENT_WAKE, false, NULL, 0U, 0U, 1U);
    out = evento(&ctx, TECLA_CICLO_EVENT_MANAGEMENT_TIMEOUT, true, NULL, 0U, 0U, 1U);
    assert(out.estado == TECLA_CICLO_SLEEP_DECISION &&
           out.accion == TECLA_CICLO_ACCION_NONE);

    (void)tecla_ciclo_iniciar(&ctx, TECLA_CICLO_MODO_OPERATIVO);
    (void)evento(&ctx, TECLA_CICLO_EVENT_WAKE, false, NULL, 0U, 0U, 1U);
    (void)evento(&ctx, TECLA_CICLO_EVENT_MANAGEMENT_DONE, true, NULL, 0U, 0U, 1U);
    out = evento(&ctx, TECLA_CICLO_EVENT_SAMPLE_READY, true, boot, UINT32_MAX, 0U, 1U);
    assert(out.evento_ignorado && ctx.estado == TECLA_CICLO_MEASURE);

    puts("PASS node_cycle_state_machine_modes_ack_correlation_bounded_retry_sleep_gate");
    return 0;
}
