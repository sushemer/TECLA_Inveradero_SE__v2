# Wi-Fi STA + AP

El gateway debe operar simultáneamente como:

- **STA:** intenta conectarse a la red configurada para comunicarse con backend.
- **AP:** mantiene una red local de diagnóstico disponible aunque STA falle o no haya Internet.

La disponibilidad de STA no debe detener ESP-NOW, la FIFO ni el servidor local. El servidor debe exponer únicamente información actual y de diagnóstico en esta etapa.
