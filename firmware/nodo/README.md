# Firmware de prueba de nodos 2 y 4

Perfiles del proyecto (desde esta carpeta):

```powershell
idf.py -B build-nodo2 set-target esp32c3 build
idf.py -B build-nodo4 -D "SDKCONFIG=sdkconfig.node4" -D "IDF_TARGET=esp32" -D "TECLA_NODO_ID=4" build
```

El Nodo 2 usa ESP32-C3, DHT22 y VEML7700. El Nodo 4 usa ESP32-WROOM, cuatro SEN0193 y ADS1115. El código continúa leyendo aunque no se haya configurado radio; en ese modo imprime la MAC STA local por UART.

## Preparación de radio

Copie `src/configuracion/node_config.example.h` a `src/configuracion/node_config.local.h` y ponga la MAC STA del gateway en `TECLA_GATEWAY_STA_MAC`. El canal inicial es 1 y debe coincidir entre router, gateway y nodos. No compile para flashear hasta poner la MAC real del gateway.

Secuencia: iniciar el servidor local y anotar la IP del PC; configurar, compilar y flashear el gateway; leer su MAC STA por UART; configurar, compilar y flashear Nodo 2; leer su MAC STA; agregarla a la whitelist del gateway y volver a compilar/flashear el gateway. Para probar Nodo 4, repetir la misma configuración de nodo y registrar su MAC en la whitelist.

Las lecturas del ADS1115 se reportan en mV sin calibrar, no como porcentaje de humedad. Esta compilación no valida físicamente los sensores ni el alcance de radio.
