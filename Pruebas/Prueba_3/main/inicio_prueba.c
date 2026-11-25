#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

#include "configuracion/configuracion_prueba.h"
#include "comunicacion/prueba_espnow.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "energia/sueno_profundo.h"
#include "sensores/lectura_dispositivos.h"

/* Ejecuta carga de CPU y transmisión opcional durante la ventana de medición. */
static void ejecutar_ventana_activa(const configuracion_prueba_t *config)
{
    const TickType_t inicio = xTaskGetTickCount();
    TickType_t ultima_transmision = inicio;
    TickType_t siguiente_lectura = inicio +
        pdMS_TO_TICKS(config->nodo.intervalo_lecturas_s * 1000U);
    const TickType_t marcas_ventana = pdMS_TO_TICKS(config->nodo.ventana_activa_s * 1000U);
    TickType_t marcas_intervalo_transmision = pdMS_TO_TICKS(
        config->comunicacion.intervalo_transmision_ms);
    volatile uint32_t trabajo_cpu = 1;

    if (marcas_intervalo_transmision == 0) marcas_intervalo_transmision = 1;
    while (true) {
        const TickType_t actual = xTaskGetTickCount();
        const TickType_t transcurrido = (TickType_t)(actual - inicio);
        if (marcas_ventana != 0 && transcurrido >= marcas_ventana) break;

        if (config->nodo.intervalo_lecturas_s > 0 &&
            (int32_t)(actual - siguiente_lectura) >= 0) {
            lectura_dispositivos_leer(&config->cargas);
            siguiente_lectura = actual +
                pdMS_TO_TICKS(config->nodo.intervalo_lecturas_s * 1000U);
        }

        if (config->comunicacion.transmision_activada &&
            (TickType_t)(actual - ultima_transmision) >= marcas_intervalo_transmision) {
            (void)prueba_espnow_transmitir();
            ultima_transmision = actual;
        }

        if (config->nodo.cpu_activa) {
            trabajo_cpu = trabajo_cpu * 1664525u + 1013904223u;
        } else {
            TickType_t marcas_espera;
            if (marcas_ventana == 0) {
                marcas_espera = pdMS_TO_TICKS(1000);
            } else {
                marcas_espera = marcas_ventana - transcurrido;
            }

            if (config->comunicacion.transmision_activada) {
                const TickType_t tiempo_transcurrido_transmision =
                    (TickType_t)(actual - ultima_transmision);
                TickType_t espera_transmision;

                if (tiempo_transcurrido_transmision >= marcas_intervalo_transmision) {
                    espera_transmision = 1;
                } else {
                    espera_transmision = marcas_intervalo_transmision -
                                         tiempo_transcurrido_transmision;
                }

                if (espera_transmision < marcas_espera) {
                    marcas_espera = espera_transmision;
                }
            }
            if (marcas_espera == 0) marcas_espera = 1;
            vTaskDelay(marcas_espera);
        }
    }
    (void)trabajo_cpu;
}

void app_main(void)
{
    const configuracion_prueba_t *config = configuracion_prueba_obtener();
    configuracion_prueba_mostrar(config);
    lectura_dispositivos_mostrar(&config->cargas);

    prueba_espnow_iniciar(&config->comunicacion);
    vTaskDelay(pdMS_TO_TICKS(config->nodo.espera_inicial_s * 1000U));

    esp_err_t resultado_cargas = lectura_dispositivos_iniciar(&config->cargas);
    if (resultado_cargas != ESP_OK) {
        printf("No se pudieron iniciar las cargas: error %d\n", (int)resultado_cargas);
    } else {
        lectura_dispositivos_leer(&config->cargas);
    }

    ejecutar_ventana_activa(config);
    prueba_espnow_detener();

    if (energia_entrar_sueno_profundo(&config->sueno_profundo)) return;

    vTaskSuspend(NULL);
}

