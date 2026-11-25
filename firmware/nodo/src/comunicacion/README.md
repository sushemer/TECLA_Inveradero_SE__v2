# Comunicación del nodo

La comunicación se divide en dos responsabilidades:

- `salida_propia/`: publicación de las lecturas del nodo actual.
- `gateway_propio/`: recepción futura de lecturas de otros nodos.

En la primera versión solo se implementa la salida propia. La recepción de
otros nodos no se activa por tener una placa con capacidad de gateway.
