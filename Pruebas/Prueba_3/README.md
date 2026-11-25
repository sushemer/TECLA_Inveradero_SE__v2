# Prueba 3: consumo del nodo

Banco de pruebas ESP-IDF para ESP32-C3 SuperMini y ESP32-WROOM-32. Permite medir CPU activa, escucha/transmisión ESP-NOW, lecturas de sensores y sueño profundo por separado.

## Estructura del firmware

```text
Prueba_3/
├── CMakeLists.txt
├── README.md
└── main/
    ├── inicio_prueba.c             # coordina la ventana activa y el estado final
    ├── Kconfig.projbuild           # opciones 0/1 agrupadas por función
    ├── configuracion/
    │   └── configuracion_prueba.c/.h # convierte Kconfig en configuración común
    ├── comunicacion/
    │   └── prueba_espnow.c/.h      # inicia, escucha, transmite y apaga ESP-NOW
    ├── sensores/
    │   ├── lectura_dispositivos.c/.h # coordina las lecturas seleccionadas
    │   ├── lectura_dht22.c/.h        # lectura digital y verificación de checksum
    │   ├── lectura_veml7700.c/.h     # configuración y lectura de luz
    │   ├── lectura_ads1115.c/.h      # conversión de una entrada analógica
    │   └── bus_i2c.c/.h              # comunicación compartida con periféricos I2C
    └── energia/
        └── sueno_profundo.c/.h     # sueño profundo y despertar por temporizador
```

`inicio_prueba.c` coordina los módulos. La configuración y las implementaciones de ESP-NOW, sensores y energía quedan separadas para reutilizarlos al desarrollar el firmware.

## Opciones del menú de configuración

Las opciones de activación son valores numéricos: **0 = apagado/no conectado**, **1 = encendido/conectado**. El ID de perfil y los tiempos son valores numéricos normales.

### 01. Nodo y ventana de medición

| Opción | Uso |
|---|---|
| `PRUEBA_ID_NODO` | Identificador 1–4. |
| `PRUEBA_PERFIL_NODO` | `1` C3 con DHT22; `2` C3 con DHT22 y VEML7700; `3` C3 con DHT22; `4` WROOM para humedad de sustrato. Solo identifica el montaje. |
| `PRUEBA_CPU_ACTIVO` | `0/1`, carga de cálculo durante la ventana. |
| `PRUEBA_TIEMPO_VENTANA_ACTIVA_S` | Segundos de actividad. `0` mantiene esta fase indefinidamente y no pasa a sueño profundo. |
| `PRUEBA_ESPERA_INICIAL_S` | Pausa de laboratorio para conectar el medidor; no representa el tiempo de arranque posterior al sueño profundo. |
| `PRUEBA_INTERVALO_LECTURAS_S` | Tiempo entre lecturas; por defecto son 600 s (10 minutos). La primera lectura ocurre al iniciar la ventana activa. |

### 02. Radio ESP-NOW

| Opción | Uso |
|---|---|
| `PRUEBA_ESPNOW_RECEPCION` | `0/1`, radio ESP-NOW activa y escuchando durante la ventana. No valida el contenido recibido. |
| `PRUEBA_ESPNOW_TRANSMISION` | `0/1`, envío broadcast opcional para medir transmisión; sin confirmación ni validación. |
| `PRUEBA_ESPNOW_CANAL` | Canal que deben compartir los equipos. |
| `PRUEBA_ESPNOW_INTERVALO_TRANSMISION_MS` | Tiempo entre paquetes cuando la transmisión está activada. |

La radio solo se inicializa cuando recepción o transmisión vale `1`. La transmisión está desactivada (`0`) por defecto. Cuando ambas opciones valen `0`, el consumo medido no incluye la radio ESP-NOW.

### 03. Sensores y cargas conectadas

| Opción | Carga que identifica |
|---|---|
| `PRUEBA_SENSOR_DHT22` | DHT22 |
| `PRUEBA_SENSOR_VEML7700` | VEML7700, en el nodo de luz |
| `PRUEBA_SENSOR_ADS1115` | ADS1115, previsto para el nodo de sustrato |
| `PRUEBA_SENSOR_SEN0193_1` … `_4` | Cada sonda SEN0193 por separado |
| `PRUEBA_CARGA_DIVISOR_BATERIA` | Divisor completo de 220 kΩ + 100 kΩ |
| `PRUEBA_CARGA_TPS63020` | TPS63020 dentro de la ruta de alimentación |

Las opciones de sensores en `1` activan las rutinas de inicialización y lectura disponibles. El código lee el DHT22, el VEML7700 y los canales habilitados del ADS1115; las opciones del divisor y TPS63020 solo describen el montaje. Los capacitores y el interruptor no requieren una bandera de consumo independiente.

El SEN0193 se lee como valor ADC crudo y milivoltios mediante su canal del ADS1115; todavía no se convierte a porcentaje de humedad. Para leer SEN0193, activa tanto su opción individual como `PRUEBA_SENSOR_ADS1115`.

### Pines provisionales para las pruebas

| Señal | ESP32-C3 SuperMini | ESP32-WROOM-32 |
|---|---:|---:|
| DHT22 datos | GPIO4 | GPIO4 |
| I2C SDA | GPIO5 | GPIO21 |
| I2C SCL | GPIO6 | GPIO22 |
| ADS1115 | Dirección 0x48; A0–A3 para SEN0193 1–4 | Dirección 0x48; A0–A3 para SEN0193 1–4 |
| VEML7700 | Dirección 0x10 | Dirección 0x10 |

Estos pines son valores iniciales tomados de la configuración previa y de conexiones habituales; confirma que coincidan con el cableado real antes de energizar. El DHT22 necesita una resistencia pull-up entre la línea de datos y 3.3 V. El VEML7700 se mide con ganancia 1 e integración de 100 ms. El voltaje del ADS1115 usa la escala configurada de ±4.096 V; son valores de lectura de prueba, no una calibración metrológica.

### 04. Sueño profundo

| Opción | Uso |
|---|---|
| `PRUEBA_USAR_SUENO_PROFUNDO` | `0/1`, activa el sueño profundo al terminar la ventana. |
| `PRUEBA_DURACION_SUENO_PROFUNDO_S` | Duración del sueño antes de reiniciar y comenzar otra corrida. |

Al finalizar la ventana, el firmware detiene ESP-NOW/Wi-Fi antes de entrar a sueño profundo. Al despertar, el chip arranca el programa desde el inicio. Si el sueño profundo vale `0`, la ventana termina y la tarea de prueba queda suspendida.

## Perfiles de los cuatro nodos

| Nodo | Placa | Cargas del perfil |
|---:|---|---|
| 1 | ESP32-C3 SuperMini | DHT22 |
| 2 | ESP32-C3 SuperMini | DHT22 y VEML7700 |
| 3 | ESP32-C3 SuperMini | DHT22 |
| 4 | ESP32-WROOM-32 | Cuatro SEN0193 y ADS1115 |

El consumo del regulador, el divisor y los sensores cuenta solo si esos componentes están conectados al circuito alimentado durante la medición. La opción del divisor únicamente registra que está conectado; la lectura de voltaje de batería aún no está implementada. Las banderas de sensores activan las lecturas, pero no cortan su alimentación: el SEN0193 conectado a 3.3 V seguirá consumiendo durante el intervalo entre lecturas.

## Combinaciones de medición

| Prueba | CPU activo | recepción | transmisión | sueño profundo | Cargas externas |
|---|---:|---:|---:|---:|---|
| CPU con radio apagada | 1 | 0 | 0 | 0 | Todas en 0 |
| Radio escuchando | 0 | 1 | 0 | 0 | Todas en 0 |
| CPU y radio escuchando | 1 | 1 | 0 | 0 | Todas en 0 |
| Transmisión opcional | 0 | 0 | 1 | 0 | Todas en 0 |
| Sueño profundo de la placa sola | 0 | 0 | 0 | 1 | Todas en 0 |
| Sueño profundo con una carga conectada | 0 | 0 | 0 | 1 | Solo la carga medida en 1 |

ESP-NOW no recibe instrucciones mientras el nodo está en sueño profundo: la radio está apagada. Para una prueba de espera, recepción debe estar en `1` y sueño profundo en `0`. La transmisión opcional no implica que se esté probando el protocolo de la aplicación.

Los tiempos se suceden: pausa inicial + ventana activa + sueño profundo + arranque. Por eso, un sueño de 600 s no produce por sí solo un intervalo de 600 s entre inicios de ventana. La pausa inicial puede ajustarse en `0` cuando el instrumento ya esté conectado.

## Comprobaciones realizadas

Las banderas y los perfiles se comprobaron en seis configuraciones C3/WROOM con cabeceras simuladas de ESP-IDF. Esta comprobación de sintaxis no sustituye una compilación real ni pruebas físicas. En este entorno no está disponible `idf.py` ni el framework, por lo que la compilación ESP-IDF queda pendiente.

## Registro de una corrida

| Dato | Valor |
|---|---|
| Nodo / perfil | |
| Placa objetivo ESP-IDF (`esp32c3` / `esp32`) | |
| Banderas CPU, recepción, transmisión y sueño profundo | |
| Cargas con bandera `1` | |
| Tensión y punto de alimentación | |
| Corriente promedio estable | |
| Pico observado | |
| Instrumento y rango | |
| Duración medida | |
| Observaciones | |

## Referencias y base del código

- [`Prueba 1: ejemplo ESP-NOW del nodo`](../Prueba_1/Nodo/main/espnow_example_main.c)
- [`Prueba 2: ESP-NOW del nodo`](../Prueba_2/Nodo_Negro/main/espnow_nodo.c), [`CMakeLists.txt`](../Prueba_2/Nodo_Negro/main/CMakeLists.txt) y [lectura DHT22 de referencia](../Prueba_2/Nodo_Negro/main/sensores.c).
- [ADC de una conversión de ESP-IDF 5.4](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/peripherals/adc_oneshot.html) y [bus I2C de ESP-IDF 5.4](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/peripherals/i2c.html).
- [Controlador ESP-IDF-lib para DHT22](https://components.espressif.com/components/esp-idf-lib/dht/versions/1.2.0/readme) y [referencia técnica del VEML7700](https://esp-idf-lib.github.io/veml7700/).
- [Ejemplo oficial ESP-NOW de ESP-IDF](https://github.com/espressif/esp-idf/tree/master/examples/wifi/espnow) y [guía oficial ESP-NOW](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_now.html).
- [Modos de sueño ESP32-C3](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/system/sleep_modes.html): El sueño profundo apaga los periféricos inalámbricos y reinicia la aplicación al despertar.
- [Optimización del tiempo de arranque ESP32-C3](https://docs.espressif.com/projects/esp-idf/en/release-v5.4/esp32c3/api-guides/performance/speed.html): opciones de registro y del cargador de arranque para reducir el tiempo de arranque.
- Hojas de datos de [ESP32-C3](https://www.espressif.com/sites/default/files/documentation/esp32-c3_datasheet_en.pdf), [ESP32](https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf) y [ESP32-WROOM-32](https://www.espressif.com/sites/default/files/documentation/esp32-wroom-32_datasheet_en.pdf).
