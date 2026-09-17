# Sistema de monitoreo para invernadero - Versión 2

Este repositorio corresponde a la **segunda versión del sistema de monitoreo para invernadero**, desarrollado como continuación y actualización de un proyecto realizado previamente.

La nueva versión busca mejorar la arquitectura original, simplificar la comunicación entre dispositivos y adaptar el sistema a nuevos requerimientos del invernadero.

## Versión anterior

El desarrollo previo del proyecto puede consultarse en el siguiente repositorio:

[Repositorio de la versión anterior](https://github.com/sushemer/TECLA_Inveradero_SE)

Esta segunda versión reutiliza parte de la experiencia, estructura y componentes desarrollados anteriormente, pero incorpora cambios importantes en la arquitectura y en el alcance del sistema.

## Objetivo actual

Desarrollar una arquitectura basada principalmente en nodos ESP32 distribuidos dentro del invernadero, capaces de adquirir y procesar información de distintos sensores y transmitirla hacia un dispositivo central.

Actualmente se está evaluando ESP-NOW como mecanismo de comunicación entre los nodos y el gateway debido a sus características de comunicación directa y baja latencia.

## Arquitectura actual

La arquitectura propuesta actualmente es:

```text
Sensores
   │
   ▼
ESP32 Nodo
   │
   │ ESP-NOW
   ▼
ESP32 Gateway
   │
   ▼
Backend / Base de datos
   │
   ▼
Visualización
```
