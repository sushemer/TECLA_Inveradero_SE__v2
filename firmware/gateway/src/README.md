# Estructura del gateway (flujo v2)

`src/CMakeLists.txt` registra el componente ESP-IDF. El punto de entrada es
`app/app_main.c`, que coordina Wi-Fi, ESP-NOW, HTTP y la página local.

| Ruta | Función y estado |
|---|---|
| `app/app_main.c` | Recibe y valida tramas, mantiene la cola temporal de telemetría, envía lecturas a la API y confirma al nodo después del HTTP 2xx. También contiene hoy la coordinación Wi-Fi. |
| `config/gateway_config.example.h` | Plantilla de SSID, AP, MAC de nodos y mapeo de tipos de lectura. |
| `queue/command_queue.c`, `.h` | Primitivas de cola durable de comandos en NVS. Están compiladas e inicializadas, pero falta conectarlas a la consulta HTTP, el despacho ESP-NOW y los ACK de comandos. |
| `diagnostics/rssi_monitor.c`, `.h` | Agrega en RAM el RSSI observado al recibir tramas; lo muestra la página local. |
| `diagnostics/reading_average.c`, `.h` | Código de promedio en RAM, pero la ruta v2 actual no lo alimenta; `/api/averages` no representa promedios de telemetría v2. |
| `diagnostics/telemetry_cache.c`, `.h` | Diseño/código de caché pendiente; no está en `src/CMakeLists.txt` ni en el flujo activo. |
| `network/gateway_api_url.c`, `.h` | Construye la URL para el endpoint legacy `/lecturas`. |
| `network/web_server/local_server.c`, `.h` | Página local SoftAP: `/` y `/api/nodes` para diagnóstico RSSI. `/api/averages` existe, pero no se alimenta con el flujo v2. |
| `network/wifi/README.md` | Nota de diseño; todavía no hay un módulo fuente Wi-Fi separado. |
| `../../comun/protocolo/tecla_protocol.h` | Contrato binario v2 compartido con el nodo. Ver [README del protocolo](../../comun/protocolo/README.md). |

## Recorrido v2 actual

```text
Nodo --ESP-NOW--> Gateway --HTTP POST /api/lecturas--> Backend
       <--ACK después de HTTP 2xx--
```

El gateway acepta nodos 2 y 4 mediante MAC configuradas localmente. La cola
de telemetría admite ocho tramas en RAM y se pierde si se reinicia el gateway.
El ACK confirma respuesta exitosa de la API; el callback de radio por sí solo
no confirma que el backend haya guardado la lectura.

## Pendientes

- Alta dinámica y manifiesto de nodos, gateways y sensores.
- Autenticación y validación de identidad en la API.
- Consumidor de la cola durable de comandos y confirmación de aplicación en el nodo.
- Persistencia de telemetría recibida antes de confirmar al nodo.
- Conexión del promedio local con las lecturas que pasan por la ruta v2.
- Build ESP-IDF y verificación en hardware.

Los directorios `communication/esp_now/`, `storage/` y `telemetry/` están
vacíos en este checkout. `gateway/main/` también está vacío: ESP-IDF usa
`src/` como componente. No representan funcionalidades implementadas.
