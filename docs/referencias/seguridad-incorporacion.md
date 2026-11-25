# Incorporación, MAC y seguridad

## Objetivo

Permitir registrar un nodo desde la página local sin reprogramar el gateway
para cada dispositivo, pero sin aceptar paquetes de cualquier ESP32 cercano.

## Referencias técnicas

- [Seguridad de ESP-NOW en ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_now.html#security): PMK, LMK y cifrado CCMP.
- [Ejemplo oficial de seguridad de espressif/esp-now](https://github.com/espressif/esp-now/tree/master/examples/security): pairing seguro con prueba de posesión.
- [Ejemplo oficial de pairing](https://github.com/espressif/esp-now/tree/master/examples/coin_cell_demo): referencia para incorporación y dispositivos de bajo consumo.

## Flujo propuesto

1. El nodo muestra por serial su MAC y un código temporal de incorporación.
2. El usuario abre la página local del gateway.
3. El gateway muestra solicitudes pendientes, pero no las acepta automáticamente.
4. El usuario registra MAC, `id_nodo`, ubicación, sensores e intervalo.
5. El gateway acepta solo si coinciden MAC y código temporal.
6. El gateway guarda el registro en NVS.
7. El gateway envía la configuración al nodo.
8. El nodo confirma la aplicación.
9. El gateway agrega el nodo a la lista autorizada y usa comunicación unicast.

## Qué no debe considerarse seguridad suficiente

- Solo la MAC: puede falsificarse.
- Solo un código fijo como `7AC0`: puede repetirse o capturarse.
- Broadcast permanente: facilita que cualquier dispositivo se anuncie.
- Aceptar el primer paquete recibido: no permite verificar la identidad.

Para el prototipo puede existir un código temporal visible por serial. Para una
versión más robusta se usará un secreto de incorporación de un solo uso y
cifrado ESP-NOW con PMK/LMK.

## Separación de autorizaciones

La página local administra el registro. El gateway aplica la autorización. El
nodo solo acepta comandos provenientes de su gateway configurado y confirma la
configuración aplicada. Internet no participa en la autorización local.
