#include "sensor_validation.h"

#include <math.h>

bool tecla_medicion_es_plausible(uint8_t tipo_valor, float valor)
{
    if (!isfinite(valor)) return false;
    switch (tipo_valor) {
    case TECLA_TIPO_VALOR_TEMPERATURA:
        return valor >= -40.0f && valor <= 80.0f;
    case TECLA_TIPO_VALOR_HUMEDAD:
        return valor >= 0.0f && valor <= 100.0f;
    case TECLA_TIPO_VALOR_LUZ:
        /* VEML7700 range/saturation depends on gain and integration settings. */
        return valor >= 0.0f;
    default:
        /* Soil/other types need a calibrated sensor-specific contract. */
        return false;
    }
}
