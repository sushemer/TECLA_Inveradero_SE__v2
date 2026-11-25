# Diagnóstico del gateway

## RSSI

`rssi_monitor.c` conserva en RAM muestras RSSI recibidas por ESP-NOW. La página
local `/` y `GET /api/nodes` muestran el valor reciente y estadísticas de
ventana. RSSI es la intensidad de señal observada por el gateway; no confirma
por sí sola la salud del nodo ni de sus sensores.

## Promedios locales

`reading_average.c` mantiene datos de promedio en RAM y el servidor local
expone `/api/averages`. Sin embargo, **la ruta v2 activa no alimenta esa
caché**, por lo que hoy el endpoint no ofrece promedios del flujo de lecturas
v2. Integrarla con v2 es trabajo pendiente. Cuando contiene muestras, son
temporales, no reemplazan lecturas originales y se pierden al reiniciar.

## Caché de telemetría

`telemetry_cache.c` y `.h` junto con `TELEMETRY_CACHE_SPEC.md` están en
preparación. Estos fuentes no están registrados en `src/CMakeLists.txt` y no
forman parte del firmware compilado actualmente.
