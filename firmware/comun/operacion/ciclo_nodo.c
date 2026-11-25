#include "ciclo_nodo.h"

#include <stddef.h>
#include <string.h>

// Comprueba que el modo pertenezca a los valores definidos por el ciclo.
static bool modo_valido(tecla_ciclo_modo_t modo){
    return modo == TECLA_CICLO_MODO_OPERATIVO || modo == TECLA_CICLO_MODO_INACTIVO ||  modo == TECLA_CICLO_MODO_MANTENIMIENTO;
}

// Limpia el contexto e inicia el ciclo en estado reposo.
bool tecla_ciclo_iniciar(tecla_ciclo_contexto_t *contexto, tecla_ciclo_modo_t modo_inicial){
    if (contexto == NULL || !modo_valido(modo_inicial)) return false;
    memset(contexto, 0, sizeof(*contexto));
    contexto->estado = TECLA_CICLO_IDLE;
    contexto->modo = modo_inicial;
    return true;
}

// Cambia el modo solo durante reposo o rendezvous.
bool tecla_ciclo_definir_modo(tecla_ciclo_contexto_t *contexto, tecla_ciclo_modo_t modo){
    if (contexto == NULL || !modo_valido(modo)) return false;
    if (contexto->estado != TECLA_CICLO_IDLE &&
        contexto->estado != TECLA_CICLO_RENDEZVOUS) return false;
    contexto->modo = modo;
    return true;
}

// Verifica que el ACK coincida con el boot y la secuencia en curso.
static bool ack_corresponde(const tecla_ciclo_contexto_t *contexto, const tecla_ciclo_entrada_t *entrada){
    return entrada->boot_id != NULL && entrada->seq != 0U &&
           contexto->seq_activo != 0U &&
           entrada->seq == contexto->seq_activo &&
           memcmp(entrada->boot_id, contexto->boot_id_activo,
                  TECLA_CICLO_BOOT_ID_BYTES) == 0;
}

// Construye la salida actual con el estado y los datos de reintento.
static tecla_ciclo_salida_t salida(tecla_ciclo_contexto_t *contexto, tecla_ciclo_accion_t accion, bool ignorado){
    tecla_ciclo_salida_t resultado = {
        .estado = contexto->estado,
        .accion = accion,
        .reintentos_usados = contexto->reintentos_usados,
        .intento_radio_siguiente = (uint16_t)contexto->reintentos_usados + 1U,
        .evento_ignorado = ignorado,
    };
    return resultado;
}

// Programa otro intento o cierra el envío al alcanzar el límite.
static tecla_ciclo_salida_t cerrar_intento( tecla_ciclo_contexto_t *contexto, uint8_t max_reintentos){
    if (contexto->reintentos_usados < max_reintentos) {
        contexto->reintentos_usados++;
        contexto->estado = TECLA_CICLO_RETRY_WAIT;
        return salida(contexto, TECLA_CICLO_ACCION_WAIT_RETRY_BACKOFF, false);
    }
    contexto->estado = TECLA_CICLO_SLEEP_DECISION;
    return salida(contexto, TECLA_CICLO_ACCION_NONE, false);
}

// Avanza la máquina de estados y reporta qué acción ejecutará el llamador.
tecla_ciclo_salida_t tecla_ciclo_procesar(tecla_ciclo_contexto_t *contexto, const tecla_ciclo_entrada_t *entrada,uint8_t max_reintentos){
    if (contexto == NULL || entrada == NULL || !modo_valido(contexto->modo)) {
        tecla_ciclo_salida_t invalida = {
            .estado = TECLA_CICLO_IDLE,
            .accion = TECLA_CICLO_ACCION_NONE,
            .reintentos_usados = 0U,
            .intento_radio_siguiente = 0U,
            .evento_ignorado = true,
        };
        return invalida;
    }

    switch (contexto->estado) {
    case TECLA_CICLO_IDLE:
        if (entrada->evento != TECLA_CICLO_EVENT_WAKE) break;
        contexto->estado = TECLA_CICLO_RENDEZVOUS;
        return salida(contexto, TECLA_CICLO_ACCION_OPEN_MANAGEMENT, false);

    case TECLA_CICLO_RENDEZVOUS:
        if (entrada->evento == TECLA_CICLO_EVENT_MANAGEMENT_TIMEOUT) {
            contexto->estado = TECLA_CICLO_SLEEP_DECISION;
            return salida(contexto, TECLA_CICLO_ACCION_NONE, false);
        }
        if (entrada->evento != TECLA_CICLO_EVENT_MANAGEMENT_DONE) break;
        if (contexto->modo == TECLA_CICLO_MODO_OPERATIVO &&
            entrada->muestreo_debido) {
            contexto->estado = TECLA_CICLO_MEASURE;
            return salida(contexto, TECLA_CICLO_ACCION_READ_SENSORS, false);
        }
        contexto->estado = TECLA_CICLO_SLEEP_DECISION;
        return salida(contexto, TECLA_CICLO_ACCION_NONE, false);

    case TECLA_CICLO_MEASURE:
        if (entrada->evento == TECLA_CICLO_EVENT_SAMPLE_FAILED) {
            contexto->estado = TECLA_CICLO_SLEEP_DECISION;
            return salida(contexto, TECLA_CICLO_ACCION_NONE, false);
        }
        if (entrada->evento != TECLA_CICLO_EVENT_SAMPLE_READY ||
            entrada->boot_id == NULL || entrada->seq == 0U ||
            entrada->seq == UINT32_MAX || entrada->frame_bytes == NULL ||
            entrada->frame_len == 0U ||
            entrada->frame_len > TECLA_CICLO_MAX_FRAME_BYTES) break;
        memcpy(contexto->boot_id_activo, entrada->boot_id,
               TECLA_CICLO_BOOT_ID_BYTES);
        contexto->seq_activo = entrada->seq;
        memcpy(contexto->frame_bytes, entrada->frame_bytes, entrada->frame_len);
        contexto->frame_len = (uint16_t)entrada->frame_len;
        contexto->reintentos_usados = 0U;
        contexto->estado = TECLA_CICLO_WAIT_ACK;
        return salida(contexto, TECLA_CICLO_ACCION_SEND_FRAME, false);

    case TECLA_CICLO_WAIT_ACK:
        if (entrada->evento == TECLA_CICLO_EVENT_TX_CALLBACK_OK) {
            if (entrada->intento_radio != (uint16_t)contexto->reintentos_usados + 1U) break;
            // La confirmación de transmisión de radio no prueba recepción de aplicación.
            return salida(contexto, TECLA_CICLO_ACCION_NONE, false);
        }
        if (entrada->evento == TECLA_CICLO_EVENT_APP_ACK ||
            entrada->evento == TECLA_CICLO_EVENT_APP_NACK) {
            if (!ack_corresponde(contexto, entrada)) break;
            if (entrada->evento == TECLA_CICLO_EVENT_APP_ACK) {
                contexto->estado = TECLA_CICLO_SLEEP_DECISION;
                return salida(contexto, TECLA_CICLO_ACCION_NONE, false);
            }
            return cerrar_intento(contexto, max_reintentos);
        }
        if (entrada->evento == TECLA_CICLO_EVENT_TX_CALLBACK_FAILED ||
            entrada->evento == TECLA_CICLO_EVENT_ACK_TIMEOUT) {
            if (entrada->intento_radio != (uint16_t)contexto->reintentos_usados + 1U) break;
            return cerrar_intento(contexto, max_reintentos);
        }
        break;

    case TECLA_CICLO_RETRY_WAIT:
        if (entrada->evento != TECLA_CICLO_EVENT_RETRY_DELAY_DONE) break;
        if (entrada->intento_radio != (uint16_t)contexto->reintentos_usados + 1U) break;
        contexto->estado = TECLA_CICLO_WAIT_ACK;
        return salida(contexto, TECLA_CICLO_ACCION_SEND_FRAME, false);

    case TECLA_CICLO_SLEEP_DECISION:
        if (entrada->evento == TECLA_CICLO_EVENT_SLEEP_AUTHORIZED) {
            contexto->estado = TECLA_CICLO_SLEEPING;
            return salida(contexto, TECLA_CICLO_ACCION_ENTER_DEEP_SLEEP, false);
        }
        if (entrada->evento == TECLA_CICLO_EVENT_SLEEP_DENIED) {
            contexto->estado = TECLA_CICLO_AWAKE_WAIT;
            return salida(contexto, TECLA_CICLO_ACCION_ARM_BOUNDED_MANAGEMENT_WAIT, false);
        }
        break;

    case TECLA_CICLO_SLEEPING:
    case TECLA_CICLO_AWAKE_WAIT:
        if (entrada->evento != TECLA_CICLO_EVENT_WAKE &&
            entrada->evento != TECLA_CICLO_EVENT_NEXT_MANAGEMENT_DUE) break;
        contexto->estado = TECLA_CICLO_RENDEZVOUS;
        return salida(contexto, TECLA_CICLO_ACCION_OPEN_MANAGEMENT, false);

    default:
        break;
    }
    return salida(contexto, TECLA_CICLO_ACCION_NONE, true);
}
