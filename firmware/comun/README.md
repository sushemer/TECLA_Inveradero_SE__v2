# Contratos compartidos
Aquí se definen datos y comportamiento puro que deben interpretarse igual en los nodos y el gateway. No se accede al hardware desde estos módulos.

## Contenido actual

- `protocolo/tecla_protocol.h`: contrato binario TECLA versión 2. Declara los tipos de trama de telemetría, comando de actuador, comando de configuración, confirmación y anuncio de capacidades. Define los campos comunes de trama(versión, tipo, tamaño de carga, secuencia e `id_nodo`), la lectura (ID de sensor, tipo de valor, estado y valor), y las cargas de cada mensaje. La configuración también incluye intervalo, máscaras de sensores/actuadores, estado operativo y destinos de lectura. El helper inline valida que una configuración recibida corresponda a este nodo y no habilite capacidades que el firmware no anuncia.
- `operacion/modo_operacion.h`: enumera los roles `nodo`, `nodo_gateway` y `gateway`, las rutas de telemetría directa/API y ESP-NOW, y los indicadores de funciones generales por rol. El modo compilado actualmente es `nodo`; la función inline devuelve esa configuración, no inicializa servicios.
- `operacion/ciclo_nodo.h` y `ciclo_nodo.c`: controlador determinista del ciclo del nodo. Modela estados y eventos de despertar, gestión, muestreo, transmisión, ACK/NACK, timeout, reintentos y decisión de sueño. Conserva en contexto el frame serializado para que los reintentos reutilicen los mismos bytes, `boot_id` y secuencia. Es lógica pura comprobable en host: quien la integra sigue siendo responsable de temporizadores, radio, persistencia y autorización de batería/sueño.

## Cambios de contrato
Si cambia el formato binario, actualizar la versión cuando corresponda y coordinar el cambio con todos los emisores, receptores y pruebas. No reutilizar un campo con otro significado. Mantener la lógica del ciclo independiente de ESP-IDF para que pueda probarse sin placa.
