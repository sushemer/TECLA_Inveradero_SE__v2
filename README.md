# Sistema de monitoreo para invernadero - Versión 2
Este repositorio corresponde a la **segunda versión del sistema de monitoreo para invernadero**, desarrollada como continuación y actualización del proyecto original.

La nueva versión busca mejorar la arquitectura del sistema, reducir el cableado, simplificar la comunicación entre dispositivos y facilitar la incorporación de nuevos sensores, nodos y funciones de control.

## Versión anterior
El desarrollo previo del proyecto puede consultarse en el siguiente repositorio: [Repositorio de la versión anterior](https://github.com/sushemer/TECLA_Inveradero_SE)

Esta segunda versión reutiliza parte de la experiencia, estructura y componentes desarrollados anteriormente, pero incorpora cambios importantes en la arquitectura y en el alcance del sistema.

## Repositorio de la plataforma web
La aplicación web se desarrolla a partir del repositorio existente de **CimaInvernadero**, en el cual continuarán realizándose las modificaciones relacionadas con la interfaz y la visualización de información: [Repositorio web CimaInvernadero](https://github.com/ClubTECLA/CimaInvernadero)

Este repositorio contiene principalmente el desarrollo de los nodos ESP32, el gateway y la integración de la comunicación con el backend. La plataforma web, su presentación visual y las consultas de información se mantienen en el repositorio de CimaInvernadero.

## Objetivo actual
Desarrollar una arquitectura distribuida basada principalmente en nodos ESP32 instalados en diferentes zonas del invernadero. Cada nodo deberá adquirir información de los sensores conectados, identificar el origen de las mediciones y transmitirlas hacia un dispositivo central.

Además del monitoreo ambiental, la arquitectura deberá permitir la integración de actuadores cuando sean necesarios. Estos podrán ejecutar acciones locales, como activar o desactivar un ventilador, una bomba, una válvula u otro dispositivo de control, de acuerdo con las condiciones definidas para el sistema.

Actualmente se contempla ESP-NOW como mecanismo de comunicación entre los nodos y el gateway debido a sus características de comunicación directa, bajo consumo y baja dependencia de la red Wi-Fi para cada punto de medición. La selección definitiva deberá validarse mediante pruebas de alcance, estabilidad, consumo y funcionamiento en las condiciones reales del invernadero.


## Componentes principales
- ESP32
-- **Nodos:** realizan las lecturas de los sensores instalados en cada zona y transmiten la información al gateway.
- **Gateway:**
-- Combinado
- **Sensores ambientales:** permiten medir variables como temperatura, humedad, PAR y otras condiciones del invernadero.
- **Actuadores:** podrán ejecutar acciones de control, como activar ventilación, bombas o válvulas, cuando formen parte del alcance aprobado.
- **Backend:** recibe, valida y procesa la información enviada por el gateway.
- **Base de datos:** almacena las mediciones, fechas, variables, nodos, sensores y zonas de origen para permitir consultas posteriores.
- **Aplicación web:** permite visualizar las mediciones actuales, consultar el historial y organizar la información por nodo, sensor, variable o zona.

## Funcionamiento ante fallos
Cuando la comunicación se restablezca, el sistema deberá intentar recuperar automáticamente el envío de nuevas mediciones sin requerir el reinicio manual de los dispositivos. 

## Seguridad y confiabilidad
- Los datos recibidos deberán validarse antes de almacenarse como mediciones correctas.
- Cada medición deberá incluir información que permita identificar su nodo, sensor y variable.
- La instalación de sensores, nodos, actuadores y cableado deberá realizarse sin generar riesgos para las personas, los cultivos o la infraestructura del invernadero.
- Las funciones locales de monitoreo y control no deberán depender completamente de la disponibilidad de la página web.

## Firmware
La base modular del gateway y los nodos se encuentra en [`firmware/`](firmware/):


## Referencias técnicas para el firmware

Esta sección reúne documentación y repositorios que se utilizarán como referencia durante la implementación. Las referencias no significan que se vaya a copiar código directamente ni que el componente ya esté seleccionado de forma definitiva.

### Comunicación entre nodo y gateway
La comunicación principal propuesta es **ESP-NOW**:
- [ESP-NOW en la guía oficial de ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_now.html): API, peers, callbacks, envío, recepción y seguridad.
- [Ejemplo oficial de ESP-NOW de ESP-IDF](https://github.com/espressif/esp-idf/tree/master/examples/wifi/espnow): referencia para inicialización de Wi-Fi, registro de callbacks, peers, secuencia, CRC y colas.
- [Componente ESP-NOW de Espressif](https://github.com/espressif/esp-now): ejemplos adicionales para comunicación uno-a-muchos, muchos-a-muchos, pairing, seguridad, control y OTA.

Se usará ESP-NOW porque permite que los nodos se comuniquen directamente con el gateway sin depender de que cada nodo tenga acceso a Internet. El gateway será el dispositivo encargado de combinar ESP-NOW con Wi-Fi y HTTPS hacia CimaInvernadero.

El protocolo propio del proyecto se mantendrá en [`firmware/comun/protocolo/`](firmware/comun/protocolo/). Sobre ESP-NOW se agregarán validación de versión, tamaño, nodo de origen, secuencia, CRC, ACK, reintentos y política de duplicados.

### DHT22: temperatura y humedad relativa
El DHT22 será la referencia inicial para temperatura y humedad relativa, debido a que ya se utilizó en la versión anterior del firmware y el backend ya contempla esos tipos de dato.

- [Driver DHT22 para ESP-IDF](https://github.com/fernandorpardo/DHT22_ESP-IDF): referencia externa para la temporización y lectura del dispositivo.
- [Repositorio anterior del firmware](https://github.com/sushemer/TECLA_Inveradero_SE): contiene la lectura, validación, checksum, límites físicos y cálculo de VPD usados como punto de partida.

El driver definitivo deberá aislarse de la aplicación. Una lectura inválida no deberá detener el nodo ni invalidar automáticamente las lecturas de otros sensores.

### Humedad del suelo
La humedad del suelo se tratará inicialmente como una medición analógica, salvo que el modelo final del sensor requiera otro protocolo. La lectura deberá incluir calibración del sensor seco/húmedo, límites, filtrado y detección de desconexión.

- [Ejemplo oficial de ADC de ESP-IDF](https://github.com/espressif/esp-idf/tree/master/examples/peripherals/adc/oneshot_read): referencia para ADC one-shot y calibración.
- [Ejemplo oficial de ADC continuo de ESP-IDF](https://github.com/espressif/esp-idf/tree/master/examples/peripherals/adc/continuous_read): referencia si posteriormente se requieren muestras frecuentes o procesamiento por bloques.

El modelo concreto del sensor de humedad del suelo, su circuito de acondicionamiento, GPIO, rango eléctrico y curva de calibración quedan pendientes de confirmar con el hardware disponible. No se debe asumir que el porcentaje mostrado por un sensor resistivo barato representa directamente humedad volumétrica.

### Luz ambiental y PAR
La medición de luz debe distinguir entre **iluminancia** en lux y **PAR/PPFD** para radiación útil fotosintéticamente. No son magnitudes intercambiables.

- [Componente BH1750 en ESP Component Registry](https://components.espressif.com/components/espressif/bh1750/versions/1.0.3): referencia para un sensor digital de luz ambiental por I2C.
- [Ejemplo de implementación BH1750 para ESP32/ESP-IDF](https://github.com/kimsniper/BH1750): referencia de conexión I2C y lectura en lux.
- [Ejemplos oficiales de I2C de ESP-IDF](https://github.com/espressif/esp-idf/tree/master/examples/peripherals/i2c): referencia para inicialización del bus y manejo de dispositivos I2C.

El BH1750 puede ser útil para luz ambiental, pero no debe documentarse como sensor PAR sin validar su respuesta espectral y el método de conversión. El sensor PAR definitivo y su unidad de almacenamiento siguen pendientes.

### Wi-Fi, backend y componentes ESP-IDF
- [Repositorio oficial de ESP-IDF](https://github.com/espressif/esp-idf): framework, ejemplos y versiones compatibles con el chip seleccionado.
- [Guía oficial del sistema de compilación de ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html): componentes, `CMakeLists.txt`, `idf_component.yml` y `managed_components`.
- [ESP-IDF Component Manager](https://github.com/espressif/idf-component-manager): mecanismo previsto para fijar y descargar dependencias sin copiar bibliotecas manualmente al repositorio.

El gateway mantendrá separado el transporte ESP-NOW del cliente HTTPS. La recepción de un paquete no deberá ejecutar directamente una solicitud al backend; el paquete se validará, se colocará en una cola y otra tarea decidirá si se envía, reintenta o conserva.

### Ahorro de energía y sleep
El modo de energía dependerá del tipo de dispositivo:

- El **gateway** normalmente permanecerá activo, porque debe recibir nodos, mantener diagnóstico local y atender comandos.
- Un **nodo alimentado por corriente** podrá usar modem-sleep o auto light-sleep cuando no esté realizando trabajo.
- Un **nodo alimentado por batería** podrá evaluar deep-sleep si el intervalo de medición permite desconectar Wi-Fi y ESP-NOW entre muestras.

Referencias:
- [Modos de bajo consumo de ESP-IDF en escenarios Wi-Fi](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/low-power-mode/low-power-mode-wifi.html): comparación entre modem-sleep, auto light-sleep y deep-sleep.
- [Modos de bajo consumo del sistema ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/low-power-mode/low-power-mode-soc.html): funcionamiento de light-sleep, auto light-sleep, deep-sleep y wake-up.
- [Guía de sleep de ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/sleep_modes.html): fuentes de wake-up y APIs de sleep.

Deep-sleep no debe activarse antes de definir qué datos deben conservarse en RTC/NVS, cómo se mantiene la secuencia de mensajes y cómo se recupera la configuración después del reinicio. Además, al usar deep-sleep se pierde la conexión inalámbrica y el nodo debe ejecutar nuevamente su secuencia de arranque al despertar.

### Estado de las referencias
| Tema | Referencia inicial | Estado del proyecto |
|---|---|---|
| Nodo-gateway | ESP-NOW oficial de Espressif | Propuesto; falta validar alcance y estabilidad en el invernadero |
| Temperatura/humedad | DHT22 | Base inicial; falta implementar driver modular |
| Humedad del suelo | ADC de ESP-IDF | Modelo y calibración pendientes |
| Luz ambiental | BH1750/I2C | Referencia; no equivale automáticamente a PAR |
| Ahorro de energía | modem-sleep, auto light-sleep o deep-sleep | Depende de alimentación e intervalo |