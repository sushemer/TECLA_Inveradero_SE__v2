# Identidad, enrolamiento y perfiles de firmware

**Estado:** mapa del firmware activo y propuesta de incorporación. La selección de perfil y almacenamiento/lectura fail-closed de `firmware_node_id` están implementados. El parser host-testable ya se conecta a un REPL UART opcional bajo Kconfig, desactivado por defecto; no se validó con build ni hardware ESP-IDF. Enrollment autenticado, integración v3, caché gateway del manifiesto y asignación dinámica siguen pendientes. La fuente del contrato central es el checkout web `CimaInvernadero/docs/API_GATEWAY_V1.md`; no se modifica desde este repositorio.

## Identidades que no deben mezclarse

| Identidad | Autoridad/ciclo | Uso |
|---|---|---|
| MAC de fábrica STA | ESP32/eFuse; leer con `esp_read_mac(..., ESP_MAC_WIFI_STA)` | Transporte ESP-NOW y asociación vigente. No autentica por sí sola y no identifica sensores. |
| `firmware_node_id` | Provisionamiento duradero; `uint32` positivo | ID técnico estable que viaja en wire v3 y API. No se deriva de MAC ni perfil. Si falta/se borra NVS, queda sin provisionar hasta pairing explícito. |
| `dispositivo.id` / `nodo_id` | Base de datos central | ID SQL de la entidad nodo. Solo lo conoce gateway/API; no se compila en nodo ni se envía por radio. |
| `sensor_id` | Base de datos central, instancia/historial | Gateway traduce el mapping del manifiesto; nunca viaja por ESP-NOW ni se quema en el binario. |
| `sensor_firmware_id` + `canal` | Registry/capability de firmware y manifiesto | Identifica la instancia física dentro de una capacidad compilada. Son strings API exactos (trim y luego match byte a byte); el API no exige minúsculas ni valida contra el registry. |
| Perfil de sensores | Selección de build para placa/drivers/capabilities | Describe lo que puede medir el binario. No equivale a un nodo físico, MAC, ID SQL, inventario asignado ni estado administrativo. |
| `command_config_revision` (`config_revision` alias legacy) | Estado de configuración/commands API | API v1 ahora reserva el contador monotónico uint32 a COMMAND_CONFIG. Asignación cambia `assignment_revision`; estado/cadencia/metadatos, `manifest_revision`. MANIFEST_ACK no confirma comandos. |

El firmware v2 activo todavía usa `TECLA_ID_NODO` como ID del encabezado y como parte de la configuración heredada, pero la selección de capabilities ya se hace con `TECLA_SENSOR_PROFILE`. El ID v2 no es `firmware_node_id` v3 ni un permiso para copiar IDs SQL al binario.

## Mapa del firmware activo

| Archivo | Estado actual | Cambio objetivo |
|---|---|---|
| `firmware/nodo/src/app/app_main.c` | Inicializa storage, inicia REPL si `CONFIG_TECLA_NODE_IDENTITY_CONSOLE` está habilitada y exige identidad válida antes de muestrear; ausente/corrupto => log `UNPROVISIONED` y retorno seguro, dejando la tarea de consola disponible. Luego conserva el flujo v2 de una lectura y termina. | Confirmar board/UART en hardware y conectar flujo de arranque/radio v3. |
| `firmware/comun/operacion/ciclo_nodo.*` | Máquina de estados pura host-testable para rendezvous, modos de nodo, medición/envío, ACK de aplicación correlacionado, backoff acotado y gate externo de sueño. No está en el CMake activo ni conectada a `app_main`. | Enlazar solo al integrar transporte, temporizadores, persistencia y policy de batería; caché local de sensor en mantenimiento es otro flujo pendiente. |
| `firmware/nodo/src/configuracion/configuracion_sensores.h` | `TECLA_SENSOR_PROFILE_ID` selecciona capabilities compiladas; IDs locales 1..6 y canales SEN0193 siguen siendo constantes v2 legacy. | Separación de perfil implementada; aún falta migrar los slots del anuncio v2 al registry compacto v3. |
| `firmware/nodo/src/sensors/lectura_sensores.c` | Las ramas DHT22/VEML7700/SEN0193 declaradas se seleccionan por perfil; SEN0193 reporta error porque driver ADS1115 no está integrado. | Mantener un solo source tree; el perfil controla drivers/capabilities, no asignaciones SQL ni qué nodos están instalados. |
| `firmware/nodo/src/storage/configuracion_nvs.c/.h` | Guarda configuración remota. Init ya no borra NVS automáticamente al hallar partición sin páginas/versión incompatible; propaga error para recuperación de servicio explícita. | Evitar borrado destructivo; procedimiento físico de recuperación todavía pendiente. |
| `firmware/nodo/src/storage/node_identity.c/.h` | Módulo host-testable: registro de 13 bytes `TNID`, schema 1, firmware ID uint32 LE y CRC-32/IEEE; carga, set-once y reprovision con flag de confirmación. CRC detecta corrupción, no autentica identidad; el flag no prueba una acción física. | Conectar set-once/reprovision a CLI/ceremonia autorizada y definir recuperación contra registro central. Llamar operaciones desde una sola tarea, pues el helper genérico no serializa llamadas concurrentes. |
| `firmware/nodo/src/storage/node_identity_nvs.c/.h` | Adaptador NVS en namespace `tecla_identity`, clave `node_id_v1`; ID ausente/corrupto no se sustituye por MAC/perfil/SQL. No existe CLI de provisioning. | Validar comportamiento con build ESP-IDF y definir ceremonia antes del uso físico. |
| `firmware/nodo/src/storage/node_identity_cli.c/.h` | Parser/procesador puro; se enlaza solo con la opción Kconfig activa y harness host pasa. Soporta `status`, `provision <id>`, `reprovision <id>`, confirmación exacta y `cancel`; estado pendiente en RAM. | Validar target IDF y consola física autorizada; el feature se mantiene opt-in. |
| `firmware/nodo/src/storage/node_identity_console.c/.h` + `Kconfig.projbuild` | Adaptador REPL UART opcional, llamado por `app_main` antes de comprobar identidad; Kconfig está desactivado por defecto y depende de `ESP_CONSOLE_UART`. Usa UART/baud configurados por ESP-IDF. | Sin build ESP-IDF ni prueba física; confirmar que bridge/board expone UART configurado y reservar acceso durante commissioning. |
| `firmware/nodo/CMakeLists.txt` | `TECLA_NODO_ID` (legacy v2) acepta 2 o 4; `TECLA_SENSOR_PROFILE` selecciona `DHT22_ONLY`, `DHT22_VEML7700` o `SEN0193_4CH_DECLARED`. | Separación parcial implementada: perfil compilado y encabezado v2 son argumentos distintos; el valor por defecto conserva el build histórico. |
| `firmware/nodo/src/CMakeLists.txt` | Pasa `TECLA_ID_NODO` legacy y `TECLA_SENSOR_PROFILE_ID` al componente activo. | Mantener ambos conceptos separados hasta retirar v2; no llamar al legacy ID identidad durable v3. |
| `firmware/gateway/src/app/app_main.c` | Sigue usando allowlist/peers estáticos 2/4 y el POST legacy; base URL vacía deja AP local/ESP-NOW offline y evita conectar STA. | Cliente API v1 manifiesto autenticado, tabla binding dinámica, candidatos y unicast por asignación. |
| `firmware/gateway/Kconfig.projbuild` + `src/config/gateway_config.example.h` | `TECLA_GATEWAY_API_BASE_URL` configurable por Kconfig o header local; vacío por defecto. URL construida como base API + `/lecturas` para el endpoint legacy actual. | No incrustar URL/credenciales; implementar rutas v1/auth cuando API contract bridge esté integrado. |
| `firmware/gateway/src/queue/command_queue.*` | Cola de comandos y resultados NVS durable ya existe, pero no está conectada al cliente HTTP ni radio. | Antes de cada envío exigir status→lease→dispatch frescos y unicast; no retransmitir autorización tras reboot/expiry. |
| `firmware/protocol/v3/tecla_wire_v3.*` | Codec bytewise con builders/parsers tipados de ocho mensajes, incluido en ambos componentes CMake activos. `app_main`/radio aún no lo invocan. Golden vectors y pruebas host independientes cubren parsing y límites. | Conectar a callbacks/radio solo tras revisión de contrato, identidad provisionada, seguridad de peer y pruebas de extremo a extremo host. |

No se debe editar la variante `*-DESKTOP-BS27THB*` para cambiar el firmware activo. `TECLA_NODO_ID`/perfil v2 permanece solo en el sendero v2 legado hasta una migración explícita.

## Protocolo de provisioning serial

Al habilitar `CONFIG_TECLA_NODE_IDENTITY_CONSOLE` en menuconfig, usar el puerto/baud de la consola UART que configure ESP-IDF (la configuración actual del proyecto apunta a UART0/115200, pero debe confirmarse en la placa). El REPL recibe `identity <acción>`: `identity status`; `identity provision <uint32>`; `identity reprovision <uint32>`; `identity confirm-reprovision <mismo uint32>`; `identity cancel`. El módulo host puro procesa la acción sin el prefijo `identity`. Se aceptan espacios/tabuladores repetidos y CR/LF alrededor, solo dígitos decimales ASCII, sin signo ni argumentos extra; línea máxima 48 bytes. `0` se rechaza y `UINT32_MAX` (4294967295) es válido como ID técnico positivo.

`provision` solo escribe si la identidad está ausente; repetir el mismo ID responde `ALREADY_PROVISIONED` sin escribir y proponer otro ID responde `ID_CONFLICT` sin sobrescribir. `reprovision` exige identidad válida, solo arma una solicitud en RAM y no escribe; la confirmación separada debe repetir exactamente el ID. `cancel` borra el estado pendiente; reset también lo borra. El adaptador responde `OK firmware_node_id=<id>\r\n` para status/provision/confirm exitosos, `CONFIRM firmware_node_id=<id>; use identity confirm-reprovision <id> or identity cancel\r\n` al armar reprovisión, `OK CANCELLED\r\n` al cancelar y `ERR <RESULT_NAME>\r\n` ante otros resultados. El comando no constituye autenticación: debe habilitarse solo durante acceso físico autorizado y deshabilitarse en builds desplegados. No hay credenciales en consola. Reprovision no se dispara automáticamente ni por macro. El app debe reiniciarse manualmente después de provisionar para salir del estado UNPROVISIONED. Nunca se debe interpretar un macro de perfil, MAC o SQL como ID provisionado.

## Flujo propuesto de incorporación

1. **Build:** compilar una sola base de código con perfil de hardware explícito. El perfil anuncia capabilities compatibles; no elige `firmware_node_id`, `nodo_id`, MAC, sensor SQL ni ubicación.
2. **Provisionar identidad física:** leer MAC STA real del ESP32; el operador asigna/recupera un `firmware_node_id` positivo por un mecanismo local explícito (primera versión: USB/serial con acción del operador). El valor se persiste en NVS antes de activar v3. No usar ID de perfil o de SQL como sustituto.
3. **Nodo sin identidad:** NVS ausente, borrado o inválido deja al nodo en UNPAIRED; no genera un ID, no usa MAC como identity fallback, no acepta configuración radio y no publica telemetría central. El operador debe volver a provisionar explícitamente. La asociación posterior no puede suponerse por escuchar una trama.
4. **Candidato de gateway:** gateway puede registrar observaciones de `(firmware_node_id, MAC, boot, versión wire/fw, capabilities)` con `POST /api/gateway/v1/discovery/candidates`; API responde sin asignar y `GET .../gateways/{id}/candidates` permite su revisión administrativa. Ese cliente no está integrado en firmware. MAC/capability vista no autoriza telemetría ni órdenes. La UI debe permitir selección explícita de gateway + nodo SQL + firmware ID; RSSI no participa en ownership ni se envía centralmente.
5. **Asignación autorizada:** el operador confirma en la UI. API ya tiene rutas `PUT .../nodes` para `nodo_id + firmware_node_id + mac_address` y `PUT .../sensors` para `sensor_id + sensor_firmware_id + canal`. El backend conserva historial y produce manifiesto scoped al gateway. Conflictos MAC/ID vigentes deben impedir asignación duplicada.
6. **Gateway:** cliente autenticado descarga manifiesto, verifica schema/hash/scope/estado, guarda snapshot íntegro como last-known-good en NVS y asocia cada `(MAC, firmware_node_id)` a capabilities. Una MAC desconocida permanece candidata; jamás se adopta por RSSI. Config/comando se envía unicast únicamente al mapping vigente, nunca broadcast.
7. **Sin Internet:** gateway puede seguir usando su último manifiesto íntegro para mostrar telemetría/mapping local. Sin API no obtiene status/lease/dispatch nuevos y por tanto no despacha comandos nuevos. No debe dejar configuraciones de manifiesto divergentes en FIFO. Al reconectar, primero GET manifiesto/presencia y compara hashes; si mapping cambió, datos viejos quedan con revisiones originales y no se remapean al snapshot nuevo.
8. **Revocación/reemplazo:** nueva asignación cierra historial previo. Cambio MAC o reemplazo de sensor produce nueva asociación SQL; gateway traduce con `assignment_revision`/`manifest_revision`. Firmware usa su profile/capabilities constantes; SQL IDs y estados de inventario vienen en manifiesto. Si driver/capability física no existe en ese binario, el alta no puede inventarla: requiere nuevo build/flasheo de perfil compatible.

MAC identifica transporte y sirve para match de peer; no es autenticación. El protocolo v3 aún no tiene interoperabilidad de radio ni protección CCMP implementada. API auth/HTTPS protege gateway-servidor, pero no resuelve suplantación/replay por radio; no habilitar control remoto en campo hasta definir el trust/commissioning radio.

## Perfiles físicos: una base, varios builds deterministas

Perfiles observados en el código activo (no significan que un nodo concreto tenga esos sensores instalados):

| Perfil de build | Capabilities que declara el código | Estado real |
|---|---|---|
| `DHT22_ONLY` | DHT22 por GPIO4 | Driver activo; se puede reutilizar para cualquier placa compatible que use ese hardware. |
| `DHT22_VEML7700` | DHT22 + VEML7700 en I2C | Drivers activos en el código para el perfil existente; no es asignación de inventario. |
| `SEN0193_4CH_DECLARED` | Cuatro slots SEN0193/ADS1115 A0..A3 | Solo declarado; ADS1115/calibración no está en la ruta activa, por tanto no anunciar lecturas válidas ni subir perfil como instalado. |

Nodos que tengan combinación idéntica reutilizan el mismo binario. Añadir/quitar hardware o driver cambia el perfil/build del nodo; cambiar inventario SQL, estado, sensor_id o assignment no recompila si el perfil ya soporta la capability. El ID SQL se asigna en API por cada nueva instancia histórica. El gateway usa el registry compacto v3 y el manifiesto para mapear slot/canal; no se compilan ID SQL ni MAC por nodo.

Ejemplo objetivo de build (no ejecutado; no se validó con ESP-IDF):

```powershell
idf.py -B C:\Users\SUSHEMER\Documents\CodexBuilds\Invernadero\nodo-dht22-v3 -DTECLA_SENSOR_PROFILE=DHT22_ONLY -DTECLA_NODO_ID=2 build
idf.py -B C:\Users\SUSHEMER\Documents\CodexBuilds\Invernadero\nodo-dht22-veml-v3 -DTECLA_SENSOR_PROFILE=DHT22_VEML7700 -DTECLA_NODO_ID=2 build
```

Cada build debe usar directorio externo nuevo y un perfil conocido; cambiar `TECLA_NODO_ID` solo afecta el encabezado v2 heredado, no la identidad v3. La selección independiente de perfil compila/declara capabilities, pero el protocolo v2 aún etiqueta nodos con el ID legacy y el enrolamiento v3 no está integrado. El build SEN0193 conserva la simulación legacy de error y no debe anunciar lecturas como operativas hasta integrar/validar ADS1115 y calibración. El valor CMake `TECLA_SENSOR_PROFILE` y el perfil compilado también deben persistirse/mostrarse como `profile_id`, no como `firmware_node_id`.

## Cambios necesarios fuera del checkout embebido

- **Chat6/API:** discovery no vinculante y separación entre `command_config_revision` y hashes de manifiesto/asignación están descritos en el contrato actualizado. No existe integración en este firmware.
- **Interfaz web:** bandeja de candidatos `NO_ASIGNADO`, vista de capabilities declaradas vs instaladas, selección explícita de gateway y nodo SQL, y asignación/reemplazo de sensor por `sensor_firmware_id+canal`; mostrar manifest sync CURRENT/OUT_OF_DATE/UNKNOWN y estado no provisionado. Nunca autoasignar por MAC/RSSI.
- **Seguridad:** definir ceremonia de USB/QR/pairing token y protección radio/replay (CCMP/clave por peer) antes de aceptar un candidato o recibir comandos. MAC no autentica.

## Gates de implementación

1. Cambiar perfil/capabilities con tests host de matriz; no asignar nodos físicos ni SQL.
2. Identity NVS versionada/set-once/reprovision gated y fail-closed implementada; REPL UART opcional conectado a `app_main` bajo Kconfig, pero resta build target IDF y prueba física de UART.
3. Base URL Kconfig vacía/offline implementada; solo arma endpoint legacy `/lecturas`. API v1/auth no está conectada.
4. Persistencia gateway manifest LKG y binding dinámico aún no implementados. API v1 ya define candidates/snapshot; antes de radio faltan security/replay/CCMP, integración codec tipado y validación target IDF.
5. Build IDF y evidencia física son gates separados; no se flashea hardware en esta tarea.
