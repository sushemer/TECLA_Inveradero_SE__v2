#ifndef TECLA_GATEWAY_RSSI_MONITOR_H
#define TECLA_GATEWAY_RSSI_MONITOR_H

#include <stdbool.h>
#include <stdint.h>

#define TECLA_RSSI_MONITOR_MAX_NODES 8U
#define TECLA_RSSI_MONITOR_WINDOW_SIZE 16U

typedef struct {
    uint32_t firmware_node_id;
    int8_t last_dbm;
    int8_t min_dbm;
    int8_t max_dbm;
    float average_dbm;
    uint8_t sample_count;
    uint64_t last_seen_monotonic_us;
    uint64_t age_us;
    bool stale;
} tecla_rssi_snapshot_t;

typedef struct {
    uint32_t firmware_node_id;
    uint64_t timestamps_us[TECLA_RSSI_MONITOR_WINDOW_SIZE];
    int8_t values_dbm[TECLA_RSSI_MONITOR_WINDOW_SIZE];
    uint8_t count;
    uint8_t next;
} tecla_rssi_node_window_t;

typedef struct {
    tecla_rssi_node_window_t nodes[TECLA_RSSI_MONITOR_MAX_NODES];
} tecla_rssi_monitor_t;

void tecla_rssi_monitor_init(tecla_rssi_monitor_t *monitor);
/* Call only from the gateway's worker task, never from the Wi-Fi callback. */
bool tecla_rssi_monitor_record(tecla_rssi_monitor_t *monitor,
                               uint32_t firmware_node_id,
                               int8_t rssi_dbm,
                               uint64_t captured_monotonic_us);
bool tecla_rssi_monitor_snapshot(const tecla_rssi_monitor_t *monitor,
                                 uint32_t firmware_node_id,
                                 uint64_t now_monotonic_us,
                                 uint64_t stale_after_us,
                                 tecla_rssi_snapshot_t *snapshot);

#endif
