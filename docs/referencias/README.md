# Referencias técnicas del sistema embebido

Esta carpeta reúne las fuentes que se usarán para diseñar y validar el
firmware. Todavía no contiene código del proyecto. Las referencias están
separadas para evitar copiar una solución completa sin comprobar que coincida
con nuestro hardware y protocolo.

## Cómo interpretar las referencias

| Categoría | Uso en este proyecto |
|---|---|
| Base oficial | Documentación o ejemplo de Espressif, TI, Vishay o DFRobot que define el comportamiento que debemos respetar. |
| Repositorio de apoyo | Código de terceros que sirve para estudiar una integración concreta; no se copiará automáticamente. |
| Video de apoyo | Explicación visual de conexiones o pruebas; no sustituye una hoja de datos ni la documentación de ESP-IDF. |
| Pendiente de decisión | Tema que requiere confirmar el componente comprado, la API o una prueba de hardware. |

## Índice

- [Sensores y ADC](sensores.md)
- [ESP-NOW y Wi-Fi local](esp-now-wifi.md)
- [Energía, batería y sueño](energia-sueno.md)
- [Seguridad e incorporación de nodos](seguridad-incorporacion.md)
- [APIs HTTP y página local](apis.md)
- [Pruebas y validación](pruebas.md)

## Orden recomendado de consulta

1. Confirmar el modelo físico del sensor.
2. Leer la hoja de datos y la referencia oficial.
3. Ejecutar el ejemplo aislado con un solo dispositivo.
4. Registrar valores, errores y consumo.
5. Integrar el driver en el nodo sin mezclarlo todavía con ESP-NOW.
6. Integrar el protocolo y la API después de validar la lectura local.

## No se debe copiar todavía

No se debe copiar un proyecto completo de Internet al firmware. Los ejemplos
pueden usar Arduino, otro modelo de ESP32, otro sensor, otra unidad o una
política de reintentos distinta. Primero se comparará cada referencia con el
contrato de [`firmware/comun/protocolo/`](../../firmware/comun/protocolo/), el
hardware comprado y la API de CimaInvernadero.
