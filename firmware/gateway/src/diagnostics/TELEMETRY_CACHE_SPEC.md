# Caché local de diagnósticos de sensores

## Estado y alcance

`telemetry_cache.{h,c}` define una caché acotada en RAM para lecturas de sensores en mantenimiento. No usa API, HTTP, NVS ni el callback ESP-NOW. Capacidad fija: 16 lecturas individuales; retención monotónica configurable al inicializar. Al llenarse, rechaza la nueva lectura y cuenta overflow; no expulsa silenciosamente una entrada aún vigente. Al vencer TTL, elimina la entrada y libera capacidad. Un reinicio del gateway pierde toda la caché.

El helper solo acepta la combinación `node_mode=OPERATIVE` + `sensor_state=MAINTENANCE`. Filtra sensores OPERATIVE/INACTIVE/RETIRED y cualquier nodo INACTIVE/MAINTENANCE. Es deliberado: mantenimiento del nodo suprime muestreo productivo y no se debe reinterpretar como mantenimiento de un sensor individual. Valores no finitos, identity/sequence inválida, slot/código desconocido y timestamps futuros se rechazan.

Cada registro conserva firmware_node_id, boot_id, seq, hora monotónica de captura, slot/measurement code, health/valor y las revisiones manifest/assignment de captura. La caché no resuelve IDs SQL ni remapea muestras; esa traducción requiere snapshot validado en gateway. No guarda RSSI.

## Integración pendiente y límites

El protocolo activo v2 no lleva `sensor_admin_state`, y el gateway actual usa IDs/MAC estáticos; no hay fuente válida para invocar esta caché desde `app_main` hoy. La página local existente solo muestra RSSI. Por eso el módulo todavía no hace visibles lecturas de mantenimiento ni participa en el ACK ESP-NOW. No alimentar la caché a partir de `sensor_health` (salud/error) ni inferir mantenimiento por una lectura inválida.

Para integración futura, gateway debe obtener el estado por sensor del manifiesto autenticado, conservar el frame/revisiones sin remapear, pasar solo lecturas explícitamente EN_MANTENIMIENTO al store, y servir un snapshot exclusivamente en la página local. Central serializer debe filtrar esas lecturas antes del POST; `LOCAL_VOLATILE_BUFFERED` solo procede después de que la lectura esté realmente aceptada en una caché local. Si hay overflow, responder sin ACK local de aceptación y exponer el contador para diagnóstico.

## Evidencia host

`tests/telemetry_cache_test.c` cubre filtro por modos administrativo, capacidad, política de overflow sin overwrite, expiración exacta, timestamps futuros, `NaN`, copia de revisiones y buffer de snapshot insuficiente. Es una prueba C host; el módulo no está incluido en CMake activo y no hay integración/página ni evidencia física.
