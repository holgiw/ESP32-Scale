# SPDX-License-Identifier: GPL-3.0-only (siehe / see LICENSE)
# Prueft alle Waagen im Netz (checkDevices.ps1) und bringt veraltete auf den Stand des Builds in build_waage:
#  - per WLAN (ArduinoOTA, espota.exe), ohne USB
#  - Waagen am USB (ESP32-C3) werden mit waagesetup.ps1 geflasht (Einstellungen bleiben, nur die Anwendung wird ersetzt);
#    danach wird das WLAN abgefragt (Enter = nicht aendern)
# Vor jedem Flash wird gefragt (Enter = Ja). Eine Waage am USB kann ihre Firmware-Version nicht melden, deshalb wird dort
# immer gefragt. Gebaut wird mit buildAll.bat.
#   .\flashAll.ps1               pruefen, dann fragen und flashen
#   .\flashAll.ps1 -CheckOnly    nur pruefen und anzeigen
#   .\flashAll.ps1 -Yes          ohne Rueckfragen (alle veralteten Waagen im Netz und alle am USB, WLAN wird nicht abgefragt)
#   .\flashAll.ps1 -Force        auch aktuelle Waagen neu einspielen
#
# Checks all scales on the network (checkDevices.ps1) and brings outdated ones to the state of the build in build_waage:
#  - over WiFi (ArduinoOTA, espota.exe), no USB needed
#  - scales on USB (ESP32-C3) are flashed with waagesetup.ps1 (settings stay, only the application is replaced);
#    afterwards the WiFi is asked for (Enter = leave unchanged)
# Before every flash it asks (Enter = yes). A scale on USB cannot report its firmware version, so it is always asked
# for. Build with buildAll.bat.
#   .\flashAll.ps1               check, then ask and flash
#   .\flashAll.ps1 -CheckOnly    only check and show
#   .\flashAll.ps1 -Yes          without questions (all outdated scales on the network and all on USB, WiFi is not asked for)
#   .\flashAll.ps1 -Force        also re-flash up-to-date scales

param([switch]$CheckOnly, [switch]$Yes, [switch]$Force)

$root = $PSScriptRoot
function Ask([string]$question) {
    if ($Yes) { return $true }
    $a = Read-Host "$question [J/n, Y/n]"
    return ($a -eq '' -or $a -match '^[JjYy]')
}

# ---- 1. Waagen im Netz pruefen / check the scales on the network
Write-Host '=== Waagen im Netz / scales on the network ==='
$devices = @(& "$root\checkDevices.ps1" -PassThru)
if ($devices.Count -eq 0) {
    Write-Host 'Keine Waage im Netz gefunden. / No scale found on the network.'
}
else {
    $devices | Sort-Object IP | Format-Table IP, Name, Chip, Firmware, Soll, Stand, HX711, MQTT, Zeit -AutoSize | Out-String -Width 200 | Write-Host
}

# ---- 2. Per WLAN (OTA) / via WiFi (OTA)
$bin = Join-Path $root 'build_waage\esp32c3\waage.ino.bin'
$espota = Get-ChildItem "$env:LOCALAPPDATA\Arduino15\packages\esp32\hardware\esp32\*\tools\espota.exe" -ErrorAction SilentlyContinue | Sort-Object FullName | Select-Object -Last 1
$failed = 0
foreach ($d in ($devices | Where-Object { $_.Soll -and ($_.Stand -eq 'aelter' -or $_.Stand -eq '?' -or $Force) })) {
    if ($CheckOnly) { Write-Host "$($d.Name) ($($d.IP)): veraltet oder unbekannt / outdated or unknown"; continue }
    if (-not (Test-Path $bin)) { Write-Host "Build fehlt / missing: $bin"; $failed++; continue }
    if (-not $espota) { Write-Host 'espota.exe nicht gefunden (Arduino-ESP32-Core) / espota.exe not found (Arduino ESP32 core)'; $failed++; continue }
    if (-not (Ask "$($d.Name) ($($d.IP)) per WLAN aktualisieren / update via WiFi?")) { continue }
    Write-Host "Spiele ein / uploading: $($d.IP) ..."
    & $espota.FullName -i $d.IP -p 3232 -f $bin -r
    if ($LASTEXITCODE -ne 0) { Write-Host "  Fehler / error (espota exit code $LASTEXITCODE)"; $failed++; continue }
    $ok = $false
    for ($i = 0; $i -lt 40 -and -not $ok; $i++) {
        Start-Sleep -Seconds 3
        try {
            $req = [System.Net.HttpWebRequest]::Create("http://$($d.IP)/api/topbarStatus")
            $req.Timeout = 3000
            $resp = $req.GetResponse()
            $top = (New-Object System.IO.StreamReader($resp.GetResponseStream())).ReadToEnd()
            $resp.Close()
            if ($top -match [regex]::Escape('"version":"' + $d.Soll + '"')) { $ok = $true }
        } catch {}
    }
    if ($ok) { Write-Host "  $($d.Name) laeuft mit / runs with $($d.Soll)" } else { Write-Host "  $($d.Name) meldet die neue Version nicht / does not report the new version"; $failed++ }
}

# ---- 3. Waagen am USB / scales on USB
Write-Host ''
Write-Host '=== Waagen am USB / scales on USB ==='

# 303A:1001 = eingebautes USB des ESP32-C3 (laufend und im Download-Modus)
# 303A:1001 = built-in USB of the ESP32-C3 (running and in download mode)
$boards = @(Get-CimInstance Win32_PnPEntity | Where-Object { $_.Name -match 'COM(\d+)\)' -and $_.DeviceID -match 'VID_303A&PID_1001' } | ForEach-Object {
    [pscustomobject]@{ Port = [int]([regex]::Match($_.Name, 'COM(\d+)\)').Groups[1].Value) }
})
if ($boards.Count -eq 0) { Write-Host 'Keine Waage am USB. / No scale on USB.' }
foreach ($b in $boards) {
    if ($CheckOnly) { Write-Host "COM$($b.Port): ESP32-C3"; continue }
    if (-not (Ask "COM$($b.Port) (ESP32-C3) mit dem Build aus build_waage flashen / flash with the build from build_waage?")) { continue }
    $args2 = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "$root\build_waage\waagesetup.ps1", '-Port', $b.Port)
    if (-not $Yes) { $args2 += '-AskWifi' }
    & powershell @args2
    if ($LASTEXITCODE -ne 0) { Write-Host "  Flashen fehlgeschlagen / flashing failed (COM$($b.Port))"; $failed++ }
}

Write-Host ''
if ($failed) { Write-Host "$failed Fehler / error(s)."; exit 1 }
Write-Host 'Fertig. / Done.'
