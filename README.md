# Sistema de monitoreo para invernadero - Versión 2

Este repositorio corresponde a la **segunda versión del sistema de monitoreo para invernadero**, desarrollada como continuación y actualización del proyecto original.

La nueva versión busca mejorar la arquitectura del sistema, reducir el cableado, simplificar la comunicación entre dispositivos y facilitar la incorporación de nuevos sensores, nodos y funciones de control.

## Versión anterior

El desarrollo previo del proyecto puede consultarse en el siguiente repositorio:

[Repositorio de la versión anterior](https://github.com/sushemer/TECLA_Inveradero_SE)

Esta segunda versión reutiliza parte de la experiencia, estructura y componentes desarrollados anteriormente, pero incorpora cambios importantes en la arquitectura y en el alcance del sistema.

## Repositorio de la plataforma web

La aplicación web se desarrolla a partir del repositorio existente de **CimaInvernadero**, en el cual continuarán realizándose las modificaciones relacionadas con la interfaz y la visualización de información:

[Repositorio web CimaInvernadero](https://github.com/ClubTECLA/CimaInvernadero)

Este repositorio contiene principalmente el desarrollo de los nodos ESP32, el gateway y la integración de la comunicación con el backend. La plataforma web, su presentación visual y las consultas de información se mantienen en el repositorio de CimaInvernadero.

## Objetivo actual

Desarrollar una arquitectura distribuida basada principalmente en nodos ESP32 instalados en diferentes zonas del invernadero. Cada nodo deberá adquirir información de los sensores conectados, identificar el origen de las mediciones y transmitirlas hacia un dispositivo central.

Además del monitoreo ambiental, la arquitectura deberá permitir la integración de actuadores cuando sean necesarios. Estos podrán ejecutar acciones locales, como activar o desactivar un ventilador, una bomba, una válvula u otro dispositivo de control, de acuerdo con las condiciones definidas para el sistema.

Actualmente se contempla ESP-NOW como mecanismo de comunicación entre los nodos y el gateway debido a sus características de comunicación directa, bajo consumo y baja dependencia de la red Wi-Fi para cada punto de medición. La selección definitiva deberá validarse mediante pruebas de alcance, estabilidad, consumo y funcionamiento en las condiciones reales del invernadero.


## Componentes principales

- **Nodos ESP32:** realizan las lecturas de los sensores instalados en cada zona y transmiten la información al gateway.
- **Sensores ambientales:** permiten medir variables como temperatura, humedad y otras condiciones del invernadero.
- **Sensores adicionales:** podrán incorporarse sensores como PAR o CO₂, dependiendo de los requerimientos del invernadero, la compatibilidad técnica y la disponibilidad de recursos.
- **Actuadores:** podrán ejecutar acciones de control, como activar ventilación, bombas o válvulas, cuando formen parte del alcance aprobado.
- **ESP32 Gateway:** concentra los datos recibidos mediante ESP-NOW y los comunica con el backend.
- **Backend:** recibe, valida y procesa la información enviada por el gateway.
- **Base de datos:** almacena las mediciones, fechas, variables, nodos, sensores y zonas de origen para permitir consultas posteriores.
- **Aplicación web:** permite visualizar las mediciones actuales, consultar el historial y organizar la información por nodo, sensor, variable o zona.

## Funcionamiento ante fallos
Los nodos deberán mantener sus funciones locales de medición y control aunque exista una interrupción temporal de comunicación con el gateway, el backend o la aplicación web.

Cuando la comunicación se restablezca, el sistema deberá intentar recuperar automáticamente el envío de nuevas mediciones sin requerir el reinicio manual de los dispositivos. También se contempla el uso de almacenamiento temporal y retransmisión posterior cuando sea necesario y técnicamente viable.

## Seguridad y confiabilidad
- Los datos recibidos deberán validarse antes de almacenarse como mediciones correctas.
- Cada medición deberá incluir información que permita identificar su nodo, sensor, variable o zona de origen.
- La instalación de sensores, nodos, actuadores y cableado deberá realizarse sin generar riesgos para las personas, los cultivos o la infraestructura del invernadero.
- Las funciones locales de monitoreo y control no deberán depender completamente de la disponibilidad de la página web.

## Estado actual

La arquitectura se encuentra en etapa de validación. Antes de realizar la instalación definitiva se deberán comprobar, entre otros aspectos:

- Cobertura y estabilidad de ESP-NOW dentro del invernadero.
- Comunicación entre los nodos y el gateway.
- Comunicación entre el gateway y el backend.
- Alimentación eléctrica de los nodos, sensores y actuadores.
- Compatibilidad y selección del sensor PAR.
- Necesidad de incorporar un sensor de CO₂.
- Integración y funcionamiento de los actuadores, cuando correspondan.
- Almacenamiento temporal y recuperación ante interrupciones de comunicación.
- Adaptación de la base de datos para nuevos nodos, sensores, variables y zonas.
- Integración de la plataforma web con la nueva estructura de información.

La incorporación de nuevos sensores o actuadores dependerá de su compatibilidad con los nodos ESP32, de los requerimientos del invernadero, del presupuesto disponible y de las pruebas realizadas durante el desarrollo.
