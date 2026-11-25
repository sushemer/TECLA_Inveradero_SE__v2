$ErrorActionPreference = 'Stop'
$appPath = Join-Path $PSScriptRoot '..\src\app\app_main.c'
$source = Get-Content -LiteralPath $appPath -Raw
$match = [regex]::Match($source, '(?s)static\s+esp_err_t\s+enviar_backend\s*\(.*?\n}\s*\n\s*static\s+esp_err_t\s+enviar_ack')
if (-not $match.Success) { throw 'No se pudo localizar el serializador central enviar_backend().' }
if ($match.Value -match '(?i)rssi') { throw 'El serializador central referencia RSSI; no debe enviarlo a la API.' }
$callback = [regex]::Match($source, '(?s)static\s+void\s+recibir_telemetria\s*\(.*?\n}\s*\n\s*static\s+esp_err_t\s+agregar_peer')
if (-not $callback.Success) { throw 'No se pudo localizar el callback ESP-NOW.' }
if ($callback.Value -notmatch 'rx_ctrl->rssi' -or
    $callback.Value -notmatch 'captured_monotonic_us' -or
    $callback.Value -notmatch 'xQueueSend\(telemetry_queue' -or
    $callback.Value -match 'tecla_rssi_monitor_record') {
    throw 'El callback debe copiar RSSI/tiempo a la cola y dejar el agregado al worker.'
}
Write-Output 'PASS callback copies RSSI metadata to queue without processing; central serializer excludes RSSI'
