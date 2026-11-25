# Modos operativos, sueño y cola de órdenes

## No mezclar estado administrativo con conexión

El estado que una persona asigna al nodo/sensor describe cómo debe trabajar. La conexión se deriva de cuándo se esperaba su siguiente ventana de comunicación y de cuándo ocurrió realmente. `OPERATIVO` no significa “conectado ahora”; `FALLANDO` tampoco debe reemplazar el estado administrativo guardado.

| Entidad / estado | Lecturas regulares | Actuadores automáticos | Qué hace al despertar | Qué se envía al servidor central |
|---|---|---|---|---|
| Nodo `OPERATIVO` | Sí, según `sample_interval` | Sí, solo si sensor, regla y actuador están habilitados | Abre rendezvous de gestión, procesa como máximo la orden permitida, lee y reporta lecturas normales | Telemetría de sensores operativos y estado/heartbeat |
| Nodo `EN MANTENIMIENTO` | Lecturas diagnósticas/locales según plan de mantenimiento; no se interpreta como nodo apagado | Pausados salvo que una tarea de servicio los habilite explícitamente | Abre rendezvous, atiende diagnóstico/configuración, lee solo lo necesario y deja claro que son datos de mantenimiento | No subir las lecturas de mantenimiento al servidor central; retenerlas localmente solo según la política de la página local del gateway |
| Nodo `INACTIVO` | No | No | Despierta por timer RTC solo para una ventana breve de gestión, consultar/recibir reactivación y volver a dormir si no hay orden | Heartbeat/estado mínimo cuando la política de conexión lo permita; ninguna lectura normal |
| Comunicación `FALLANDO` (derivada) | Depende del estado administrativo | Depende del estado administrativo; la política segura debe pausar si falta telemetría requerida | Se determina tras perder rendezvous esperados, no por una lectura antigua de forma aislada | Al reconectar, reportar recuperación y volver a sincronizar configuración |

El estado `INACTIVO` no debe apagar la posibilidad de reactivarlo para siempre: eso produciría el problema circular de no poder recibir la orden de activación. Su período de escucha es una configuración de gestión independiente de `sample_interval`; el backend calcula el vencimiento de órdenes con el período de rendezvous, tiempo de arranque, escucha y margen de reintento. No se fija aquí un valor universal de diez minutos: debe ser configurable y validarse contra consumo y latencia aceptable.

`EN MANTENIMIENTO` no significa encender y apagar rápidamente para tomar lecturas normales. Es un modo de servicio: permite lecturas de diagnóstico/locales y comunicación de gestión, pero pausa automatización y subida normal al servidor central. El gateway debe marcar cada lectura con modo/estado de origen y aplicar el filtro antes del POST central. Su página local todavía está pendiente en el firmware actual; hasta que exista, no se debe afirmar que esas lecturas se muestran o retienen allí.

El mantenimiento de un sensor es independiente del modo del nodo. Un nodo puede seguir `OPERATIVO` aunque un sensor esté `EN MANTENIMIENTO`: el gateway conserva esa lectura para visualización local, la marca con estado y no la reenvía al servidor central. Los sensores `OPERATIVO` sí se muestran localmente y se envían. Estados de nodo, sensor y actuador no se convierten automáticamente unos en otros.

## OTA y reinicio

OTA no es un estado equivalente a `EN MANTENIMIENTO`; es una operación separada. Un ESP32 en deep sleep no recibe OTA ni comandos inmediatos. La actualización remota solo podría comenzar durante una ventana en la que el nodo esté despierto, tenga el transporte requerido y cumpla sus condiciones de energía/seguridad; una actualización por USB sigue siendo una vía de servicio.

El taller P14 ofrece una distinción útil: al programar RESET, el dispositivo envía ACK antes de arrancar la cuenta; el estado del LED sí se guarda en NVS, mientras el temporizador RESET queda en RAM. Para Invernadero, “recibida” y “aplicada/ejecutada” deben ser eventos diferentes. Una acción que reinicie el nodo debe persistir su intención e ID antes de aceptar la orden y reportar su resultado después del siguiente arranque. Un actuador tampoco se considera físicamente ejecutado por enviar el ACK antes de accionarlo.

## Ciclo de sueño propuesto

1. Despertar por timer RTC; configurar salidas en estado seguro antes de activar drivers.
2. Cargar de NVS identidad, modo y configuración, y recuperar cualquier resultado de orden pendiente.
3. Inicializar radio y abrir `management_window`; el gateway puede entregar solo la cabeza de su FIFO.
4. Validar ID de nodo, versión, command ID, vencimiento, capacidades y revisión de asignación/configuración.
5. Persistir recepción/resultado en el nodo antes de contestar el ACK de aplicación correspondiente. Si ya se procesó el mismo command ID, devolver el resultado guardado sin repetir un efecto no idempotente.
6. En `OPERATIVO`, tomar lecturas en el `sample_interval`; en `EN MANTENIMIENTO`, tomar únicamente las lecturas de diagnóstico configuradas; en `INACTIVO`, no leer sensores.
7. Transmitir estado/telemetría; distinguir ACK de recepción del gateway de resultado de aplicación en nodo. Esperar timeout acotado, reintentar con el mismo command ID y secuencia, y cerrar la radio antes de dormir.
8. Programar el siguiente despertar con el período de muestreo para `OPERATIVO`/`EN MANTENIMIENTO` o con `management_poll_interval` para `INACTIVO`.

## FIFO de comandos del gateway

La FIFO nueva guarda hasta 8 órdenes de forma durable en NVS. Su estado local se alinea con el ciclo de API provisional:

`RECIBIDO_GATEWAY` (orden descargada y persistida) → `WAITING_NODE_ACK` (envío en curso) → `RECIBIDO_NODO` (ACK de aplicación, todavía no terminal) → `APPLIED` o `REJECTED`.

Si vence sin respuesta final, el estado es `NO_CONFIRMATION_UNKNOWN`: no prueba que el nodo no haya ejecutado la acción. El gateway conserva el ID, secuencia, payload, intentos y vencimiento después de reiniciar. Tras reboot vuelve a entregar la misma orden; para que eso sea seguro, las órdenes deben ser estados absolutos/idempotentes y el nodo debe recordar el command ID y resultado. La cabeza no se elimina hasta que el backend confirme que guardó el resultado. Si la FIFO está llena, se rechaza la nueva orden y no se descarta una antigua.

La cola HTTP del backend necesita una lease o redelivery: el `GET` actual marca la orden como entregada antes de que el gateway pueda confirmar que la persistió en NVS. Si el gateway pierde energía en esa ventana, sin reentrega la orden puede quedar atascada como entregada hasta expirar. El gateway también debe esperar la respuesta terminal antes de procesar la siguiente orden y aplicar una pausa configurable (1 s inicial propuesto) entre órdenes.

## Estado de implementación comprobado

- **Implementado en este cambio:** estructura FIFO NVS de 8 elementos, deduplicación del mismo command ID/contenido, estados persistidos, conservación del resultado terminal hasta reporte, recuperación de órdenes en vuelo y no borrar NVS automáticamente ante incompatibilidad.
- **Validado con harness host:** cola vacía, enqueue/dedupe, resultados aplicado/rechazado, recuperación tras reinicio, vencimiento desconocido, lleno sin descartar cabeza, checksum corrupto y versión de snapshot no soportada.
- **Pendiente de integración:** polling HTTP de múltiples órdenes, lease/reentrega del backend, parser del contrato API, envío y ACK de aplicación ESP-NOW, deduplicación/persistencia del nodo, pausa de 1 s en el consumidor, página local del gateway y medición de consumo/latencia en hardware.
- **No probado en hardware:** recepción de comandos, persistencia real en el NVS de la placa, reinicio durante una orden y comportamiento de sueño/rendezvous.

La versión activa del nodo todavía no ejecuta el ciclo de radio/órdenes descrito aquí. La validación de configuración y el protocolo existentes son piezas de soporte, no prueba de integración end-to-end.
