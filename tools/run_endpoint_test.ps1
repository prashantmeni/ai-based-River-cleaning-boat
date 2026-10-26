# Endpoint matrix test orchestrator (continue.txt / test.txt workflow).
# Steps: verify internet -> prepare -> join RiverBot-AP -> DHCP wait ->
# bounded endpoint probe (logs saved) -> ALWAYS restore in finally ->
# verify internet -> report camera/endpoint result separately.
#
# Usage: powershell -ExecutionPolicy Bypass -File tools\run_endpoint_test.ps1

$ErrorActionPreference = 'Stop'
$here   = Split-Path -Parent $MyInvocation.MyCommand.Path
$root   = Split-Path -Parent $here
$logdir = Join-Path $root 'logs'
$ts     = Get-Date -Format 'yyyyMMdd_HHmmss'
New-Item -ItemType Directory -Force -Path $logdir | Out-Null
Start-Transcript -Path (Join-Path $logdir "orchestrator_ep_$ts.log") | Out-Null

function Test-Internet {
    try {
        $r = Invoke-WebRequest 'https://connectivitycheck.gstatic.com/generate_204' `
             -TimeoutSec 6 -UseBasicParsing
        return ($r.StatusCode -eq 204)
    } catch { return $false }
}

$probe_ok = $false
try {
    $inet_before = Test-Internet
    "step1 internet before: $inet_before"
    $wifi  = Join-Path $here 'wifi_test.ps1'
    $probe = Join-Path $root 'tests\integration\test_endpoints.py'
    foreach ($f in @($wifi, $probe)) {
        if (-not (Test-Path $f)) { throw "missing script: $f" }
    }
    "step2 scripts ready"
    & $wifi connect
    if ($LASTEXITCODE -ne 0) { throw "wifi connect failed" }
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
    $elog = Join-Path $logdir "endpoints_$ts.log"
    "step5 endpoint probe -> $elog"
    python -u $probe --log $elog --stream-seconds 12
    $probe_ok = ($LASTEXITCODE -eq 0)
}
finally {
    "step7 finally: restoring home wifi"
    & (Join-Path $here 'wifi_test.ps1') restore
}

$inet_after = Test-Internet
"===== RESULTS ====="
if ($probe_ok) { "endpoint probe : DONE" } else { "endpoint probe : FAIL (see logs)" }
"internet       : before=$inet_before  after=$inet_after"
"logs: $logdir"
Stop-Transcript | Out-Null
if ($probe_ok -and $inet_after) { exit 0 } else { exit 1 }
