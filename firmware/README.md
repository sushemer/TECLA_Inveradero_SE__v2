# Firmware del sistema de invernadero

Esta carpeta contiene los proyectos ESP-IDF del gateway y los nodos, además del protocolo común. Este estado describe la prueba local de los nodos 2 y 4.

## Prueba local preparada

- Nodo 2: ESP32-C3, DHT22 y VEML7700.
- Nodo 4: ESP32-WROOM, cuatro SEN0193 conectados a un ADS1115.
- Gateway: ESP32-WROOM, recepción ESP-NOW y envío HTTP a `POST /api/lecturas`.
- El gateway acepta sólo las MAC configuradas y envía ACK cuando el backend responde 2xx. La cola admite ocho tramas en RAM; un reinicio pierde las pendientes.
- El ADS1115 reporta mV sin calibrar, no porcentajes de humedad.
- Canal inicial de plantillas: 1. Router, gateway y nodos deben usar el mismo canal de 2.4 GHz.

## Pendiente

El servidor HTTP de diagnóstico, configuración remota, actuadores, CRC, persistencia de cola y pruebas físicas no forman parte de esta preparación.

Las MAC STA, credenciales Wi-Fi e IP del servidor se completan localmente antes de compilar. Las plantillas están en `nodo/src/configuracion/` y `gateway/src/config/`.
