#include "../src/diagnostics/rssi_monitor.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

int main(void)
{
    tecla_rssi_monitor_t monitor;
    tecla_rssi_monitor_init(&monitor);
    assert(tecla_rssi_monitor_record(&monitor, 70001U, -80, 100U));
    assert(tecla_rssi_monitor_record(&monitor, 70001U, -60, 200U));
    assert(tecla_rssi_monitor_record(&monitor, 70001U, -70, 300U));
    assert(tecla_rssi_monitor_record(&monitor, 70002U, -45, 250U));

    tecla_rssi_snapshot_t s;
    assert(tecla_rssi_monitor_snapshot(&monitor, 70001U, 500U, 250U, &s));
    assert(s.last_dbm == -70 && s.min_dbm == -80 && s.max_dbm == -60);
    assert(s.sample_count == 3U && fabsf(s.average_dbm - (-70.0f)) < 0.001f);
    assert(s.age_us == 200U && !s.stale);
    assert(tecla_rssi_monitor_snapshot(&monitor, 70002U, 1000U, 500U, &s));
    assert(s.sample_count == 1U && s.last_dbm == -45 && s.stale);

    for (uint64_t i = 0; i < TECLA_RSSI_MONITOR_WINDOW_SIZE + 2U; ++i) {
        assert(tecla_rssi_monitor_record(&monitor, 70003U, (int8_t)(-90 + i), 1000U + i));
    }
    assert(tecla_rssi_monitor_snapshot(&monitor, 70003U, 2000U, 0U, &s));
    assert(s.sample_count == TECLA_RSSI_MONITOR_WINDOW_SIZE);
    assert(s.min_dbm == -88 && s.max_dbm == -73 && s.last_dbm == -73);
    assert(!tecla_rssi_monitor_snapshot(&monitor, 70004U, 2000U, 0U, &s));
    assert(tecla_rssi_monitor_record(&monitor, 70004U, -55, 0U));
    assert(tecla_rssi_monitor_snapshot(&monitor, 70004U, 0U, 1U, &s));
    assert(s.last_dbm == -55 && s.age_us == 0U && !s.stale);
    puts("PASS rssi_window_isolation_age_and_capacity");
    return 0;
}
