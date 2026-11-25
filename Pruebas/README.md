# Pruebas del proyecto embebido
Esta carpeta reúne las pruebas realizadas y las que todavía deben ejecutarse antes de la versión final. El firmware no se modifica desde este documento; aquí se registran procedimientos, resultados y evidencias.

## Pruebas realizadas o disponibles

### Prueba 1: comunicación ESP-NOW de referencia
La carpeta `Prueba_1` conserva los ejemplos iniciales de nodo, gateway y lectura de MAC. Sirven para comprobar la configuración básica de ESP-IDF, Wi-Fi y ESP-NOW, incluyendo envío broadcast y unicast.

Estas pruebas son de referencia y no sustituyen la validación con los sensores reales del proyecto.

### Prueba 2: nodo, gateway y ACK
La carpeta `Prueba_2` contiene una prueba más cercana al sistema actual:
- Nodo con lectura ADC y DHT22.
- Paquete con temperatura, humedad, VPD y voltaje.
- Envío mediante ESP-NOW al gateway.
- Validación de MAC y tamaño del paquete.
- Respuesta ACK con número de secuencia.
- Medición básica del RTT del ACK.

El código y los README originales se conservan en sus carpetas. Antes de marcar esta prueba como final debe repetirse con las placas y sensores reales del prototipo.

### Prueba 3: banco de consumo
[`Prueba_3`](Prueba_3/README.md) contiene un firmware ESP-IDF modular para ESP32-C3 y ESP32-WROOM-32. Sus banderas 0/1 permiten aislar CPU activa, escucha ESP-NOW, transmisión opcional y sueño profundo. También incluye lecturas de DHT22, VEML7700 y ADS1115/SEN0193; las banderas del divisor y TPS63020 solo registran el montaje.

Esta prueba no valida el envío de datos de aplicación, la calibración de sensores ni el firmware final. La compilación y las mediciones físicas con las placas siguen pendientes.

### Evidencias existentes
En `Talleres/Taller_5/imgs/` se conservan evidencias de circuito, MAC, comunicación, salida, transmisiones, envío fallido, lectura inválida y solicitudes de datos. Deben relacionarse con fecha, hardware y versión concreta al preparar el informe final.

## Pruebas pendientes
### ESP-NOW con sensores reales
1. Conectar el sensor físico definitivo al nodo.
2. Registrar una medición real.
3. Construir y enviar el paquete mediante ESP-NOW.
4. Confirmar recepción, validación y ACK en el gateway.
5. Verificar que el valor recibido coincida con la lectura local.
6. Repetir con el sensor desconectado o con una lectura inválida.

La evidencia debe incluir fotografía del montaje, monitor serial del nodo y gateway, fecha, MAC, canal ESP-NOW y versión del firmware.

### Medición individual del consumo
Debe medirse cada dispositivo por separado:

- Nodo en reposo.
- Nodo leyendo sensores.
- Nodo transmitiendo por ESP-NOW.
- Nodo esperando ACK.
- Gateway recibiendo ESP-NOW.
- Gateway con Wi-Fi activo.
- Gateway enviando datos al backend.
- Sensor o actuador conectado, cuando corresponda.

Para cada medición registrar:

| Campo | Registro |
|---|---|
| Dispositivo | Nodo, gateway, sensor o actuador |
| Alimentación | Voltaje aplicado |
| Estado | Reposo, lectura, transmisión o recepción |
| Corriente mínima | Valor medido |
| Corriente promedio | Valor medido |
| Corriente máxima | Valor medido |
| Duración | Tiempo de la prueba |
| Instrumento | Multímetro, fuente o medidor |
| Fecha y configuración | Placa, firmware y sensores |

No se debe calcular autonomía usando únicamente el consumo teórico. Primero deben obtenerse mediciones reales y repetibles.

### Otras pruebas pendientes

- Pérdida de comunicación ESP-NOW y reintentos.
- Gateway sin conexión Wi-Fi.
- Gateway sin conexión con el backend.
- Paquete duplicado o con secuencia incorrecta.
- Sensor desconectado.
- Reinicio del nodo y conservación de configuración.
- Activación y desactivación del actuador, si aplica.
- Prueba de alcance y estabilidad dentro del invernadero.

## Estado resumido

| Área | Estado |
|---|---|
| Ejemplo ESP-NOW | Realizado como prueba de referencia |
| Nodo y gateway con ACK | Implementado como prueba de desarrollo |
| ESP-NOW con sensores reales definitivos | Pendiente de repetir y documentar |
| Consumo individual de dispositivos | Pendiente |
| Consumo durante ciclos completos | Pendiente |
| Pruebas de reconexión | Pendiente de evidencia completa |

Cada nueva prueba debe agregarse aquí con su resultado y enlazarse a la evidencia correspondiente.
