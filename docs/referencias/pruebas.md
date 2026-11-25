# Referencias y plan de pruebas

## Repositorios base

| Tema | Base principal | Qué se toma | Qué no se copia sin validar |
|---|---|---|---|
| ESP-IDF | [espressif/esp-idf](https://github.com/espressif/esp-idf) | APIs y ejemplos oficiales | Arquitectura completa de otro proyecto |
| ESP-NOW | [ejemplo ESP-NOW de ESP-IDF](https://github.com/espressif/esp-idf/tree/master/examples/wifi/espnow) | Canal, peer, callback, cola y ACK de ejemplo | Su protocolo de aplicación completo |
| ESP-NOW avanzado | [espressif/esp-now](https://github.com/espressif/esp-now) | Pairing, seguridad y control | Usarlo como reemplazo automático del protocolo TECLA |
| DHT22 | [esp-idf-lib/dht](https://github.com/esp-idf-lib/dht) | Driver ESP-IDF y checksum | Intervalos o pines del ejemplo |
| VEML7700 | [esp-idf-lib/veml7700](https://github.com/UncleRus/esp-idf-lib/tree/master/components/veml7700) | I2C, integración y lux | Confundir lux con PAR |
| SEN0193 | [DFRobot Wiki](https://wiki.dfrobot.com/sen0193/) | Cableado y calibración | Porcentaje universal sin calibración local |
| HTTP local | [ESP-IDF RESTful Server](https://github.com/espressif/esp-idf/tree/master/examples/protocols/http_server/restful_server) | Rutas y servidor local | Su interfaz web completa |
| HTTP cliente | [ESP-IDF esp_http_client](https://github.com/espressif/esp-idf/tree/master/examples/protocols/esp_http_client) | POST, timeout y respuesta | Endpoint de demostración |

## Pruebas por etapa

### Etapa 1: sensores aislados

- DHT22 correcto, checksum incorrecto y desconexión.
- SEN0193 en aire, sustrato seco, sustrato húmedo y agua solo como referencia.
- ADS1115 en cada canal por separado.
- Sensor de luz en oscuridad, interior y exterior.
- Repetibilidad y valores fuera de rango.

### Etapa 2: nodo sin comunicaciones

- Leer cada 10 minutos.
- Mostrar valor, unidad y estado.
- Medir consumo despierto y en sueño.
- Reiniciar y comprobar recuperación de configuración.

### Etapa 3: ESP-NOW mínimo

- MAC conocida.
- Canal igual en ambos dispositivos.
- Un paquete de prueba.
- ACK correcto, timeout y repetición.
- Paquete desconocido rechazado.

### Etapa 4: incorporación local

- Solicitud nueva.
- MAC no registrada.
- Código incorrecto.
- Registro desde la web local.
- Configuración pendiente hasta que el nodo despierte.
- ACK de configuración aplicada.

### Etapa 5: gateway y API

- Sin Internet.
- API caída.
- Respuesta HTTP inválida.
- Duplicado por reintento.
- Reinicio del gateway con cola pendiente.
- Confirmación de que ESP-NOW aceptado no significa API aceptada.

## Resultado esperado

Cada prueba debe registrar: fecha, placa, sensor, alimentación, pines,
firmware, resultado, error observado, consumo y evidencia. La calibración del
SEN0193 debe conservar los valores usados y no solo el porcentaje final.
