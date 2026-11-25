# Estado actual del nodo

El CMake activo compila `app/app_main.c`. Ese archivo recupera configuración NVS, inicializa sensores, hace una sola llamada a `tecla_leer_sensores()` y retorna. No inicia ESP-NOW, no tiene ciclo de muestreo/sueño ni recibe comandos/ACK. `app_main-DESKTOP-BS27THB.c` es una variante no compilada y no describe el comportamiento activo.

Perfiles disponibles:

- Nodo 2: ESP32-C3, DHT22 y VEML7700.
- Nodo 4: ESP32-WROOM, cuatro entradas SEN0193 a través de ADS1115.

La configuración de sensores está en `configuracion/configuracion_sensores.h`. El componente activo usa `TECLA_NODO_ID` del CMake raíz para seleccionar perfil 2 o 4. El nodo aún necesita protocolo v3, rendezvous de gestión, transporte de telemetría y ACK de aplicación. Consulte `sensors/MEASUREMENT_POLICY.md` para política de rangos, filtros e intervalos de batería.
