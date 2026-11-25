#include <assert.h>
#include <stdint.h>

#include "../src/configuracion/sensor_profile.h"

int main(void)
{
#if TECLA_SENSOR_PROFILE_ID == TECLA_SENSOR_PROFILE_DHT22_ONLY
    assert(TECLA_PROFILE_HAS_DHT22 == 1);
    assert(TECLA_PROFILE_HAS_VEML7700 == 0);
    assert(TECLA_PROFILE_HAS_SEN0193 == 0);
    assert(TECLA_PROFILE_SENSOR_MASK == (1U << 0U));
#elif TECLA_SENSOR_PROFILE_ID == TECLA_SENSOR_PROFILE_DHT22_VEML7700
    assert(TECLA_PROFILE_HAS_DHT22 == 1);
    assert(TECLA_PROFILE_HAS_VEML7700 == 1);
    assert(TECLA_PROFILE_HAS_SEN0193 == 0);
    assert(TECLA_PROFILE_SENSOR_MASK == ((1U << 0U) | (1U << 5U)));
#elif TECLA_SENSOR_PROFILE_ID == TECLA_SENSOR_PROFILE_SEN0193_4CH_DECLARED
    assert(TECLA_PROFILE_HAS_DHT22 == 0);
    assert(TECLA_PROFILE_HAS_VEML7700 == 0);
    assert(TECLA_PROFILE_HAS_SEN0193 == 1);
    assert(TECLA_PROFILE_SENSOR_MASK == ((1U << 1U) | (1U << 2U) | (1U << 3U) | (1U << 4U)));
#else
#error "unexpected profile"
#endif
    return 0;
}
