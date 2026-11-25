#include "battery_policy.h"

#include <math.h>

tecla_battery_interval_decision_t tecla_battery_interval_for_percent(
    bool measurement_valid,
    float battery_percent,
    uint32_t *interval_seconds_out)
{
    if (interval_seconds_out == 0) {
        return TECLA_BATTERY_INTERVAL_INVALID_ARGUMENT;
    }
    *interval_seconds_out = 0U;
    if (!measurement_valid) {
        return TECLA_BATTERY_INTERVAL_NO_MEASUREMENT;
    }
    if (!isfinite(battery_percent) || battery_percent < 0.0f ||
        battery_percent > 100.0f) {
        return TECLA_BATTERY_INTERVAL_INVALID_PERCENT;
    }

    if (battery_percent > 50.0f) {
        *interval_seconds_out = 600U;
    } else if (battery_percent > 25.0f) {
        *interval_seconds_out = 1200U;
    } else if (battery_percent > 10.0f) {
        *interval_seconds_out = 2400U;
    } else {
        *interval_seconds_out = 3600U;
    }
    return TECLA_BATTERY_INTERVAL_SELECTED;
}

static int interval_index(uint32_t interval_seconds)
{
    switch (interval_seconds) {
    case 600U: return 0;
    case 1200U: return 1;
    case 2400U: return 2;
    case 3600U: return 3;
    default: return -1;
    }
}

tecla_battery_sleep_decision_t tecla_battery_policy_check(
    const tecla_battery_policy_t *policy,
    uint32_t interval_seconds,
    bool battery_measurement_valid,
    uint16_t battery_mv)
{
    const int index = interval_index(interval_seconds);
    if (index < 0) return TECLA_BATTERY_SLEEP_INVALID_INTERVAL;
    if (policy == 0 || !policy->threshold_calibrated[index] ||
        policy->minimum_mv[index] == 0U) {
        return TECLA_BATTERY_SLEEP_UNCALIBRATED;
    }
    if (!battery_measurement_valid || battery_mv == 0U) {
        return TECLA_BATTERY_SLEEP_NO_MEASUREMENT;
    }
    return battery_mv >= policy->minimum_mv[index]
        ? TECLA_BATTERY_SLEEP_ALLOWED
        : TECLA_BATTERY_SLEEP_BELOW_THRESHOLD;
}
