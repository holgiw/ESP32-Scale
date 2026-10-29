# SPDX-License-Identifier: GPL-3.0-only (siehe / see LICENSE)
# Sucht alle Waagen im Netz und zeigt eine Tabelle: Adresse, Name, Chip, laufende Firmware (Build-Zeit), ob sie dem
# neuesten Build in build_waage entspricht, HX711- und MQTT-Status, Uhrzeit.
#   .\checkDevices.ps1                      sucht im Netz des PCs (die ersten drei Zahlen seiner Adresse, z. B. 192.168.0.x)
#   .\checkDevices.ps1 -Subnet 192.168.1    anderes Netz
#   .\checkDevices.ps1 -Ip 192.168.1.50    nur diese Adressen (mehrere mit Komma)
#   .\checkDevices.ps1 -PassThru            gibt die Waagen als Objekte zurueck statt als Tabelle (fuer flashAll.ps1)
# Der Soll-Stand ist die Build-Zeit, die in der Datei build_waage\esp32c3\waage.ino.bin steht (Kennung WAAGE_FW_BUILD=).
# Aeltere Firmware ohne Version zeigt "?".
#
# Finds all scales on the network and shows a table: address, name, chip, running firmware (build time), whether it
# matches the newest build in build_waage, HX711 and MQTT state, time.
#   .\checkDevices.ps1                      searches the PC's network (the first three numbers of its address, e.g. 192.168.0.x)
#   .\checkDevices.ps1 -Subnet 192.168.1    another network
#   .\checkDevices.ps1 -Ip 192.168.1.50    only these addresses (several with comma)
#   .\checkDevices.ps1 -PassThru            returns the scales as objects instead of a table (for flashAll.ps1)
# The target is the build time stored in the file build_waage\esp32c3\waage.ino.bin (marker WAAGE_FW_BUILD=).
# Older firmware without a version shows "?".

param([string]$Subnet, [string[]]$Ip, [switch]$PassThru)

$ErrorActionPreference = 'SilentlyContinue'

# Mit -File kommt "a,b" als ein Text an - trennen / with -File "a,b" arrives as one string - split it
if ($Ip) { $Ip = @($Ip | ForEach-Object { $_ -split '[,; ]+' } | Where-Object { $_ }) }

# Soll-Build aus der Binaerdatei lesen, "2026-10-19 10:07:51"
# Read the target build from the binary, "2026-10-19 10:07:51"
function Get-TargetBuild {
    $file = Join-Path $PSScriptRoot 'build_waage\esp32c3\waage.ino.bin'
    if (-not (Test-Path $file)) { return $null }
    $text = [System.Text.Encoding]::GetEncoding(28591).GetString([System.IO.File]::ReadAllBytes($file))
    $at = $text.IndexOf('WAAGE_FW_BUILD=')
    if ($at -lt 0) { return $null }
    $end = $text.IndexOf([char]0, $at)
    $stamp = ($text.Substring($at + 15, $end - $at - 15) -replace '\s+', ' ').Trim()
    try { return [datetime]::ParseExact($stamp, 'MMM d yyyy HH:mm:ss', [System.Globalization.CultureInfo]::InvariantCulture).ToString('yyyy-MM-dd HH:mm:ss') }
    catch { return $null }
}
$soll = Get-TargetBuild

# Netz des PCs bestimmen (erste drei Zahlen der Adresse), falls kein -Subnet angegeben ist
# Determine the PC's network (first three numbers of its address) if no -Subnet is given
if (-not $Subnet -and -not $Ip) {
    $route = Get-NetRoute -DestinationPrefix '0.0.0.0/0' | Sort-Object RouteMetric | Select-Object -First 1
    $addr = Get-NetIPAddress -InterfaceIndex $route.InterfaceIndex -AddressFamily IPv4 | Select-Object -First 1
    if (-not $addr) { Write-Host 'Kein Netz gefunden - -Subnet 192.168.0 angeben. / No network found - give -Subnet 192.168.0.'; exit 1 }
    $Subnet = ($addr.IPAddress -split '\.')[0..2] -join '.'
}

# Kandidaten: offene Port-80-Adressen im Netz
# Candidates: addresses with an open port 80 on the network
if (-not $Ip) {
    Write-Host "Suche in / searching $Subnet.1 - $Subnet.254 ..."
    $attempts = 1..254 | ForEach-Object {
        $c = New-Object System.Net.Sockets.TcpClient
        [pscustomobject]@{ Ip = "$Subnet.$_"; Client = $c; Task = $c.ConnectAsync("$Subnet.$_", 80) }
    }
    Start-Sleep -Milliseconds 1500
    $Ip = @($attempts | Where-Object { $_.Task.IsCompleted -and -not $_.Task.IsFaulted -and $_.Client.Connected } | ForEach-Object { $_.Ip })
    $attempts | ForEach-Object { $_.Client.Close() }
}


# Mit Zeitlimit (3 s): ein Geraet mit offenem Port 80, das nicht antwortet, haelt sonst den ganzen Lauf auf
# With a time limit (3 s): a device with an open port 80 that does not answer would otherwise hold up the whole run

function Get-Page([string]$url) {
    try {
        $req = [System.Net.HttpWebRequest]::Create($url)
        $req.Timeout = 3000
        $req.ReadWriteTimeout = 3000
        $resp = $req.GetResponse()
        $reader = New-Object System.IO.StreamReader($resp.GetResponseStream(), [System.Text.Encoding]::UTF8)
        $text = $reader.ReadToEnd()
        $reader.Close(); $resp.Close()
        return $text
    } catch { return $null }
}

$rows = foreach ($ipAddr in $Ip) {
    $page = Get-Page "http://$ipAddr/"
    if (-not $page) { continue }

    # Eine Waage erkennt man an der Kennung WAAGE in der Kopfzeile (kein Sonderzeichen im Muster)
    # A scale is recognized by the WAAGE mark in the header (no special character in the pattern)
    if ($page -notmatch "brand-mark'>WAAGE<") { continue }
    $name = [regex]::Match($page, "class='host-link' href='[^']*'>([^<]*)").Groups[1].Value
    $chip = [regex]::Match($page, 'Chip Model: (ESP32[A-Za-z0-9-]*)').Groups[1].Value -replace '^ESP32-?', ''
    if (-not $chip) { $chip = 'C3' }
    $top = Get-Page "http://$ipAddr/api/topbarStatus"
    $version = [regex]::Match([string]$top, '"version":"([^"]*)"').Groups[1].Value
    $timeOk = [regex]::Match([string]$top, '"time":"([^"]*)"').Groups[1].Value
    $hx = [regex]::Match([string]$top, '"hx":"([^"]*)"').Groups[1].Value
    $mq = [regex]::Match([string]$top, '"mq":"([^"]*)"').Groups[1].Value
    $dt = [regex]::Match([string]$top, '"datetime":"([^"]*)"').Groups[1].Value
    $state = if (-not $soll -or -not $version) { '?' } elseif ($version -eq $soll) { 'aktuell' } elseif ($version -lt $soll) { 'aelter' } else { 'neuer' }
    [pscustomobject]@{
        IP = $ipAddr; Name = $name; Chip = $chip; Firmware = $version; Soll = $soll
        Stand = $state; HX711 = $(if ($hx -eq 'ok') { 'OK' } else { 'FEHLER' })
        MQTT = $(if ($mq -eq 'ok') { 'OK' } elseif ($mq -eq 'na') { '-' } else { 'FEHLER' })
        Zeit = $(if ($timeOk -eq 'ok') { 'OK' } else { 'FEHLT' }); Uhrzeit = $dt
    }
}

if ($PassThru) { return @($rows) }
if (-not $rows) { Write-Host 'Keine Waage gefunden. / No scale found.'; exit 1 }
$rows | Sort-Object IP | Format-Table -AutoSize
Write-Host ('Soll / target: {0}' -f $soll)
