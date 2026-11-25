# Promedios ambientales locales del gateway

## Alcance

El gateway calcula una vista derivada en RAM. No modifica ni elimina el frame recibido, no crea una trama de promedio para ESP-NOW y no envía promedios al servidor. La API central continúa recibiendo lecturas individuales; su `lecturas` conserva una fila por muestra aceptada y su deduplicación es independiente.

El nodo toma una medición por ciclo de lectura y no guarda ventanas en NVS. El cálculo del gateway no reduce el número de despertares, consultas al sensor ni transmisiones del nodo.

## Sensores incluidos

| Código de medición v3 | Fuente actual | Política |
|---|---|---|
| 1 Temperatura | DHT22 | Se agrega una muestra por ciclo cuando el modo del nodo es OPERATIVO, el nodo marca el sensor FUNCIONANDO y el valor es finito. |
| 2 Humedad relativa | DHT22 | Misma muestra/ciclo; no se hacen cinco consultas consecutivas. |
| 7 Iluminancia | VEML7700 | Se conserva la integración existente de 100 ms y espera de 120 ms; una lectura por ciclo. |
| 4 Humedad del suelo | SEN0193, cuatro canales declarados | Excluido por ahora: el lector activo devuelve ERROR/NaN y no integra ADS1115. No se promedian voltajes ni se convierten a porcentaje sin driver y calibración. |

El perfil activo documentado incluye DHT22 y VEML7700. SEN0193 está declarado en el perfil de cuatro canales, pero no es una fuente activa de mediciones. RSSI, estado del nodo, estado administrativo del sensor y salud no son códigos de promedio.

## Regla

- Serie: `firmware_node_id + slot_code + measurement_code + manifest_revision + assignment_revision`. Un cambio de asignación o manifiesto nunca mezcla dos sensores que reutilicen el mismo slot.
- Dedupe de frame: `firmware_node_id + boot_id + seq`; si la identidad se repite con bytes iguales, no aumenta el conteo; si cambia el contenido, se rechaza como conflicto.
- Ventana móvil: 10 800 s (3 h), usando el tiempo monotónico en que el gateway recibió cada frame. No se inventa una hora UTC.
- Umbral: mínimo 3 muestras válidas distintas. Con 1 o 2 se publica el conteo, `ready=false` y `mean=null`; no se presenta como promedio listo.
- Retención por serie: últimas 16 muestras dentro de la ventana. Límite RAM global: 1 024 valores, 256 series y 512 identidades recientes de frame. Si la tabla de valores se llena, expulsa el valor más antiguo; si todas las series están ocupadas, descarta la nueva serie y contabiliza `dropped_series`. El JSON local expone contadores de duplicados, conflictos y expulsiones.
- Expiración: al cumplir 3 h, la muestra sale de RAM. Si quedan menos de 3, la vista vuelve a incompleta; sin muestras, la serie desaparece.
- Reinicio del gateway: se pierden muestras y deduplicación en RAM. No hay recuperación de promedios desde NVS.
- Aceptación ambiental: además de decodificar correctamente la trama, requiere `effective_mode=OPERATIVO`, `sensor_health=FUNCIONANDO` y dato finito. Esto filtra estados reportados por firmware; no acredita exactitud, rango de fabricante ni calibración física.

## Superficie local

- `GET /api/averages`: declara `kind=gateway_ram_arithmetic_mean`, ventana, mínimo, `sample_count`, `ready`, `mean`, edades monotónicas y prefijos de hashes de asignación.
- `/`: muestra nodo, slot, código/nombre de medición, promedio solo cuando hay al menos tres muestras y el número de muestras. La página indica que es RAM temporal y que no se envía al servidor.
- El feature del promedio solo recibe frames de ESP-NOW v3 aceptados por la lista local de bindings. La opción v3 sigue detrás de Kconfig y desactivada por defecto.

## Impacto de API, base y UI central

No se cambia el protocolo v3: cada lectura del nodo conserva su valor y su identidad original. No se altera el esquema SQL ni se almacena una fila agregada. `POST /api/lecturas` mantiene su contrato de lecturas individuales y deduplicación. `GET /api/lecturas/promedio` es otra vista derivada de 60 minutos calculada por SQL sobre filas crudas; no equivale al promedio RAM de 3 horas del gateway y actualmente su grupo `global` mezcla nodos/sensores. La UI central y las gráficas continúan usando lecturas centrales; el promedio de esta especificación se ve solamente en la página local.

## Costo de medición

No se añaden conversiones al nodo ni ráfagas de muestras. DHT22 sigue consultándose una vez por ciclo y siempre debe respetar más de 2 s entre consultas si en el futuro se añade otro ciclo de lectura. VEML7700 conserva una conversión con la integración y espera actuales. El gateway necesita RAM y cálculo aritmético al recibir tramas; no añade transmisiones ni cambia la frecuencia de sueño.

## Verificación y pendientes

El módulo host debe probar media, 3 h, límite 16, duplicado/conflicto, modo/health inválido, capacidad hasta 10 nodos/240 series, reinicio y frame sin modificar. Aún no se valida en ESP-IDF ni en hardware. El receptor v3 local actual pone frames aceptados en una FIFO RAM de 8; no existe todavía envío v3/API central y esa FIFO puede llenarse. Por tanto el promedio local es una pieza experimental, no un flujo radio→API completo.
