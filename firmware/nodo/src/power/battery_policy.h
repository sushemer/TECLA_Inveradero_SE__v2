#ifndef TECLA_BATTERY_POLICY_H
#define TECLA_BATTERY_POLICY_H

#include <stdbool.h>
#include <stdint.h>

#define TECLA_BATTERY_INTERVAL_COUNT 4U

typedef enum {
    TECLA_BATTERY_SLEEP_ALLOWED = 0,
    TECLA_BATTERY_SLEEP_UNCALIBRATED,
    TECLA_BATTERY_SLEEP_NO_MEASUREMENT,
    TECLA_BATTERY_SLEEP_BELOW_THRESHOLD,
    TECLA_BATTERY_SLEEP_INVALID_INTERVAL
} tecla_battery_sleep_decision_t;

typedef enum {
    TECLA_BATTERY_INTERVAL_SELECTED = 0,
    TECLA_BATTERY_INTERVAL_NO_MEASUREMENT,
    TECLA_BATTERY_INTERVAL_INVALID_PERCENT,
    TECLA_BATTERY_INTERVAL_INVALID_ARGUMENT
} tecla_battery_interval_decision_t;

typedef struct {
    /* A zero/unset calibrated flag means fail-safe: deep sleep is not authorized. */
    bool threshold_calibrated[TECLA_BATTERY_INTERVAL_COUNT];
    uint16_t minimum_mv[TECLA_BATTERY_INTERVAL_COUNT];
} tecla_battery_policy_t;

/* Percentage schedule supplied by the measurement source; no voltage conversion occurs here.
 * Boundaries are intentional: 50 -> 1200 s, 25 -> 2400 s, 10 -> 3600 s. */
tecla_battery_interval_decision_t tecla_battery_interval_for_percent(
    bool measurement_valid,
    float battery_percent,
    uint32_t *interval_seconds_out);

/* Supported nominal sampling intervals: 10, 20, 40, and 60 minutes. */
tecla_battery_sleep_decision_t tecla_battery_policy_check(
    const tecla_battery_policy_t *policy,
    uint32_t interval_seconds,
    bool battery_measurement_valid,
    uint16_t battery_mv);

#endif
