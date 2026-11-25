# Referencias de APIs y formatos de comunicación

## Capas que no deben mezclarse

```text
Sensor → nodo
nodo → gateway       ESP-NOW
gateway → API        HTTPS/JSON
web local → gateway  HTTP local
web principal → backend/API
```

ESP-NOW no debe enviar JSON de la API directamente. El nodo y el gateway usan
el contrato binario de [`firmware/comun/protocolo/`](../../firmware/comun/protocolo/);
solo el gateway construye el JSON hacia el backend.

## Gateway como cliente de CimaInvernadero

La referencia histórica del repositorio anterior contiene el endpoint:

```text
POST https://cimainvernadero-production.up.railway.app/api/lecturas
```

Esta URL debe verificarse nuevamente contra la rama actual de CimaInvernadero
antes de implementarla, porque el backend está cambiando.

El formato de referencia discutido es:

```json
{
  "dispositivo_id": 12,
  "bateria_porcentaje": 87,
  "lecturas": [
    {
      "tipo_dato_id": 1,
      "dato": 24.8
    }
  ]
}
```

No se debe cerrar todavía el nombre de `sensor_id`, `tipo_dato_id`, ubicación,
estado y secuencia sin revisar el contrato vigente de la API. El gateway debe
conservar internamente más información que la que se envía inicialmente a la
API: MAC, secuencia, estado de sensor, ubicación y ruta de llegada.

## Base oficial para enviar

- [API `esp_http_client` de ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_http_client.html): cliente HTTP/HTTPS, encabezados, cuerpo, respuestas y timeout.
- [Ejemplo oficial `esp_http_client`](https://github.com/espressif/esp-idf/tree/master/examples/protocols/esp_http_client): referencia para solicitudes.
- [Ejemplo oficial HTTP por sockets](https://github.com/espressif/esp-idf/tree/master/examples/protocols/http_request): útil para entender GET y respuesta HTTP, no será la primera opción del firmware.
- [FAQ oficial de HTTP de Espressif](https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/protocols/http.html): casos de POST y transferencia de datos.

## Operaciones que debemos definir

### Enviar lecturas

```text
POST /api/lecturas
```

El gateway debe considerar confirmada la entrega solo ante una respuesta HTTP
exitosa y válida. Si la API no responde, la lectura queda pendiente en la cola.

### Consultar configuración

La ruta exacta todavía debe confirmarse con CimaInvernadero. Conceptualmente:

```text
GET /api/dispositivos/{id}
GET /api/dispositivos/{id}/configuracion
```

No se deben implementar estas rutas por suposición. Primero hay que revisar
los controladores reales del backend.

### Enviar configuración al nodo

La configuración no debe ir directamente desde Internet al nodo. El flujo es:

```text
Página/API → gateway → configuración pendiente → nodo al despertar → ACK
```

### Consultar el gateway local

Estas rutas ya están previstas en la documentación del gateway y son locales:

| Método | Ruta | Función |
|---|---|---|
| GET | `/api/status` | Estado de AP, STA, Internet, ESP-NOW y backend. |
| GET | `/api/nodes` | Último estado de cada nodo autorizado. |
| GET | `/api/diagnostics` | Contadores, errores, reintentos y tiempos. |

## Videos de apoyo

- [HTTP Client con ESP-IDF y POST](https://www.youtube.com/watch?v=8uBIBfUBCt8): apoyo visual para configurar el cliente.
- [ESP32 HTTP POST y JSON](https://www.youtube.com/watch?v=3EQFWapZQuE): solo como introducción general; el código final usará `esp_http_client` oficial.
