# Referencias de ESP-NOW, Wi-Fi y página local

## ESP-NOW como enlace nodo-gateway

### Base oficial

- [API oficial ESP-NOW de ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_now.html): inicialización, peers, callbacks, canales, ACK de envío y seguridad.
- [Ejemplo oficial ESP-NOW de ESP-IDF](https://github.com/espressif/esp-idf/tree/master/examples/wifi/espnow): ejemplo base con dos ESP, callbacks, secuencia, CRC, peer y canal.
- [Código principal del ejemplo oficial](https://github.com/espressif/esp-idf/blob/master/examples/wifi/espnow/main/espnow_example_main.c): muestra que el callback debe colocar el trabajo en una cola y no ejecutar operaciones largas.
- [Componente oficial espressif/esp-now](https://github.com/espressif/esp-now): pairing, seguridad, control y funciones de nivel superior.
- [Guía de usuario del componente espressif/esp-now](https://github.com/espressif/esp-now/blob/master/User_Guide.md): referencia para pairing y provisión futura.

### Qué tomaremos del ejemplo

- El gateway y el nodo deben usar el mismo canal.
- El nodo debe conocer la MAC del gateway para comunicación unicast normal.
- Cada paquete tendrá tipo, versión, longitud, secuencia y checksum.
- La callback de recepción solo valida lo mínimo y coloca el mensaje en una cola.
- El ACK de radio confirma únicamente el tramo nodo-gateway, no la API.

### Video complementario

- [Introducción a ESP-NOW en ESP-IDF](https://www.youtube.com/watch?v=VGoiUk-jkjE): útil para visualizar pairing, callbacks y roles; la implementación final se basará en la documentación oficial.

## Wi-Fi del gateway

El gateway tendrá dos responsabilidades separadas:

```text
STA → red existente / Internet / API
AP  → página local de diagnóstico y configuración
ESP-NOW → nodos
```

La caída de STA o de Internet no debe apagar el AP ni ESP-NOW. El gateway
debe seguir mostrando su estado y acumulando paquetes pendientes.

- [Ejemplos oficiales de Wi-Fi de ESP-IDF](https://github.com/espressif/esp-idf/tree/master/examples/wifi): STA, AP, escaneo y ahorro de energía.
- [Ejemplo oficial de conexión Wi-Fi](https://github.com/espressif/esp-idf/tree/master/examples/wifi/getting_started): base para STA y SoftAP.

## Página local

La página local no es la misma que la plataforma web de CimaInvernadero. Su
función inicial será diagnóstico y configuración local:

- Estado de AP, STA e Internet.
- Canal y estado de ESP-NOW.
- Nodos registrados y MAC autorizadas.
- Última lectura por nodo.
- Cola pendiente y errores.
- Incorporación de un nodo nuevo.

- [Servidor HTTP oficial de ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_http_server.html): rutas GET/POST y manejadores.
- [Ejemplo HTTP Server simple](https://github.com/espressif/esp-idf/tree/master/examples/protocols/http_server/simple): base mínima de rutas.
- [Ejemplo oficial RESTful Server](https://github.com/espressif/esp-idf/tree/master/examples/protocols/http_server/restful_server): referencia para una API local y página embebida.
