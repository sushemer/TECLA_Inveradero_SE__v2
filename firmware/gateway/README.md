# Firmware del gateway

El gateway es un proyecto ESP-IDF independiente. Recibe telemetría de los
nodos 2 y 4 por ESP-NOW, valida el perfil y la MAC configurada, encola hasta
ocho tramas, convierte mediciones al contrato de `POST /api/lecturas` y envía
ACK únicamente cuando la API responde 2xx. Las tramas repetidas ya aceptadas
se confirman sin volver a insertarlas mientras el gateway siga encendido.

## Configuración

Copie `src/config/gateway_config.example.h` como
`src/config/gateway_config.local.h` y complete SSID, contraseña, canal y MAC
STA de los nodos. Configure `TECLA_GATEWAY_API_BASE_URL` con `idf.py menuconfig`
en la sección **TECLA gateway**, o defínala en el header local. Déjela vacía
para iniciar el AP local y ESP-NOW sin conexión STA ni POST central. Para esta configuración inicial, router,
gateway y nodos deben usar el mismo canal Wi-Fi de 2.4 GHz (la plantilla usa
el canal 1). La MAC del gateway y las MAC
de nodo se muestran en el monitor serial; si una MAC aún no está registrada,
el gateway la imprime como no autorizada.

La base es la raíz del API, por ejemplo `https://servidor.example/api` (sin
usuario, contraseña, query ni fragmento). El firmware actual concatena
`/lecturas`, por lo que usa la ruta legacy `/api/lecturas`; el cliente v1,
autenticación y asignación dinámica aún no están integrados. No hay URL/IP
central compilada por defecto. HTTPS y su validación de certificado requieren
configuración/validación ESP-IDF antes de uso de campo.

La FIFO de telemetría v2 está en RAM; los datos pendientes se pierden si se reinicia el gateway. El servidor local SoftAP WPA2 sirve `/` y `/api/nodes`, incluso si STA/Internet falta. `/api/nodes` muestra RSSI recibido, que no es estado de salud del nodo ni se envía en el JSON central. Ver `src/network/web_server/README.md` y `src/diagnostics/RSSI_SPEC.md`.

El endpoint local `/api/averages` existe, pero la ruta v2 actual no alimenta su caché; no representa promedios de las lecturas v2. No reemplaza ni guarda las lecturas originales. Este README se centra en la ruta v2.

Pendiente: manifiesto/asignación dinámica, autenticación del gateway, consumidor de la cola durable de comandos, cola durable para lecturas, compilación ESP-IDF y validación física. El endpoint central sigue siendo el contrato legacy `POST /api/lecturas`.
