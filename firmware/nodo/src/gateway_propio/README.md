# Nodo con capacidad futura de gateway

Esta carpeta reserva la función de una placa híbrida que lee sus propios
sensores y, más adelante, puede recibir telemetría de otros nodos.

## Modo actual

El firmware se ejecuta como `TECLA_MODO_NODO`:

- Lee únicamente los sensores conectados a esta placa.
- Envía únicamente su propia telemetría.
- No recibe datos de otros nodos.
- No crea una cola para datos ajenos.
- No cambia el `id_nodo` de sus lecturas.

## Modo futuro `TECLA_MODO_NODO_GATEWAY`

```text
Sensores propios ───────────────> telemetría propia ─> API
Otros nodos ─> ESP-NOW ─> cola ──> API
```

La recepción de otros nodos no debe habilitarse hasta implementar MAC
autorizadas, relación nodo-ubicación, validación, cola, deduplicación, ACK
posterior al almacenamiento local y reintentos hacia la API.

El gateway dedicado continúa en `firmware/gateway/`; esta carpeta solo reserva
la extensión para una placa que combine nodo y concentrador.
