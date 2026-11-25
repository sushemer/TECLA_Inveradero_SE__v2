# Referencias de alimentación, consumo y sueño

## Principio de operación

Un nodo alimentado por batería debe hacer lo siguiente:

```text
despertar → inicializar → leer → validar → comunicar → esperar ACK limitado → dormir
```

No debe permanecer esperando indefinidamente una respuesta del gateway. Si no
hay respuesta, registra el fallo, aplica el límite de reintentos y vuelve a
dormir para intentar en el siguiente ciclo.

## Referencias oficiales

- [Modos de sueño de ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/sleep_modes.html): deep-sleep, light-sleep y fuentes de despertar.
- [Ejemplo oficial de deep-sleep](https://github.com/espressif/esp-idf/tree/master/examples/system/deep_sleep): temporizador, causa de despertar y ciclo mínimo.
- [Ejemplo oficial de ahorro Wi-Fi](https://github.com/espressif/esp-idf/tree/master/examples/wifi/power_save): ahorro sin desconectar completamente la aplicación.
- [NVS de ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html): configuración persistente, identidad, secuencia e intervalo.

## Decisiones para el proyecto

- Intervalo inicial: 10 minutos.
- Antes de recibir la primera configuración: intervalo de incorporación corto,
  limitado para no gastar batería indefinidamente.
- El nodo no conservará una cola histórica completa en la primera versión.
- La configuración más reciente reemplazará la anterior.
- El intervalo efectivo y el estado de batería deben formar parte del estado
  del nodo, no de una suposición del gateway.

## Riesgos a probar

- La placa de desarrollo puede consumir más que el ESP32 desnudo por LED,
  regulador y convertidor USB.
- ESP-NOW y Wi-Fi no mantienen conexiones durante deep-sleep.
- El nodo debe reinicializar sensores, radio y configuración al despertar.
- El SEN0193 puede consumir varios miliamperios; conviene estudiar si se
  alimentará solo durante la lectura.
