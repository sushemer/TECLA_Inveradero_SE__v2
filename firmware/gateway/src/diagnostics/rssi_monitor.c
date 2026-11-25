#include "rssi_monitor.h"

#include <stddef.h>
#include <string.h>

void tecla_rssi_monitor_init(tecla_rssi_monitor_t *monitor)
{
    if (monitor != NULL) memset(monitor, 0, sizeof(*monitor));
}

static tecla_rssi_node_window_t *find_window(tecla_rssi_monitor_t *monitor,
                                             uint32_t node_id,
                                             bool create)
{
    tecla_rssi_node_window_t *empty = NULL;
    for (size_t i = 0; i < TECLA_RSSI_MONITOR_MAX_NODES; ++i) {
        tecla_rssi_node_window_t *window = &monitor->nodes[i];
        if (window->count != 0U && window->firmware_node_id == node_id) return window;
        if (window->count == 0U && empty == NULL) empty = window;
    }
    if (!create || empty == NULL) return NULL;
    empty->firmware_node_id = node_id;
    return empty;
}

bool tecla_rssi_monitor_record(tecla_rssi_monitor_t *monitor,
                               uint32_t firmware_node_id,
                               int8_t rssi_dbm,
                               uint64_t captured_monotonic_us)
{
    if (monitor == NULL || firmware_node_id == 0U) return false;
    tecla_rssi_node_window_t *window = find_window(monitor, firmware_node_id, true);
    if (window == NULL) return false;
    window->values_dbm[window->next] = rssi_dbm;
    window->timestamps_us[window->next] = captured_monotonic_us;
    window->next = (uint8_t)((window->next + 1U) % TECLA_RSSI_MONITOR_WINDOW_SIZE);
    if (window->count < TECLA_RSSI_MONITOR_WINDOW_SIZE) window->count++;
    return true;
}

bool tecla_rssi_monitor_snapshot(const tecla_rssi_monitor_t *monitor,
                                 uint32_t firmware_node_id,
                                 uint64_t now_monotonic_us,
                                 uint64_t stale_after_us,
                                 tecla_rssi_snapshot_t *snapshot)
{
    if (monitor == NULL || snapshot == NULL || firmware_node_id == 0U) return false;
    const tecla_rssi_node_window_t *window = NULL;
    for (size_t i = 0; i < TECLA_RSSI_MONITOR_MAX_NODES; ++i) {
        if (monitor->nodes[i].count != 0U &&
            monitor->nodes[i].firmware_node_id == firmware_node_id) {
            window = &monitor->nodes[i];
            break;
        }
    }
    if (window == NULL) return false;

    int32_t sum = 0;
    int8_t min_dbm = window->values_dbm[0];
    int8_t max_dbm = window->values_dbm[0];
    for (uint8_t i = 0; i < window->count; ++i) {
        const int8_t value = window->values_dbm[i];
        sum += value;
        if (value < min_dbm) min_dbm = value;
        if (value > max_dbm) max_dbm = value;
    }

    uint8_t last_index = 0U;
    for (uint8_t i = 1; i < window->count; ++i) {
        if (window->timestamps_us[i] > window->timestamps_us[last_index]) last_index = i;
    }
    const uint64_t last_seen = window->timestamps_us[last_index];
    const uint64_t age = now_monotonic_us >= last_seen ? now_monotonic_us - last_seen : 0U;
    *snapshot = (tecla_rssi_snapshot_t){
        .firmware_node_id = firmware_node_id,
        .last_dbm = window->values_dbm[last_index],
        .min_dbm = min_dbm,
        .max_dbm = max_dbm,
        .average_dbm = (float)sum / (float)window->count,
        .sample_count = window->count,
        .last_seen_monotonic_us = last_seen,
        .age_us = age,
        .stale = stale_after_us != 0U && age > stale_after_us,
    };
    return true;
}
