param([string]$Results = 'results3d/p0_benchmark_v2')
$ErrorActionPreference = 'Stop'
$rows = @(Import-Csv (Join-Path $Results 'frames.csv'))
if ($rows.Count -ne 6) { throw 'Expected three baseline and three stress runs' }
$ci = [Globalization.CultureInfo]::InvariantCulture
function Number([string]$value) { [double]::Parse($value,$ci) }
foreach ($scene in @('baseline','stress')) {
    $group = @($rows | Where-Object scene -eq $scene)
    if ($group.Count -ne 3 -or (($group.repeat | Sort-Object) -join ',') -ne '1,2,3') { throw "Bad repetitions: $scene" }
    foreach ($row in $group) {
        if ((Number $row.p95_ms) -gt 33.334 -or (Number $row.p99_ms) -gt 50) { throw "Frame budget exceeded: $scene/$($row.repeat)" }
        if ((Number $row.peak_working_set_bytes) -gt 536870912) { throw 'Memory budget exceeded' }
        if ((Number $row.input_samples) -lt 50 -or (Number $row.input_p95_ms) -gt 50) { throw 'Insufficient or slow software response samples' }
        if ($row.width -ne '1280' -or $row.height -ne '720' -or $row.viewport_w -ne '1250' -or $row.viewport_h -ne '780') { throw 'Benchmark size changed' }
    }
}
$inputTimes = @(Get-Content (Join-Path $Results 'events.txt') | ForEach-Object {
    if ($_ -match '^input_to_draw_ms=([0-9.]+) source=scripted_controller') { Number $Matches[1] }
})
$maxInput = ($inputTimes | Measure-Object -Maximum).Maximum
if ($inputTimes.Count -lt 300 -or $maxInput -gt 250) { throw 'Software maximum latency budget exceeded or samples missing' }
$summary = [ordered]@{
    evidence='actual OpenGL window; scripted controller input-to-EndDrawing, not display latency'
    passed=$true; rows=$rows; input_samples=$inputTimes.Count; max_input_ms=$maxInput
    limits=[ordered]@{p95_ms=33.334;p99_ms=50;working_set_bytes=536870912;input_p95_ms=50;input_max_ms=250}
}
$summary | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $Results 'audit.json') -Encoding utf8
Write-Output "PASS P0b frame/memory/software-response baseline; $($inputTimes.Count) probes, max=$maxInput ms"
