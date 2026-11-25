#include "../src/power/battery_policy.h"
#include "../src/sensors/sensor_validation.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

int main(void)
{
    uint32_t selected_interval = UINT32_MAX;
    assert(tecla_battery_interval_for_percent(true, 100.0f, &selected_interval) ==
           TECLA_BATTERY_INTERVAL_SELECTED && selected_interval == 600U);
    assert(tecla_battery_interval_for_percent(true, 50.01f, &selected_interval) ==
           TECLA_BATTERY_INTERVAL_SELECTED && selected_interval == 600U);
    assert(tecla_battery_interval_for_percent(true, 50.0f, &selected_interval) ==
           TECLA_BATTERY_INTERVAL_SELECTED && selected_interval == 1200U);
    assert(tecla_battery_interval_for_percent(true, 25.01f, &selected_interval) ==
           TECLA_BATTERY_INTERVAL_SELECTED && selected_interval == 1200U);
    assert(tecla_battery_interval_for_percent(true, 25.0f, &selected_interval) ==
           TECLA_BATTERY_INTERVAL_SELECTED && selected_interval == 2400U);
    assert(tecla_battery_interval_for_percent(true, 10.01f, &selected_interval) ==
           TECLA_BATTERY_INTERVAL_SELECTED && selected_interval == 2400U);
    assert(tecla_battery_interval_for_percent(true, 10.0f, &selected_interval) ==
           TECLA_BATTERY_INTERVAL_SELECTED && selected_interval == 3600U);
    assert(tecla_battery_interval_for_percent(true, 0.0f, &selected_interval) ==
           TECLA_BATTERY_INTERVAL_SELECTED && selected_interval == 3600U);
    assert(tecla_battery_interval_for_percent(false, 75.0f, &selected_interval) ==
           TECLA_BATTERY_INTERVAL_NO_MEASUREMENT && selected_interval == 0U);
    assert(tecla_battery_interval_for_percent(true, NAN, &selected_interval) ==
           TECLA_BATTERY_INTERVAL_INVALID_PERCENT && selected_interval == 0U);
    assert(tecla_battery_interval_for_percent(true, INFINITY, &selected_interval) ==
           TECLA_BATTERY_INTERVAL_INVALID_PERCENT);
    assert(tecla_battery_interval_for_percent(true, -INFINITY, &selected_interval) ==
           TECLA_BATTERY_INTERVAL_INVALID_PERCENT);
    assert(tecla_battery_interval_for_percent(true, -0.1f, &selected_interval) ==
           TECLA_BATTERY_INTERVAL_INVALID_PERCENT);
    assert(tecla_battery_interval_for_percent(true, 100.1f, &selected_interval) ==
           TECLA_BATTERY_INTERVAL_INVALID_PERCENT);
    assert(tecla_battery_interval_for_percent(true, 50.0f, NULL) ==
           TECLA_BATTERY_INTERVAL_INVALID_ARGUMENT);

    tecla_battery_policy_t policy = {0};
    assert(tecla_battery_policy_check(&policy, 600U, true, 4200U) ==
           TECLA_BATTERY_SLEEP_UNCALIBRATED);
    policy.threshold_calibrated[0] = true;
    policy.minimum_mv[0] = 3700U; /* Synthetic test-only threshold. */
    assert(tecla_battery_policy_check(&policy, 600U, true, 3700U) ==
           TECLA_BATTERY_SLEEP_ALLOWED);
    assert(tecla_battery_policy_check(&policy, 600U, true, 3699U) ==
           TECLA_BATTERY_SLEEP_BELOW_THRESHOLD);
    assert(tecla_battery_policy_check(&policy, 600U, false, 4200U) ==
           TECLA_BATTERY_SLEEP_NO_MEASUREMENT);
    assert(tecla_battery_policy_check(&policy, 601U, true, 4200U) ==
           TECLA_BATTERY_SLEEP_INVALID_INTERVAL);
    assert(tecla_battery_policy_check(&policy, 1200U, true, 4200U) ==
           TECLA_BATTERY_SLEEP_UNCALIBRATED);
    const uint32_t intervals[] = {600U, 1200U, 2400U, 3600U};
    for (unsigned i = 1U; i < TECLA_BATTERY_INTERVAL_COUNT; ++i) {
        policy.threshold_calibrated[i] = true;
        policy.minimum_mv[i] = (uint16_t)(3700U + i * 50U);
    }
    for (unsigned i = 0U; i < TECLA_BATTERY_INTERVAL_COUNT; ++i) {
        assert(tecla_battery_policy_check(&policy, intervals[i], true,
                policy.minimum_mv[i]) == TECLA_BATTERY_SLEEP_ALLOWED);
    }

    assert(tecla_medicion_es_plausible(TECLA_TIPO_VALOR_TEMPERATURA, -40.0f));
    assert(tecla_medicion_es_plausible(TECLA_TIPO_VALOR_TEMPERATURA, 80.0f));
    assert(!tecla_medicion_es_plausible(TECLA_TIPO_VALOR_TEMPERATURA, 80.1f));
    assert(tecla_medicion_es_plausible(TECLA_TIPO_VALOR_HUMEDAD, 100.0f));
    assert(!tecla_medicion_es_plausible(TECLA_TIPO_VALOR_HUMEDAD, -0.1f));
    assert(!tecla_medicion_es_plausible(TECLA_TIPO_VALOR_HUMEDAD, 100.1f));
    assert(tecla_medicion_es_plausible(TECLA_TIPO_VALOR_LUZ, 0.0f));
    assert(!tecla_medicion_es_plausible(TECLA_TIPO_VALOR_LUZ, NAN));
    assert(!tecla_medicion_es_plausible(TECLA_TIPO_VALOR_LUZ, INFINITY));
    assert(!tecla_medicion_es_plausible(TECLA_TIPO_VALOR_HUMEDAD_SUELO, 50.0f));
    puts("PASS battery_percentage_schedule_sleep_gate_and_measurement_validation");
    return 0;
}
