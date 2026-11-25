# Colas y FIFO

El gateway mantiene dos almacenes distintos:

- telemetry_queue contiene lecturas ESP-NOW en RAM, capacidad 8. Un reinicio las pierde. El firmware actual confirma al nodo después de que el backend responde 2xx.
- command_queue guarda órdenes en NVS: FIFO activa de 8 y deadletter de auditoría de hasta 8 resultados UNKNOWN_CONFIRMED. Las dos zonas tienen contadores separados.

El snapshot NVS es versión 4, con checksum y tamaño verificado. Las migraciones de esquema NVS, protocolo radio y API son independientes. Un snapshot v3 o corrupto devuelve error y no se borra ni sobrescribe. El firmware debe conservar esa partición y recuperar comandos pendientes por reconciliación con el servidor antes de actualizarla manualmente; este módulo aún no automatiza la migración v3 porque no puede inferir config_revision_target a partir del formato anterior.

## Identidad y revisión de configuración

Cada registro conserva el command_id UUID del backend, firmware_node_id uint32, queue_sequence local del gateway, payload y vencimientos. La deduplicación compara comando, nodo, tipo, payload, config_revision_expected, config_revision_target y expires_at; una diferencia para el mismo command_id se rechaza.

config_revision_expected es la última revisión aplicada que conoce el nodo; puede ser 0 si aún no se confirmó ninguna. config_revision_target es la nueva revisión deseada, debe ser mayor que expected. El ACK lleva de vuelta expected. APPLIED solo se acepta si config_revision_applied == config_revision_target. No se compara applied con expected.

## Estados del comando y ACKs

- RECEIVED_GATEWAY: el comando está guardado en la FIFO activa de NVS.
- WAITING_NODE_ACK: un dispatch de servidor autorizó un intento lógico, el gateway persistió su lease y el contador de intento, y puede transmitir una vez.
- RECEIVED_NODE: ACK de aplicación correlacionado por command_id, firmware_node_id, config_revision_expected, boot_id y seq. No es terminal.
- APPLIED / REJECTED: resultado terminal del nodo. Se conserva en NVS hasta que el servidor confirme que persistió el ACK.
- NO_CONFIRMATION_UNKNOWN: el endpoint /status confirmó que venció. No significa que el nodo no ejecutara el comando. No se reintenta ni se libera todavía la cabeza.
- UNKNOWN_CONFIRMED: el POST NO_CONFIRMATION_UNKNOWN fue confirmado durablemente por el servidor con ack_stage=UNKNOWN_CONFIRMED, result_status=UNKNOWN, gateway_queue_terminal=true y remove_reported=true. El módulo mueve la orden de la FIFO activa al deadletter y libera la cabeza sin declararla aplicada.

El ACK de UNKNOWN puede omitir boot_id y seq cuando el nodo nunca confirmó recepción. Si se incluyen, deben venir juntos y el parser/API los valida. RECEIVED, APPLIED y REJECTED requieren boot_id y seq; expected debe coincidir con el registro. APPLIED requiere además que applied coincida con target.

Un ACK terminal tardío correlacionado puede actualizar un deadletter UNKNOWN_CONFIRMED a APPLIED o REJECTED; el resultado se conserva hasta el ACK durable del backend. La API acepta esta reconciliación desde SIN_CONFIRMACION durante una ventana configurable, 30 días por omisión y entre 1 y 365 días. Tras el vencimiento, /status puede confirmar result_unknown_final=true y remove_reported=true; finalize_unknown_deadletter() retira el tombstone local porque el servidor ya cerró esa reconciliación. La fila e historial del comando permanecen en el backend.

La zona deadletter tiene capacidad fija 8. Si está llena, release_unknown() devuelve ESP_ERR_NO_MEM y conserva el UNKNOWN en la cabeza activa; no descarta auditoría ni avanza la FIFO. En producción, generar alerta al alcanzar 6/8 tombstones y monitorear la ocupación; no aceptar uso sostenido sin definir capacidad, retención y procedimiento de guardia. Para liberar espacio, consultar /status por cada UNKNOWN, esperar a que el backend confirme result_unknown_final=true y remove_reported=true al terminar la ventana tardía, ejecutar finalize_unknown_deadletter() y verificar que disminuyó el contador. Para APPLIED/REJECTED, retirar solo después del ACK durable individual del backend. Si el backend aún no confirma estado final, conservar el tombstone y escalar; no borrar NVS ni liberar FIFO manualmente.

## Lease one-shot, retry y reinicios

Para cada transmisión el consumidor debe seguir, en orden:

1. Consultar GET /commands/{id}/status; el estado pending o ttl_remaining_seconds solos no autorizan radio.
2. Solicitar un lease nuevo. El backend devuelve UUID delivery_lease_id y lease_seconds_remaining, como máximo 30 segundos y no más que el TTL.
3. Consumirlo con /dispatch. Solo HTTP 200 con delivery_authorized=true, radio_send_budget=1, delivery_lease_id, delivery_lease_expires_at, lease_seconds_remaining y delivery_attempts creciente habilita el intento lógico.
4. Convertir lease_seconds_remaining a deadline de reloj monotónico local; llamar mark_dispatch_authorized() antes de radio. El snapshot persiste el ID usado, el número de intento del servidor y el estado WAITING_NODE_ACK. Un ID repetido, vencido, de otra orden, con attempt antiguo o con budget distinto de 1 se rechaza.
5. Enviar una vez por ese dispatch y esperar ACK. Si hay timeout, llamar mark_retry(); esto solo devuelve la orden a RECEIVED_GATEWAY. La siguiente transmisión necesita status, lease y dispatch nuevos.

El lease es de un uso para un intento lógico, no garantiza exactamente una transmisión física. Puede haber reintentos entre leases; el protocolo es al menos una vez. El nodo debe deduplicar por command_id y aplicar comandos idempotentes. delivery_attempts se copia desde el servidor para reconciliar una respuesta perdida antes de persistirla localmente.

Al reiniciar, WAITING_NODE_ACK y RECEIVED_NODE vuelven a RECEIVED_GATEWAY, conservando command ID, revisiones, número de intentos, último lease y ACK del nodo. La autorización monotónica no se reutiliza: se consulta status y se obtiene otro lease. NO_CONFIRMATION_UNKNOWN permanece sin retransmitir hasta que el servidor confirme UNKNOWN_CONFIRMED.

Las secuencias de ACK son crecientes dentro de un boot_id y no envuelven: el nodo rota boot_id antes de UINT32_MAX. El ACK tardío debe usar el mismo boot y una secuencia mayor si ya había ACK previo; si no había, se guarda la identidad del ACK terminal.

## Persistencia, reintentos y límites actuales

Cada enqueue, dispatch autorizado, retry, ACK de nodo, transición a UNKNOWN, confirmación de UNKNOWN, reconciliación tardía y retiro de resultado escribe y confirma el snapshot NVS antes de actualizar RAM. La cola activa y deadletter son independientes de la caché de telemetría. La cola llena rechaza nuevas órdenes sin expulsar registros anteriores.

tecla_gateway_command_queue_retry_delay_ms() calcula backoff 1, 2, 4…60 s; TECLA_GATEWAY_COMMAND_GAP_MS define 1 s entre órdenes terminales. El consumidor actual no existe todavía, por lo que no aplica estos temporizadores ni hace el flujo HTTP/radio.

Este módulo no está conectado al sondeo HTTP, lease/dispatch, transmisión ESP-NOW, receptor de ACK ni página local. La API define el contrato, pero no hay confirmación de extremo a extremo ni prueba en hardware. Una orden no puede presentarse como recibida por el nodo solo porque llegó al gateway.
