# Contrato de radio v2

Este directorio contiene el contrato compartido por el nodo y el gateway para
las tramas binarias de la versión 2. La definición está en
[`tecla_protocol.h`](tecla_protocol.h). Es el único archivo de contrato en
este directorio.

## Qué contiene el contrato

| Tipo | Campos principales |
|---|---|
| `tecla_encabezado_trama_t` | Versión, tipo de trama, tamaño de carga, secuencia e ID de nodo. |
| `tecla_lectura_t` | ID de sensor, tipo de valor, estado de lectura y valor `float`. |
| `tecla_telemetria_t` | Estado del nodo, cantidad de lecturas, batería, intervalo y hasta ocho lecturas. |
| `tecla_comando_actuador_t` | ID del actuador y estado solicitado. |
| `tecla_comando_configuracion_t` | Intervalo, máscaras de sensores/actuadores, estado operativo y destinos. |
| `tecla_anuncio_capacidades_t` | Cantidad de sensores, actuadores disponibles e IDs de sensores. |
| `tecla_confirmacion_t` | Secuencia confirmada y resultado aceptado/rechazado. |

También define enums para tipos de trama y valor, estados del nodo y del
sensor, y destinos de lectura. Los límites declarados son ocho lecturas,
cuatro actuadores y ocho capacidades de sensor. Una función inline valida que
una configuración corresponda al nodo y a sus capacidades locales.

Una trama de telemetría identifica a un nodo en el encabezado y puede incluir
varias lecturas identificadas individualmente por `id_sensor`; por tanto, el
paquete de radio agrupa sensores del mismo nodo. Hay tres estados distintos:
el modo operativo configurado para el nodo, el resumen de estado del nodo en
telemetría y el estado de cada lectura/sensor. No deben tratarse como un único
estado.

Las estructuras están empaquetadas para su intercambio binario. Nodo y
gateway deben compilar con el mismo contrato y conservar el significado de
cada campo y enum.

## Límites actuales

Este header define tipos y una validación de configuración; no implementa
drivers, transporte ESP-NOW, serialización genérica, checksum, ACK de backend
ni persistencia. En el flujo documentado, las aplicaciones intercambian estas
estructuras por ESP-NOW y el gateway convierte las lecturas recibidas al JSON
HTTP de `POST /api/lecturas`.

El protocolo no incluye una trama independiente de heartbeat. La telemetría
transporta estado de nodo y estado de cada lectura; el gateway/backend manejan
por separado la recepción y el estado de comunicación.

## Pendientes para cerrar el contrato

- Especificación de bytes, endianness, tamaño exacto por trama y compatibilidad
  entre revisiones.
- Verificación de integridad de trama; hoy el header no define un CRC/checksum.
- Reglas de rollover/deduplicación para la secuencia y recuperación tras reboot.
- Heartbeat independiente si se requiere declarar presencia sin muestras.
- Pruebas de interoperabilidad nodo ↔ gateway con el firmware compilado.

No añadir lógica de hardware aquí; las acciones de transporte y aplicación
pertenecen a los proyectos del nodo y del gateway.
