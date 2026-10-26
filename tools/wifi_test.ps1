# River Bot Wi-Fi test helper — on-demand only, never churns profiles.
#
# Usage (from anywhere):
#   powershell -ExecutionPolicy Bypass -File tools\wifi_test.ps1 status
#   powershell -ExecutionPolicy Bypass -File tools\wifi_test.ps1 connect   # explicit ESP32 test only
#   powershell -ExecutionPolicy Bypass -File tools\wifi_test.ps1 restore   # after the test
#
# Policy (see docs/troubleshooting.md):
#   - Normal development stays on the home hotspot (realme 9 5G, auto-connect).
#   - 'connect' joins RiverBot-AP on explicit request only. It sets the home
#     profile to manual for the duration of the test (prevents the Ralink
#     roaming away mid-test) but NEVER deletes or recreates profiles.
#   - 'restore' puts the home profile back to auto-connect, reconnects, and
#     verifies the association.
#   - If a plain connect fails, ONE profile repair (export/delete/re-add from
#     the cached %TEMP%\riverbot.xml) is attempted, then a final plain
#     connect. No retry loops.

param([ValidateSet('status', 'connect', 'restore')][string]$Action = 'status')

$BOT_SSID  = 'RiverBot-AP'
$HOME_SSID = 'realme 9 5G'

function Get-CurrentSsid {
    $line = netsh wlan show interfaces | Select-String '^\s+SSID\s*:' | Select-Object -First 1
    if (-not $line) { return '' }
    ($line.ToString() -split ':', 2)[1].Trim()
}

function Wait-Ssid([string]$Wanted, [int]$Seconds = 10) {
    for ($i = 0; $i -lt $Seconds; $i++) {
        Start-Sleep 1
        if ((Get-CurrentSsid) -eq $Wanted) { return ($i + 1) }
    }
    return 0
}

switch ($Action) {
    'status' {
        "current ssid: $(Get-CurrentSsid)"
    }
    'connect' {
        $cur = Get-CurrentSsid
        if ($cur -eq $BOT_SSID) { "already on $BOT_SSID"; exit 0 }
        "current: $cur -> joining $BOT_SSID (home profile set manual for this test)"
        netsh wlan set profileparameter name=$HOME_SSID connectionmode=manual | Out-Null
        netsh wlan connect name=$BOT_SSID | Out-Null
        $t = Wait-Ssid $BOT_SSID 10
        if ($t -gt 0) { "joined after ${t}s"; exit 0 }
        # ONE last-resort profile repair (never a loop)
        "plain connect failed; performing ONE profile repair"
        $xml = "$env:TEMP\riverbot.xml"
        if (-not (Test-Path $xml)) {
            netsh wlan export profile name=$BOT_SSID folder="$env:TEMP" key=clear | Out-Null
            $f = Get-ChildItem "$env:TEMP\*RiverBot*.xml" -ErrorAction SilentlyContinue | Select-Object -First 1
            if ($f) { $xml = $f.FullName }
        }
        if (Test-Path $xml) {
            netsh wlan delete profile name=$BOT_SSID | Out-Null
            netsh wlan add profile filename=$xml user=all | Out-Null
            netsh wlan connect name=$BOT_SSID | Out-Null
            $t = Wait-Ssid $BOT_SSID 10
            if ($t -gt 0) { "joined after single repair (${t}s)"; exit 0 }
        }
        "FAILED to join $BOT_SSID - run 'restore' to bring back home wifi"
        exit 1
    }
    'restore' {
        "restoring home wifi ($HOME_SSID, auto-connect)"
        netsh wlan set profileparameter name=$HOME_SSID connectionmode=auto | Out-Null
        netsh wlan connect name=$HOME_SSID | Out-Null
        $t = Wait-Ssid $HOME_SSID 10
        if ($t -gt 0) { "restored $HOME_SSID after ${t}s (verified)"; exit 0 }
        "restore FAILED - current ssid: $(Get-CurrentSsid)"
        exit 1
    }
}
