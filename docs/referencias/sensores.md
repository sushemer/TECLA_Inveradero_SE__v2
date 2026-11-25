# Referencias de sensores y conversión de lecturas

## 1. DHT22

### Qué se usará

Una línea digital para obtener temperatura en °C y humedad relativa en %. El
driver debe comprobar la respuesta y el checksum. Si falla, debe reportar el
estado del sensor y no convertir el error en cero.

### Base técnica

- [Componente DHT de ESP-IDF-lib](https://components.espressif.com/components/esp-idf-lib/dht/versions/1.2.0/readme): referencia principal para ESP-IDF; incluye soporte para DHT22/AM2301.
- [Repositorio esp-idf-lib/dht](https://github.com/esp-idf-lib/dht): código fuente y ejemplos del componente.
- [Definición de la interfaz del driver](https://github.com/esp-idf-lib/esp-idf-lib/blob/master/components/dht/dht.h): muestra la lectura en enteros o flotantes y la necesidad del pull-up.

### Apoyo visual

- [Conexión DHT22 con ESP32](https://www.youtube.com/watch?v=YJXICV1_Kw0): revisar solo el cableado y la identificación de pines.

### Decisiones para nuestro nodo

- Alimentación preferida: 3.3 V.
- Un DHT22 por entrada digital.
- No leerlo continuamente; se respetará el intervalo mínimo del fabricante.
- Temperatura y humedad serán dos lecturas asociadas al mismo sensor.

## 2. SEN0193 para humedad de sustrato

### Qué se usará

El SEN0193 entrega una señal analógica. No mide directamente un porcentaje
universal. Cada unidad debe calibrarse en el sustrato real, porque influyen la
profundidad, compactación, salinidad y tipo de suelo.

### Base técnica

- [Ficha oficial DFRobot SEN0193](https://wiki.dfrobot.com/sen0193/): alimentación, salida analógica y conexión.
- [Calibración oficial del SEN0193](https://wiki.dfrobot.com/sen0193/docs/18037): lectura en aire y medio húmedo como límites iniciales.
- [Hoja técnica en PDF](https://media.digikey.com/pdf/data%20sheets/dfrobot%20pdfs/sen0193_web.pdf): referencia eléctrica y esquema.
- [Estudio sobre calibración de SEN0193 en distintos medios](https://www.mdpi.com/2073-4441/18/12/1431): evidencia de que la calibración aire/agua no representa necesariamente el sustrato real.

### Cómo se dividirá el trabajo

1. Leer el valor crudo con ADC o ADS1115.
2. Guardar el valor y el estado como `SIN_CALIBRAR`.
3. Medir el sensor en el sustrato real seco y húmedo.
4. Obtener una curva o límites por sensor y por tipo de sustrato.
5. Convertir a porcentaje solamente cuando la calibración sea válida.

### Apoyo visual

- [Guía oficial de calibración DFRobot](https://wiki.dfrobot.com/sen0193/docs/18037): es más confiable que un video porque muestra el procedimiento del fabricante.

## 3. Sensor de luz: BH1750 frente a VEML7700

El firmware actual prepara BH1750, pero la documentación de compras y
decisiones anteriores mencionan VEML7700. Son sensores distintos y no debemos
mezclar sus drivers, direcciones ni fórmulas.

### BH1750

- [Componente BH1750 de ESP-IDF-lib](https://components.espressif.com/components/espressif/bh1750/versions/1.1.7/readme): referencia compatible con el árbol actual del nodo.
- [Ejemplo del componente BH1750](https://components.espressif.com/components/espressif/bh1750/versions/1.1.7/examples/default): inicialización I2C y lectura en lux.
- [Ejemplos oficiales I2C de ESP-IDF](https://github.com/espressif/esp-idf/tree/master/examples/peripherals/i2c): bus, tiempos y manejo de errores.

### VEML7700

- [Documentación del driver VEML7700 para ESP-IDF-lib](https://esp-idf-lib.github.io/veml7700/): ganancia, tiempo de integración, ahorro de energía y lectura de lux.
- [Guía SparkFun VEML7700 con ESP32](https://docs.sparkfun.com/SparkFun_Ambient_Light_Sensor-VEML7700/quickstart/): conexión y prueba práctica.
- [Repositorio Adafruit VEML7700](https://github.com/adafruit/Adafruit_VEML7700): referencia alternativa de configuración y lectura.
- [Hoja de aplicación de Vishay](https://www.vishay.com/docs/84323/designingveml7700.pdf): fuente para entender ganancia, integración y compensación.

### Decisión pendiente

Antes de fijar el driver definitivo se debe confirmar si el módulo comprado es
BH1750 o VEML7700. Ninguno debe llamarse PAR ni sustituye un sensor PAR.

## 4. ADS1115

### Qué se usará

Como expansión analógica para medir por separado varios SEN0193. El ADS1115
tiene cuatro canales multiplexados; las mediciones se realizan una por una,
no de forma simultánea.

- [Hoja de datos oficial TI ADS1115](https://www.ti.com/lit/ds/symlink/ads1115.pdf): alimentación, PGA, canales, resolución y límites eléctricos.
- [Página oficial del ADS1115](https://www.ti.com/product/ADS1115): características y documentación complementaria.
- [Componente ADS111x de ESP-IDF-lib](https://components.espressif.com/components/esp-idf-lib/ads111x): referencia de integración I2C para ESP-IDF.

### Asignación de referencia

```text
A0 → humedad de sustrato 1
A1 → humedad de sustrato 2
A2 → humedad de sustrato 3
A3 → humedad de sustrato 4
```

La batería puede medirse con otro canal ADC del DevKit o con otro ADS1115. No
se debe colocar el divisor de batería en un canal que ya esté ocupado por un
sensor si se necesitan cuatro lecturas independientes.
