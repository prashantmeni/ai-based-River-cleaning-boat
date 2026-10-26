# Continuous stream test orchestrator - implements PROMPTS\test.txt.
#
# Steps: 1 verify internet  2 prepare scripts  3 switch to RiverBot-AP
#        4 wait DHCP 192.168.4.x  5 continuous stream (logs saved locally)
#        7 ALWAYS restore in finally  8 verify internet again
#        9 report camera result SEPARATELY from internet result.
#
# Usage: powershell -ExecutionPolicy Bypass -File tools\run_stream_test.ps1
#        [-StreamSeconds 60]

param([int]$StreamSeconds = 60)

$ErrorActionPreference = 'Stop'
$here    = Split-Path -Parent $MyInvocation.MyCommand.Path
$root    = Split-Path -Parent $here
$logdir  = Join-Path $root 'logs'
$ts      = Get-Date -Format 'yyyyMMdd_HHmmss'
New-Item -ItemType Directory -Force -Path $logdir | Out-Null
$transcript = Join-Path $logdir "orchestrator_$ts.log"
Start-Transcript -Path $transcript | Out-Null

function Test-Internet {
    try {
        $r = Invoke-WebRequest 'https://connectivitycheck.gstatic.com/generate_204' `
             -TimeoutSec 6 -UseBasicParsing
        return ($r.StatusCode -eq 204)
    } catch { return $false }
}

$cam_ok = $false
try {
    # step 1: internet BEFORE (must be on realme 9 5G)
    $inet_before = Test-Internet
    "step1 internet before test: $inet_before"

    # step 2: prepare everything locally BEFORE switching Wi-Fi
    $wifi  = Join-Path $here 'wifi_test.ps1'
    $probe = Join-Path $root 'tests\integration\test_stream.py'
    foreach ($f in @($wifi, $probe)) {
        if (-not (Test-Path $f)) { throw "missing script: $f" }
    }
    "step2 scripts ready (no firmware change, no AI API calls)"

    # step 3: switch to RiverBot-AP only for this test
    & $wifi connect
    if ($LASTEXITCODE -ne 0) { throw "wifi_test.ps1 connect failed" }

    # step 4: wait for DHCP in 192.168.4.x
    $lease = $null
    for ($i = 0; $i -lt 15; $i++) {
        Start-Sleep 1
        $lease = Get-NetIPAddress -AddressFamily IPv4 -InterfaceAlias 'Wi-Fi' `
                 -ErrorAction SilentlyContinue |
                 Where-Object { $_.IPAddress -like '192.168.4.*' } |
                 Select-Object -First 1
        if ($lease) { break }
    }
    if (-not $lease) { throw "no 192.168.4.x DHCP lease after 15s" }
    "step4 dhcp lease $($lease.IPAddress) after $($i + 1)s"

    # step 5: continuous stream test, all logs saved locally
    $slog = Join-Path $logdir "stream_test_$ts.log"
    "step5 continuous stream test ($StreamSeconds)s -> $slog"
    python -u $probe --seconds $StreamSeconds --log $slog
    $cam_ok = ($LASTEXITCODE -eq 0)
}
finally {
    # step 7: ALWAYS restore home Wi-Fi, even after timeout/failure
    "step7 finally: restoring home wifi"
    & (Join-Path $here 'wifi_test.ps1') restore
}

# step 8: internet AFTER
$inet_after = Test-Internet

# step 9: separate results (camera vs internet/API)
"===== RESULTS ====="
if ($cam_ok) { "camera stream : PASS" } else { "camera stream : FAIL  (see $slog)" }
"internet      : before=$inet_before  after=$inet_after"
"serial+stream logs: $logdir"
Stop-Transcript | Out-Null
if ($cam_ok -and $inet_after) { exit 0 } else { exit 1 }
