# Especificación: RSSI ESP-NOW en diagnóstico local

## Alcance y término

Interpreto “SRS” del requisito como **RSSI** (*Received Signal Strength Indicator*). El valor se obtiene en el gateway al recibir una trama ESP-NOW; no es una medición calculada o transmitida por el nodo. Se expresa en dBm y sirve para diagnóstico orientativo del enlace recibido.

La implementación queda limitada a firmware y página local del gateway. RSSI no se agrega a manifiesto, heartbeat, telemetría central, comandos ni otro JSON enviado al servidor; tampoco se guarda muestra por muestra en NVS. La instrumentación no cambia estados administrativos, salud del sensor, decisiones de control ni automatizaciones.

## Estado actual y límites de esta etapa

- El callback activo está en `firmware/gateway/src/app/app_main.c` como `recibir_telemetria(const esp_now_recv_info_t *info, ...)`. Ahora copia RSSI y tiempo monotónico junto con la telemetría validada a la cola existente; el agregado ocurre en `procesar_cola`, fuera del callback.
- ESP-IDF invoca el callback de recepción desde la tarea Wi-Fi y recomienda que sea corto; los datos del descriptor son de uso en el callback. Por tanto, MAC, ID, RSSI y tiempo monotónico se deben copiar antes de retornar, y el cálculo de ventana/renderizado debe ocurrir en una tarea de menor prioridad.
- `firmware/gateway/src/network/web_server/local_server.c` ahora sirve `/` y `/api/nodes` desde un SoftAP WPA2. El servidor se inicia antes de esperar STA. Si STA no conecta en 60 s o conecta en otro canal, el gateway deja de reconectar STA y reinicia AP+STA desconectado en el canal ESP-NOW, conservando la interfaz STA/MAC que usan los nodos. Para recuperar central después de un timeout se requiere reiniciar el gateway; esto y la convivencia/canal requieren build IDF y verificación física antes de producción.
- La configuración de nodos continúa estática (IDs 2/4 y MACs locales). La asignación RSSI→nodo debe usar la MAC ya validada contra la configuración/manifiesto activo; no aceptar una trama desconocida como medición de un nodo registrado.

La página local y los endpoints `/` y `/api/nodes` existen ahora. AP+STA/cambio de canal no se ha probado con hardware; en un timeout STA de 60 s o canal incompatible, el gateway pausa reconexión y reinicia AP+STA desconectado al canal ESP-NOW. Tras timeout, recuperar central requiere reiniciar el gateway después de corregir el router/canal.

## Flujo de datos implementado

1. El callback valida punteros, tipo/tamaño de trama y asociación MAC→ID por la lista estática actual (nodos 2/4); no hace autoasignación por RSSI.
2. Copia MAC, ID, `rx_ctrl->rssi` y tiempo monotónico a la cola de telemetría existente; no conserva punteros del callback.
3. El worker actualiza el monitor RAM protegido por mutex. La ventana por nodo es de 16 muestras; no se escribe a NVS.
4. `/api/nodes` sirve snapshot local y la página `/` muestra RSSI último/promedio/mín/máx/cantidad/edad. Sin muestra responde null/sin dato; se marca obsoleto después de 60 s.
5. La cola RSSI todavía no es independiente: si se llena la cola de telemetría también se pierde esa observación RSSI. El callback no bloquea ni procesa el agregado.
6. Reiniciar pierde la ventana; no se persisten muestras ni agregados.

> **No usar RSSI como estado de nodo.** Es una observación puntual del receptor gateway, dependiente de una trama reciente; no prueba que el sensor esté sano ni garantiza que un comando llegue. La ausencia de RSSI reciente tampoco cambia por sí sola el estado del nodo.

## Superficie local implementada

`GET /api/nodes` y `/` sirven campos exclusivos de diagnóstico local. El endpoint implementado devuelve este shape (sin MAC porque el registry actual expone solo IDs 2/4):

```json
{"nodes":[{
  "firmware_node_id": 2,
  "rssi_dbm_last": -67,
  "rssi_age_ms": 840,
  "stale": false,
  "rssi_window": {"count":12,"min_dbm":-78,"avg_dbm":-69.5,"max_dbm":-61}
}]}
```

La página local muestra “RSSI recibido por gateway (dBm)” y la edad. Este JSON es solo local y no se reutiliza como body central. El serializador central `enviar_backend()` construye su body desde los campos de telemetría permitidos y excluye el monitor RSSI.

## Archivos implementados

| Archivo | Trabajo implementado |
|---|---|
| `firmware/gateway/src/app/app_main.c` | Captura metadata de recepción y usa un mutex breve para compartir agregado con el servidor. El callback solo copia/enfila; `enviar_backend()` no lee RSSI. |
| `firmware/gateway/src/diagnostics/rssi_monitor.h` y `.c` | Ventana RAM por firmware_node_id, last/min/promedio/máx y edad. |
| `firmware/gateway/src/network/web_server/local_server.h` y `.c` | Página `/` y endpoint local `/api/nodes`, fuera del callback ESP-NOW. |
| `firmware/gateway/src/config/gateway_config.example.h` | SSID y clave de ejemplo WPA2 para AP local; reemplazar la clave antes de uso. |
| `firmware/gateway/src/CMakeLists.txt` | Compila los módulos y declara `esp_http_server`/`esp_timer`. |
| `firmware/gateway/tests/` | Test host del agregado RSSI y guard de fuente para excluir RSSI del serializer central. |

La integración usa los archivos activos listados arriba. No modifica API, backend o web central ni la variante alternativa de laptop.

## Pruebas ejecutadas y evidencia pendiente

1. PASS host GCC: agregado `[-80,-60,-70]`, aislamiento, edad/obsolescencia, ventana llena y timestamp cero.
2. PASS source guard: callback copia RSSI/tiempo a cola y no agrega; serializer central no referencia RSSI.
3. PASS de sintaxis `local_server.c` con headers host stub; no valida API/config real de ESP-IDF.
4. Pendiente: ejecución HTTP con snapshots, AP sin STA, convivencia de canal y clientes AP en board real.
5. Pendiente: build con ESP-IDF 5.5.2 en build nuevo externo y evidencia física separada.

## Dependencia y evidencia

ESP-IDF 5.5 define `esp_now_recv_info_t.rx_ctrl` como metadata Rx de la trama y documenta que el callback de recepción corre en la tarea Wi-Fi; recomienda copiar datos necesarios a una cola y procesarlos en una tarea de menor prioridad. La guía Wi-Fi identifica RSSI como potencia recibida en dBm. Ver [ESP-NOW, ESP-IDF 5.5](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32/api-reference/network/esp_now.html) y [Wi-Fi, ESP-IDF 5.5](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32/api-reference/network/esp_wifi.html). Build IDF/hardware pendientes; ahí debe verificarse el header exacto y el comportamiento AP+STA/canal.

## Estado de esta entrega

**Vertical slice implementada; build ESP-IDF y hardware pendientes.** El callback copia RSSI (`info->rx_ctrl->rssi`) y tiempo monotónico; el worker agrega en RAM (8 nodos, 16 muestras); página `/` y endpoint `/api/nodes` muestran último, min/promedio/máx, cantidad y edad. El JSON central no incorpora RSSI. El AP inicia sin STA; tras 60 s sin STA o un canal incompatible se pausa central y se conserva AP+STA desconectado en el canal ESP-NOW. Tras timeout el central vuelve al reiniciar gateway. IDs/MAC, manifiesto dinámico, sensor health y cliente API v1 no están integrados. No hay `IDF_PATH`/`idf.py` accesible; no se compiló ESP-IDF, no se probó físicamente ni se flasheó.
