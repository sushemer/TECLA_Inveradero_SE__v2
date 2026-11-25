# Sensores implementados para la prueba local

El perfil de hardware/capabilities se selecciona con `TECLA_SENSOR_PROFILE` (`DHT22_ONLY`, `DHT22_VEML7700` o `SEN0193_4CH_DECLARED`). `TECLA_NODO_ID` se conserva por compatibilidad con el encabezado del protocolo v2 y es independiente del perfil. Ninguno de los dos valores es `firmware_node_id` ni un ID SQL.

| Perfil | Sensores | Placa/pines I²C |
|---|---|---|
| Nodo 2 | DHT22 y VEML7700 | ESP32-C3; SDA GPIO5, SCL GPIO6 |
| Nodo 4 | Cuatro SEN0193 declarados; driver ADS1115 aún no integrado en la lectura activa | ESP32-WROOM; SDA GPIO21, SCL GPIO22 |

El DHT22 usa GPIO4. El ADS1115 está previsto en `0x48`; el VEML7700, en `0x10`.
La ruta activa del nodo 4 todavía no inicializa ni lee el ADS1115 y entrega
estado de error/`NAN` para SEN0193. No hay escala de milivoltios ni calibración
de porcentaje de humedad implementadas.

Los pines GPIO6–GPIO11 del ESP32-WROOM están reservados por la memoria flash;
no se deben usar para el bus I²C de ese perfil. El ESP32-C3 sí usa GPIO5/6 en
esta configuración.

El lector activo maneja DHT22 y VEML7700 en el perfil nodo 2. DHT22 usa una
transacción por ciclo y VEML7700 una conversión con integración de 100 ms; no se
aplica un burst universal. El envío de telemetría por ESP-NOW y el manejo de ACK
aún no están conectados a este lector.
