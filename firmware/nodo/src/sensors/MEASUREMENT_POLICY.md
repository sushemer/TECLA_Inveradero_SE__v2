# Política de medición y energía

## Estado observado del firmware activo

El `src/CMakeLists.txt` activo compila `app/app_main.c`; este archivo inicializa configuración y sensores, hace una sola llamada a `tecla_leer_sensores()` y retorna. No hay ciclo de muestreo, deep sleep, rendezvous, ESP-NOW o espera de ACK en ese target. `app_main-DESKTOP-BS27THB.c` no es compilado y no sirve como evidencia de comportamiento activo.

El perfil activo del nodo 2 usa DHT22 por el driver `dht` y VEML7700 por I²C. El VEML7700 se configura para integración de 100 ms y el driver espera 120 ms antes de leer una conversión. Los perfiles SEN0193 están declarados, pero el lector activo produce estado de error con `NAN`; no existe todavía una lectura ADS1115 utilizable en el ciclo activo.

## Validación local por tipo

`sensor_validation.c` aplica:

- Temperatura: valor finito entre −40 y 80 °C.
- Humedad relativa: valor finito entre 0 y 100 %.
- Luz VEML7700: valor finito no negativo. No se fija máximo sin considerar ganancia, integración y saturación de la configuración del sensor.
- Humedad de suelo y tipos no implementados: rechazados como no plausibles hasta que exista driver, unidad, rango documentado y calibración.

Estas comprobaciones detectan `NaN`, infinito y valores claramente inválidos; no convierten unidades, no compensan offset y no sustituyen calibración de instalación. El manifiesto/API v1 todavía no lleva rangos por tipo de dato, por lo que no se confunde este filtro local con validación central.

## Selección del método de muestreo

| Método | Robustez | Costo y retardo | Uso aquí |
|---|---|---|---|
| Una conversión | No elimina un valor atípico aislado; conserva cambios rápidos. | Menor número de transacciones y menor latencia/energía de adquisición. | DHT22: una transacción del driver por ciclo. VEML7700: una conversión después de su integración de 100 ms. |
| Mediana de N conversiones | Rechaza impulsos/outliers; útil ante ruido impulsivo. | N adquisiciones, almacenamiento/sort y latencia acumulada; la energía crece con las conversiones y esperas. | No aplicar a DHT22 ni al VEML7700 por defecto. Considerar en una lectura ADC solo tras medir tasa, ruido y tiempo de asentamiento. |
| Media recortada | Suaviza ruido distribuido y tolera algunos extremos. | N adquisiciones, acumulación/ordenamiento y descarte de extremos; retarda la lectura y puede borrar transitorios reales. | Opción experimental para ADS1115 si las mediciones sintéticas y de banco prueban mejora; aún no implementada ni seleccionada. |

No se fija un N universal. Los tests host usan entradas sintéticas para verificar rangos, finitud y decisiones de política; no representan ruido, error, batería ni precisión de un sensor físico.

## Pendiente crítico: porcentaje de batería, intervalo y sueño

### 1. Regla de intervalo solicitada

`tecla_battery_interval_for_percent()` es una función pura que aplica esta tabla al porcentaje **ya medido**:

| Porcentaje válido | Intervalo de muestreo base |
|---:|---:|
| `>50%` | 600 s (10 min) |
| `>25%` y `<=50%` | 1200 s (20 min) |
| `>10%` y `<=25%` | 2400 s (40 min) |
| `0..10%` | 3600 s (60 min) |

Los límites son exactos: 50→20 min, 25→40 min y 10→60 min. Porcentaje ausente devuelve `NO_MEASUREMENT`; `NaN`, infinito y valores fuera de 0–100 devuelven `INVALID_PERCENT`, sin intervalo de reemplazo. La función no convierte voltaje a porcentaje.

### 2. Fuente y calibración física

El firmware activo no contiene adquisición ADC de batería ni un divisor/calibración que produzca ese porcentaje. El campo porcentual no se debe sintetizar desde voltaje sin química y cantidad de celdas, curva bajo carga, divisor, referencia/calibración ADC y comportamiento del regulador. Por ahora, los valores de porcentaje usados por tests son sintéticos y no prueban medición ni autonomía.

El helper independiente `tecla_battery_policy_check()` conserva una segunda puerta de autorización de deep sleep basada en umbral mínimo de mV por intervalo. Por defecto esos umbrales están marcados **no calibrados** y la función niega dormir; seleccionar 60 min por porcentaje bajo no autoriza por sí solo el sueño. Ningún umbral de mV de prueba es un valor de producto.

### 3. Conexión al ciclo activo

`firmware/comun/operacion/ciclo_nodo.c` define una máquina de estados C pura host-testable: rendezvous de gestión con evento de cierre/timeout, muestreo solo en modo de nodo operativo, envío, espera de ACK de aplicación correlacionado por boot/seq, backoff y número acotado de reintentos, y decisión de sueño autorizada/denegada. Callback MAC exitoso no cierra la espera. Callback/timeout de intento viejo se ignora. `SAMPLE_READY` copia el frame serializado a un buffer RAM de hasta 250 bytes; cada reintento debe enviar ese mismo buffer con el mismo boot/seq. No serializa ni persiste el frame en NVS.

La máquina distingue modo administrativo **del nodo**: mantenimiento e inactivo omiten muestreo productivo y aún pasan por rendezvous. No modela estado de mantenimiento **del sensor** ni su lectura diagnóstica local-only; caché y filtro del gateway siguen pendientes y no deben confundirse con el estado del nodo. Al negar deep sleep, el módulo solicita armar una espera de gestión acotada. El llamador debe proveer un deadline finito y el siguiente rendezvous; el módulo puro no mantiene radio ni temporizadores por sí mismo.

La máquina es host-only: no se incluye en CMake activo ni se llama desde `app_main.c`, que todavía inicializa, lee sensores una vez y retorna. No existe ciclo conectado de despertar→medir porcentaje→muestrear→radio/ACK→dormir, ni llamada a ADC o `esp_deep_sleep_start()`. La política de porcentaje y puerta de sueño tampoco están conectadas al flujo activo; el intervalo remoto/predeterminado actual no cambia por batería. No afirmar que el nodo ya ahorra energía mediante deep sleep.

### 4. Evidencia y riesgo

Los tests host ejercitan fronteras, valores ausentes y entradas inválidas con floats sintéticos. No sustituyen build ESP-IDF ni medición física; ambos están prohibidos en este bloque. El usuario fijó fronteras exactas y no se inventó histéresis: si la medición real fluctúa alrededor de 50/25/10%, una futura integración que recalcule cada ciclo puede alternar entre intervalos. Antes de mitigarlo, medir ruido/estabilidad real y acordar margen/histéresis con evidencia.

Para autorizar deep sleep en hardware aún hacen falta química y número de celdas, curva bajo carga, referencia/divisor y calibración ADC, regulador/corriente quiescente, corrientes medidas de radio/arranque/sensores y temperatura. Tampoco se evaluaron autonomía ni recuperación de gestión/ACK al despertar.

## Fases pendientes

1. Integrar la máquina host-testable con driver de radio/protocolo v3, retención del frame y persistencia de identidad/secuencia; sensor-mantenimiento local-only requiere coordinación con caché/UI gateway.
2. Integrar reloj de rendezvous, expiración de su ventana y temporizadores finitos; deep sleep apaga recepción y no acepta comandos hasta despertar. Sin política calibrada, la máquina deniega sueño y el llamador debe sostener un ciclo de gestión acotado sin dejar la radio activa indefinidamente.
3. Integrar la lectura de porcentaje solo tras definir hardware/ADC/calibración y verificar mediciones físicas; entonces alimentar la regla de intervalo y mantener la puerta de sueño calibrada aparte.
4. Implementar driver ADS1115 y calibración por instalación antes de reportar porcentaje de humedad de suelo.

No se implementa OTA ni se cambian particiones en esta fase.
