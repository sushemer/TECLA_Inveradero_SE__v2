#include "local_server.h"

#include <stdio.h>
#include <string.h>

#include "esp_http_server.h"
#include "esp_timer.h"

#define TECLA_LOCAL_SERVER_MAX_NODES 8U
#define TECLA_RSSI_STALE_AFTER_US UINT64_C(60000000)

static tecla_rssi_monitor_t *s_monitor;
static SemaphoreHandle_t s_monitor_mutex;
static tecla_gateway_reading_average_cache_t *s_average_cache;
static SemaphoreHandle_t s_average_mutex;
static tecla_gateway_reading_average_t s_average_snapshot[TECLA_GATEWAY_AVERAGE_MAX_SERIES];
static uint32_t s_node_ids[TECLA_LOCAL_SERVER_MAX_NODES];
static uint8_t s_node_count;
static httpd_handle_t s_server;

static esp_err_t page_handler(httpd_req_t *request)
{
    static const char page[] =
        "<!doctype html><html lang='es'><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>TECLA Gateway</title><style>body{font:16px system-ui;margin:2rem;"
        "background:#101820;color:#edf3f4}table{border-collapse:collapse;width:100%}"
        "td,th{padding:.6rem;border-bottom:1px solid #52616b;text-align:left}"
        ".muted{color:#aab7b8}</style><h1>Diagnóstico local del gateway</h1>"
        "<p class='muted'>RSSI recibido por el gateway en ESP-NOW. No es estado del nodo ni del sensor.</p>"
        "<table><thead><tr><th>Nodo</th><th>Último RSSI</th><th>Ventana min / promedio / máx</th>"
        "<th>Muestras</th><th>Edad</th></tr></thead><tbody id='rows'></tbody></table>"
        "<h2>Promedios ambientales temporales del gateway</h2>"
        "<p class='muted'>Media aritmética de muestras válidas del nodo. Ventana móvil de 3 h; mínimo 3 muestras."
        " Solo RAM: se pierde al reiniciar. No reemplaza lecturas originales ni se envía al servidor.</p>"
        "<table><thead><tr><th>Nodo / slot</th><th>Medición</th><th>Promedio</th><th>Muestras</th><th>Ventana (edad)</th></tr></thead>"
        "<tbody id='averages'><tr><td colspan='5'>Esperando muestras válidas...</td></tr></tbody></table>"
        "<script>async function refresh(){try{const r=await fetch('/api/nodes');"
        "const d=await r.json();document.querySelector('#rows').innerHTML=d.nodes.map(n=>"
        "`<tr><td>${n.firmware_node_id}</td><td>${n.rssi_dbm_last===null?'sin dato':n.rssi_dbm_last+' dBm'}</td>"
        "<td>${n.rssi_window?`${n.rssi_window.min_dbm} / ${n.rssi_window.avg_dbm} / ${n.rssi_window.max_dbm} dBm`:''}</td>"
        "<td>${n.rssi_window?n.rssi_window.count:0}</td><td>${n.rssi_age_ms===null?'—':n.rssi_age_ms+' ms'} ${n.stale?'(obsoleto)':''}</td></tr>`).join('')}"
        "catch(e){document.querySelector('#rows').innerHTML='<tr><td colspan=5>sin dato</td></tr>'}"
        "try{const r=await fetch('/api/averages');const d=await r.json();"
        "const names={1:'Temperatura',2:'Humedad relativa',7:'Iluminancia'};"
        "document.querySelector('#averages').innerHTML=d.averages.length?d.averages.map(a=>"
        "`<tr><td>${a.firmware_node_id} / ${a.slot_code}</td><td>${names[a.measurement_code]||('Medición '+a.measurement_code)}</td>"
        "<td>${a.ready?a.mean.toFixed(2):'Insuficientes'}</td><td>${a.sample_count} / ${d.minimum_sample_count}</td>"
        "<td>última hace ${a.window_end_age_seconds}s; primera hace ${a.window_start_age_seconds}s</td></tr>`).join(''):"
        "'<tr><td colspan=5>Sin muestras válidas en la ventana de 3 horas.</td></tr>'}"
        "catch(e){document.querySelector('#averages').innerHTML='<tr><td colspan=5>promedio local no disponible</td></tr>'}}"
        "refresh();setInterval(refresh,2000)</script></html>";
    httpd_resp_set_type(request, "text/html; charset=utf-8");
    return httpd_resp_send(request, page, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t nodes_handler(httpd_req_t *request)
{
    char body[1200];
    size_t used = 0U;
    const int written = snprintf(body, sizeof(body), "{\"nodes\":[");
    if (written < 0 || (size_t)written >= sizeof(body)) return ESP_FAIL;
    used = (size_t)written;
    const uint64_t now_us = (uint64_t)esp_timer_get_time();

    if (xSemaphoreTake(s_monitor_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        httpd_resp_set_status(request, "503 Service Unavailable");
        return httpd_resp_sendstr(request, "diagnostics busy");
    }
    for (uint8_t i = 0; i < s_node_count; ++i) {
        tecla_rssi_snapshot_t s;
        const bool found = tecla_rssi_monitor_snapshot(s_monitor, s_node_ids[i], now_us,
                                                        TECLA_RSSI_STALE_AFTER_US, &s);
        const uint64_t age_ms_64 = found ? s.age_us / 1000U : 0U;
        const uint32_t age_ms = age_ms_64 > UINT32_MAX ? UINT32_MAX : (uint32_t)age_ms_64;
        const int n = found
            ? snprintf(body + used, sizeof(body) - used,
                "%s{\"firmware_node_id\":%lu,\"rssi_dbm_last\":%d,\"rssi_age_ms\":%lu,\"stale\":%s,\"rssi_window\":{\"count\":%u,\"min_dbm\":%d,\"avg_dbm\":%.1f,\"max_dbm\":%d}}",
                i == 0U ? "" : ",", (unsigned long)s.firmware_node_id,
                (int)s.last_dbm, (unsigned long)age_ms,
                s.stale ? "true" : "false", (unsigned)s.sample_count,
                (int)s.min_dbm, (double)s.average_dbm, (int)s.max_dbm)
            : snprintf(body + used, sizeof(body) - used,
                "%s{\"firmware_node_id\":%lu,\"rssi_dbm_last\":null,\"rssi_age_ms\":null,\"stale\":true,\"rssi_window\":null}",
                i == 0U ? "" : ",", (unsigned long)s_node_ids[i]);
        if (n < 0 || (size_t)n >= sizeof(body) - used) {
            xSemaphoreGive(s_monitor_mutex);
            return ESP_ERR_NO_MEM;
        }
        used += (size_t)n;
    }
    xSemaphoreGive(s_monitor_mutex);
    if (used + 3U > sizeof(body)) return ESP_ERR_NO_MEM;
    memcpy(body + used, "]}", 3U);
    httpd_resp_set_type(request, "application/json; charset=utf-8");
    return httpd_resp_send(request, body, (ssize_t)(used + 2U));
}

static void hash_prefix_hex(const uint8_t hash[32], char out[9])
{
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0U; i < 4U; ++i) {
        out[i * 2U] = hex[hash[i] >> 4];
        out[i * 2U + 1U] = hex[hash[i] & 0x0fU];
    }
    out[8] = '\0';
}

static esp_err_t averages_handler(httpd_req_t *request)
{
    uint16_t average_count = 0U;
    uint64_t now_us = (uint64_t)esp_timer_get_time();
    uint32_t duplicate_frames = 0U;
    uint32_t conflicting_frames = 0U;
    uint32_t evicted_samples = 0U;
    uint32_t dedupe_evictions = 0U;
    uint32_t dropped_series = 0U;
    if (s_average_cache != NULL && s_average_mutex != NULL) {
        if (xSemaphoreTake(s_average_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            httpd_resp_set_status(request, "503 Service Unavailable");
            return httpd_resp_sendstr(request, "diagnostics busy");
        }
        const tecla_gateway_average_status_t status =
            tecla_gateway_reading_average_snapshot(s_average_cache, now_us,
                s_average_snapshot, TECLA_GATEWAY_AVERAGE_MAX_SERIES,
                &average_count);
        duplicate_frames = s_average_cache->duplicate_frames;
        conflicting_frames = s_average_cache->conflicting_frames;
        evicted_samples = s_average_cache->evicted_samples;
        dedupe_evictions = s_average_cache->dedupe_evictions;
        dropped_series = s_average_cache->dropped_series;
        xSemaphoreGive(s_average_mutex);
        if (status != TECLA_AVERAGE_OK) return ESP_FAIL;
    }

    httpd_resp_set_type(request, "application/json; charset=utf-8");
    const bool active = s_average_cache != NULL;
    char prefix[384];
    const int prefix_len = snprintf(prefix, sizeof(prefix),
        "{\"kind\":\"gateway_ram_arithmetic_mean\",\"window_seconds\":10800,"
        "\"minimum_sample_count\":3,\"durability\":\"volatile_ram_lost_on_gateway_restart\","
        "\"active\":%s,\"cache_stats\":{\"duplicate_frames\":%lu,"
        "\"conflicting_frames\":%lu,\"evicted_samples\":%lu,\"dedupe_evictions\":%lu,"
        "\"dropped_series\":%lu},"
        "\"averages\":[", active ? "true" : "false",
        (unsigned long)duplicate_frames, (unsigned long)conflicting_frames,
        (unsigned long)evicted_samples, (unsigned long)dedupe_evictions,
        (unsigned long)dropped_series);
    if (prefix_len < 0 || (size_t)prefix_len >= sizeof(prefix) ||
        httpd_resp_send_chunk(request, prefix, prefix_len) != ESP_OK) return ESP_FAIL;
    if (!active) {
        if (httpd_resp_sendstr_chunk(request, "]}") != ESP_OK) return ESP_FAIL;
        return httpd_resp_sendstr_chunk(request, NULL);
    }

    char row[512];
    for (uint16_t i = 0U; i < average_count; ++i) {
        const tecla_gateway_reading_average_t *average = &s_average_snapshot[i];
        char manifest_prefix[9];
        char assignment_prefix[9];
        hash_prefix_hex(average->manifest_revision, manifest_prefix);
        hash_prefix_hex(average->assignment_revision, assignment_prefix);
        const uint32_t start_age_s = now_us >= average->window_start_monotonic_us
            ? (uint32_t)((now_us - average->window_start_monotonic_us) / UINT64_C(1000000)) : 0U;
        const uint32_t end_age_s = now_us >= average->window_end_monotonic_us
            ? (uint32_t)((now_us - average->window_end_monotonic_us) / UINT64_C(1000000)) : 0U;
        char mean[32];
        if (average->ready) {
            const int mean_len = snprintf(mean, sizeof(mean), "%.6f", (double)average->average);
            if (mean_len < 0 || (size_t)mean_len >= sizeof(mean)) return ESP_FAIL;
        } else {
            memcpy(mean, "null", sizeof("null"));
        }
        const int written = snprintf(row, sizeof(row),
            "%s{\"firmware_node_id\":%lu,\"slot_code\":%u,\"measurement_code\":%u,"
            "\"mean\":%s,\"sample_count\":%u,\"ready\":%s,"
            "\"window_start_age_seconds\":%lu,\"window_end_age_seconds\":%lu,"
            "\"manifest_revision_prefix\":\"%s\",\"assignment_revision_prefix\":\"%s\"}",
            i == 0U ? "" : ",", (unsigned long)average->firmware_node_id,
            (unsigned)average->slot_code, (unsigned)average->measurement_code,
            mean, (unsigned)average->sample_count,
            average->ready ? "true" : "false",
            (unsigned long)start_age_s, (unsigned long)end_age_s,
            manifest_prefix, assignment_prefix);
        if (written < 0 || (size_t)written >= sizeof(row)) return ESP_FAIL;
        if (httpd_resp_sendstr_chunk(request, row) != ESP_OK) return ESP_FAIL;
    }
    if (httpd_resp_sendstr_chunk(request, "]}") != ESP_OK) return ESP_FAIL;
    return httpd_resp_sendstr_chunk(request, NULL);
}

esp_err_t tecla_gateway_local_server_start(tecla_rssi_monitor_t *monitor,
                                           SemaphoreHandle_t monitor_mutex,
                                           tecla_gateway_reading_average_cache_t *average_cache,
                                           const uint32_t *firmware_node_ids,
                                           uint8_t node_count)
{
    if (monitor == NULL || monitor_mutex == NULL || firmware_node_ids == NULL ||
        node_count == 0U || node_count > TECLA_LOCAL_SERVER_MAX_NODES) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_server != NULL) return ESP_OK;
    s_monitor = monitor;
    s_monitor_mutex = monitor_mutex;
    s_average_cache = average_cache;
    s_average_mutex = monitor_mutex;
    memcpy(s_node_ids, firmware_node_ids, (size_t)node_count * sizeof(s_node_ids[0]));
    s_node_count = node_count;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 4U;
    esp_err_t err = httpd_start(&s_server, &config);
    if (err != ESP_OK) return err;
    const httpd_uri_t page = {.uri = "/", .method = HTTP_GET, .handler = page_handler};
    const httpd_uri_t nodes = {.uri = "/api/nodes", .method = HTTP_GET, .handler = nodes_handler};
    const httpd_uri_t averages = {.uri = "/api/averages", .method = HTTP_GET, .handler = averages_handler};
    err = httpd_register_uri_handler(s_server, &page);
    if (err == ESP_OK) err = httpd_register_uri_handler(s_server, &nodes);
    if (err == ESP_OK) err = httpd_register_uri_handler(s_server, &averages);
    if (err != ESP_OK) {
        httpd_stop(s_server);
        s_server = NULL;
    }
    return err;
}
