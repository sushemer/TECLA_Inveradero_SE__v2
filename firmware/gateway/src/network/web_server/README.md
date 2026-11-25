# Servidor web local de diagnóstico

La página local es una herramienta de diagnóstico del gateway, no una segunda plataforma web.

Rutas implementadas en `local_server.c`:

| Ruta | Contenido |
|---|---|
| `/` | Tabla RSSI por nodo, actualizada cada dos segundos. |
| `/api/nodes` | RSSI recibido por gateway: último, min/promedio/máx de ventana, cantidad y edad. |

El AP WPA2 y el servidor local se inician antes de esperar conexión STA; la página no depende de Internet ni de respuesta del backend. Credenciales de `gateway_config.example.h` son marcadores: configurar SSID/clave propios antes de operar.

La red ESP-NOW y AP deben usar el canal configurado. Si STA conecta a un router en otro canal o no conecta dentro de 60 s, el firmware deja de reconectar STA y reinicia AP+STA desconectado en el canal ESP-NOW para conservar la interfaz STA/MAC esperada por los nodos; el envío al backend queda pausado. Recuperar central tras timeout requiere reiniciar el gateway después de corregir la conectividad/canal.

`/api/status`, contadores y estado de FIFO siguen pendientes. La página solo incluye los IDs configurados actualmente en firmware (2 y 4); no representa todavía un manifiesto dinámico.
