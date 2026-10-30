# SPDX-License-Identifier: GPL-3.0-only (siehe / see LICENSE)
# Flasht die Waage (ESP32-C3) per USB und richtet das WLAN ein, Aufruf aus flashESP.bat.
#   Ohne -Port (Doppelklick auf flashESP.bat): sucht die Waage, fragt nach dem WLAN, flasht die Dateien aus dem Ordner
#   esp32c3 neben diesem Skript (von buildAll.bat); fehlt er, laedt das Skript waage-esp32c3.zip vom neuesten GitHub-Release
#   (SHA-256 geprueft). -Download erzwingt das Laden, WAAGE_BUILD_URL ersetzt die Adresse (Test). Danach sendet es das WLAN per USB ("WAAGE WIFI <Name-Hex> <Passwort-Hex>",
#   "WAAGE RESTART", siehe handleSerialLine() in waage.ino). Enter bei der WLAN-Frage = WLAN nicht aendern.
#   Mit -Port ohne Rueckfragen. Das WLAN-Passwort bleibt nur im Speicher dieses Skripts und geht nur per USB an die Waage.
#
# Flashes the scale (ESP32-C3) via USB and sets up the WiFi, called from flashESP.bat.
#   Without -Port (double-click on flashESP.bat): finds the scale, asks for the WiFi, flashes the files from the folder
#   esp32c3 next to this script (from buildAll.bat); if it is missing, the script downloads waage-esp32c3.zip from the newest
#   GitHub release (SHA-256 checked). -Download forces the download, WAAGE_BUILD_URL replaces the address (test). Afterwards it sends the WiFi via USB ("WAAGE WIFI <name hex> <password hex>",
#   "WAAGE RESTART", see handleSerialLine() in waage.ino). Enter at the WiFi question = leave WiFi unchanged.
#   -AskWifi  fragt auch mit -Port nach dem WLAN (flashAll.ps1) / asks for the WiFi even with -Port (flashAll.ps1)
#   With -Port without questions. The WiFi password stays only in this script's memory and only goes to the scale via USB.
param([string]$Port, [switch]$AskWifi, [switch]$Download, [switch]$FetchTools)

function ConvertTo-Hex([string]$text) {
    -join ([System.Text.Encoding]::UTF8.GetBytes($text) | ForEach-Object { $_.ToString('x2') })
}

# Frische WLAN-Suche anstossen: netsh zeigt sonst nur das Ergebnis der letzten
# Suche von Windows (die laeuft z.B. erst beim Oeffnen des WLAN-Menues).
# WlanScan() aus wlanapi.dll, ohne Administratorrechte.
# Trigger a fresh WiFi scan: netsh otherwise only shows the result of Windows'
# last scan (which runs e.g. only when the WiFi menu is opened).
# WlanScan() from wlanapi.dll, without administrator rights.
function Start-WifiScan {
    try {
        Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class UhrWlanScan {
    [DllImport("wlanapi.dll")] static extern int WlanOpenHandle(uint clientVersion, IntPtr reserved, out uint negotiated, out IntPtr handle);
    [DllImport("wlanapi.dll")] static extern int WlanCloseHandle(IntPtr handle, IntPtr reserved);
    [DllImport("wlanapi.dll")] static extern int WlanEnumInterfaces(IntPtr handle, IntPtr reserved, out IntPtr list);
    [DllImport("wlanapi.dll")] static extern int WlanScan(IntPtr handle, ref Guid iface, IntPtr ssid, IntPtr ie, IntPtr reserved);
    [DllImport("wlanapi.dll")] static extern void WlanFreeMemory(IntPtr memory);
    public static int Scan() {
        uint neg; IntPtr h;
        if (WlanOpenHandle(2, IntPtr.Zero, out neg, out h) != 0) return 0;
        int started = 0;
        IntPtr list;
        if (WlanEnumInterfaces(h, IntPtr.Zero, out list) == 0) {
            // WLAN_INTERFACE_INFO_LIST: 2x DWORD, dann je Eintrag GUID + WCHAR[256] + State = 532 Byte
            int count = Marshal.ReadInt32(list);
            for (int i = 0; i < count; i++) {
                IntPtr entry = new IntPtr(list.ToInt64() + 8 + i * 532);
                Guid g = (Guid)Marshal.PtrToStructure(entry, typeof(Guid));
                if (WlanScan(h, ref g, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero) == 0) started++;
            }
            WlanFreeMemory(list);
        }
        WlanCloseHandle(h, IntPtr.Zero);
        return started;
    }
}
"@
        if ([UhrWlanScan]::Scan() -gt 0) {
            Write-Host 'Suche WLANs ... / scanning for WiFi networks ...'
            Start-Sleep -Seconds 4
        }
    } catch {}
}

# Sichtbare 2,4-GHz-WLANs (Kanal 1-14 bei mindestens einem Zugangspunkt), nach
# Signalstaerke sortiert. netsh-Beschriftung deutsch ("Kanal") oder englisch
# ("Channel"); "SSID"/"Signal" heissen in beiden Sprachen gleich.
# Visible 2.4 GHz WiFis (channel 1-14 on at least one access point), sorted
# by signal strength. netsh labels German ("Kanal") or English ("Channel");
# "SSID"/"Signal" are the same in both languages.
function Get-WifiNetworks {
    $nets = @{}
    $ssid = $null
    $signal = 0
    try {
        foreach ($l in (netsh wlan show networks mode=bssid 2>$null)) {
            if ($l -match '^SSID \d+\s*:\s*(.*)$') {
                $ssid = $Matches[1].Trim()
                if ($ssid -and -not $nets.ContainsKey($ssid)) { $nets[$ssid] = -1 }
            }
            elseif ($ssid -and $l -match '^\s+Signal\s*:\s*(\d+)') { $signal = [int]$Matches[1] }
            elseif ($ssid -and $l -match '^\s+(Kanal|Channel)\s*:\s*(\d+)\s*$') {
                if ([int]$Matches[2] -le 14 -and $signal -gt $nets[$ssid]) { $nets[$ssid] = $signal }
            }
        }
    } catch {}
    return @($nets.Keys | Where-Object { $nets[$_] -ge 0 } | Sort-Object { -$nets[$_] })
}
function Get-CurrentWifi {
    try {
        foreach ($l in (netsh wlan show interfaces 2>$null)) {
            if ($l -match '^\s+SSID\s*:\s*(.+)$') { return $Matches[1].Trim() }
        }
    } catch {}
    return ''
}

# Gespeichertes WLAN-Passwort des PCs fuer ein Netz (netsh ... key=clear, ohne
# Administratorrechte fuer eigene Profile), leer wenn keins/nicht lesbar.
# Stored WiFi password of the PC for a network (netsh ... key=clear, without
# administrator rights for own profiles), empty if none/not readable.
function Get-WifiProfileKey([string]$ssid) {
    try {
        foreach ($l in (netsh wlan show profile "name=$ssid" key=clear 2>$null)) {
            if ($l -match '^\s*(Schl.sselinhalt|Key Content)\s*:\s*(.*)$') { return $Matches[2].Trim() }
        }
    } catch {}
    return ''
}

# Fragt das WLAN fuer die Uhr ab, Ergebnis in $script:wifiSsid/$script:wifiPass
# Asks for the WiFi for the clock, result in $script:wifiSsid/$script:wifiPass
$wifiSsid = ''
$wifiPass = ''
function Read-Wifi {
    Write-Host ''
    Write-Host 'WLAN fuer die Uhr einrichten? Die Uhr kann nur 2,4-GHz-Netze, nur diese werden gezeigt.'
    Write-Host 'Set up WiFi for the clock? The clock only supports 2.4 GHz networks, only those are shown.'
    Start-WifiScan
    $nets = @(Get-WifiNetworks)
    $current = Get-CurrentWifi
    for ($i = 0; $i -lt $nets.Count; $i++) {
        $mark = if ($nets[$i] -eq $current) { '  (PC ist verbunden / PC connected)' } else { '' }
        Write-Host ('  {0} = {1}{2}' -f ($i + 1), $nets[$i], $mark)
    }

    # PC haengt an einem 2,4-GHz-Netz: SSID und gespeichertes Passwort anbieten
    # PC is on a 2.4 GHz network: offer its SSID and stored password
    $answer = ''
    if ($current -and ($nets -contains $current)) {
        $take = Read-Host "Der PC ist mit '$current' (2,4 GHz) verbunden - WLAN-Name und Passwort uebernehmen? / PC is connected to '$current' (2.4 GHz) - take over WiFi name and password? [J/n, Y/n]"
        if (-not $take -or $take -match '^[JjYy]') {
            $key = Get-WifiProfileKey $current
            if ($key -and $key.Length -ge 8 -and $key.Length -le 63) {
                $script:wifiSsid = $current
                $script:wifiPass = $key
                Write-Host "WLAN '$current' mit dem gespeicherten Passwort des PCs uebernommen / WiFi '$current' taken over with the PC's stored password."
                return
            }
            Write-Host 'Kein gespeichertes Passwort lesbar - bitte eingeben / no stored password readable - please enter it.'
            $answer = $current
        }
    }

    if (-not $answer) {
        if ($nets.Count -eq 0) {
            # Keine Liste (kein WLAN-Adapter am PC, WLAN aus oder keine Rechte): Name eintippen
            # No list (no WiFi adapter on the PC, WiFi off or no permission): type the name
            Write-Host '  Keine WLAN-Liste verfuegbar - WLAN-Name bitte eintippen oder Enter = ueberspringen'
            Write-Host '  No WiFi list available - please type the WiFi name or Enter = skip'
        } else {
            Write-Host '  Nummer, WLAN-Name eingeben oder Enter = ueberspringen (Einrichtung spaeter per WPS/Access Point)'
            Write-Host '  number, WiFi name or Enter = skip (set up later via WPS/access point)'
        }
        $answer = Read-Host 'WLAN / WiFi'
    }
    if (-not $answer) { return }

    $ssid = if ($answer -match '^\d+$' -and [int]$answer -ge 1 -and [int]$answer -le $nets.Count) { $nets[[int]$answer - 1] } else { $answer }
    while ($true) {
        $p1 = Read-Host "Passwort fuer / password for '$ssid' (Enter = offenes Netz / open network)" -AsSecureString
        $p2 = Read-Host 'Passwort wiederholen / repeat password' -AsSecureString
        $t1 = [Runtime.InteropServices.Marshal]::PtrToStringUni([Runtime.InteropServices.Marshal]::SecureStringToBSTR($p1))
        $t2 = [Runtime.InteropServices.Marshal]::PtrToStringUni([Runtime.InteropServices.Marshal]::SecureStringToBSTR($p2))
        if ($t1 -ne $t2) { Write-Host 'Die Passwoerter stimmen nicht ueberein / passwords do not match'; continue }
        if ($t1.Length -gt 0 -and ($t1.Length -lt 8 -or $t1.Length -gt 63)) { Write-Host 'WLAN-Passwoerter haben 8-63 Zeichen / WiFi passwords have 8-63 characters'; continue }
        $script:wifiSsid = $ssid
        $script:wifiPass = $t1
        return
    }
}

# Waage am COM-Port: USB-Serial-JTAG des ESP32-C3 (303A:1001), laufend und im Download-Modus
# Scale on the COM port: USB serial JTAG of the ESP32-C3 (303A:1001), running and in download mode
function Get-ScalePorts {
    Get-CimInstance Win32_PnPEntity | Where-Object { $_.Name -match '\(COM(\d+)\)' -and $_.DeviceID -match 'VID_303A&PID_1001' } |
        ForEach-Object { [int]([regex]::Match($_.Name, '\(COM(\d+)\)').Groups[1].Value) } | Sort-Object
}

# DTR/RTS aus: ein Umschalten setzt den Chip sonst zurueck
# DTR/RTS off: switching would otherwise reset the chip
function Open-ScalePort([int]$num) {
    $sp = New-Object System.IO.Ports.SerialPort ("COM$num", 115200)
    $sp.DtrEnable = $false
    $sp.RtsEnable = $false
    $sp.ReadTimeout = 500
    $sp.WriteTimeout = 1000
    $sp.NewLine = "`n"
    $sp.Open()
    return $sp
}

# Sendet einen Befehl und wartet bis 3 s auf "WAAGE OK ..." bzw. "WAAGE ERROR ..."
# Sends a command and waits up to 3 s for "WAAGE OK ..." or "WAAGE ERROR ..."
function Send-Command($sp, [string]$cmd) {
    $sp.Write("$cmd`n")
    $until = (Get-Date).AddSeconds(3)
    while ((Get-Date) -lt $until) {
        try { $line = $sp.ReadLine() } catch { continue }
        if ($line -cmatch '(WAAGE (OK|ERROR).*)') { return $Matches[1].Trim() }
    }
    return ''
}

# ---------------------------------------------------------------------------
# Ablauf / flow
$interactive = -not $Port
$buildUrl = if ($env:WAAGE_BUILD_URL) { $env:WAAGE_BUILD_URL.TrimEnd('/') } else { 'https://github.com/holgiw/ESP32-Scale/releases/latest/download' }

function New-WebClient {
    [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
    $wc = New-Object System.Net.WebClient
    $wc.Headers.Add('User-Agent', 'waage-flash')
    $wc.Proxy = [Net.WebRequest]::GetSystemWebProxy()
    $wc.Proxy.Credentials = [Net.CredentialCache]::DefaultCredentials
    return $wc
}

# esptool.exe: neben diesem Skript, sonst das aus dem Arduino-ESP32-Core (Arduino IDE)
# esptool.exe: next to this script, otherwise the one from the Arduino ESP32 core (Arduino IDE)
function Find-Esptool {
    $local = Join-Path $PSScriptRoot 'esptool.exe'
    if (Test-Path $local) { return $local }
    $core = Get-ChildItem "$env:LOCALAPPDATA\Arduino15\packages\esp32\tools\esptool_py\*\esptool.exe" -ErrorAction SilentlyContinue | Sort-Object FullName | Select-Object -Last 1
    if ($core) { return $core.FullName }
    return $null
}

# Fehlt esptool.exe (zum Beispiel nach "Download ZIP" des Repositorys, ohne Arduino IDE), wird es aus waage_flash.zip des neuesten
# Releases geholt (SHA-256 geprueft) und neben dieses Skript gelegt; -FetchTools erzwingt das (Test).
# If esptool.exe is missing (for example after "Download ZIP" of the repository, without the Arduino IDE), it is fetched from
# waage_flash.zip of the newest release (SHA-256 checked) and put next to this script; -FetchTools forces it (test).
function Get-EsptoolFromRelease {
    $name = 'waage_flash'
    $work = Join-Path ([IO.Path]::GetTempPath()) "waage-tools$([IO.Path]::DirectorySeparatorChar)$name"
    if (Test-Path $work) { Remove-Item $work -Recurse -Force -ErrorAction SilentlyContinue }
    New-Item -ItemType Directory -Path $work -Force | Out-Null
    $zip = Join-Path $work "$name.zip"
    Write-Host "esptool.exe fehlt - wird geladen (rund 7 MB) / esptool.exe is missing - downloading it (about 7 MB): $buildUrl/$name.zip"
    try {
        $wc = New-WebClient
        $shaText = $wc.DownloadString("$buildUrl/$name.sha256")
        $wc.DownloadFile("$buildUrl/$name.zip", $zip)
    }
    catch {
        Write-Host "Download fehlgeschlagen / download failed: $($_.Exception.Message)"
        return $null
    }
    $want = ($shaText.Trim() -split '\s+')[0].ToLower()
    if ((Get-FileHash -Algorithm SHA256 $zip).Hash.ToLower() -ne $want) {
        Write-Host 'Pruefsumme von waage_flash.zip stimmt nicht. / Checksum of waage_flash.zip does not match.'
        return $null
    }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive = [System.IO.Compression.ZipFile]::OpenRead($zip)
    try {
        $entry = $archive.Entries | Where-Object { $_.Name -eq 'esptool.exe' } | Select-Object -First 1
        if (-not $entry) { return $null }
        $target = Join-Path $PSScriptRoot 'esptool.exe'
        try { [System.IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $target, $true) }
        catch {
            $target = Join-Path $work 'esptool.exe'
            [System.IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $target, $true)
        }
        return $target
    }
    finally { $archive.Dispose() }
}

$esptool = if ($FetchTools) { $null } else { Find-Esptool }
if (-not $esptool) { $esptool = Get-EsptoolFromRelease }
if (-not $esptool) {
    Write-Host 'esptool.exe nicht gefunden und nicht ladbar - neben dieses Skript legen (waage_flash.zip) oder den ESP32-Core in der Arduino IDE installieren.'
    Write-Host 'esptool.exe not found and not downloadable - put it next to this script (waage_flash.zip) or install the ESP32 core in the Arduino IDE.'
    exit 1
}

# Ordner esp32c3 neben dem Skript (buildAll.bat), sonst Download des Pakets vom neuesten GitHub-Release, per SHA-256 geprueft.
# Folder esp32c3 next to the script (buildAll.bat), otherwise download of the package from the newest GitHub release, checked by SHA-256.
$files = @('waage.ino.bootloader.bin', 'waage.ino.partitions.bin', 'boot_app0.bin', 'waage.ino.bin')

function Get-BuildDir {
    $name = 'waage-esp32c3'
    $work = Join-Path ([IO.Path]::GetTempPath()) "waage-flash$([IO.Path]::DirectorySeparatorChar)$name"
    if (Test-Path $work) { Remove-Item $work -Recurse -Force -ErrorAction SilentlyContinue }
    New-Item -ItemType Directory -Path $work -Force | Out-Null
    $zip = Join-Path $work "$name.zip"
    Write-Host "Build wird geladen / downloading the build: $buildUrl/$name.zip"
    try {
        $wc = New-WebClient
        $shaText = $wc.DownloadString("$buildUrl/$name.sha256")
        $wc.DownloadFile("$buildUrl/$name.zip", $zip)
    }
    catch {
        Write-Host "Download fehlgeschlagen / download failed: $($_.Exception.Message)"
        Write-Host 'Internetverbindung des PCs pruefen (GitHub erreichbar?), dann erneut starten.'
        Write-Host 'Check the PC internet connection (is GitHub reachable?), then start again.'
        exit 1
    }
    $want = ($shaText.Trim() -split '\s+')[0].ToLower()
    $have = (Get-FileHash -Algorithm SHA256 $zip).Hash.ToLower()
    if ($want -ne $have) {
        Write-Host 'Pruefsumme der geladenen Datei stimmt nicht - nichts wird geflasht. Spaeter erneut versuchen.'
        Write-Host 'Checksum of the downloaded file does not match - nothing is flashed. Try again later.'
        exit 1
    }
    $out = Join-Path $work 'files'
    Expand-Archive -Path $zip -DestinationPath $out -Force
    $lack = $files | Where-Object { -not (Test-Path (Join-Path $out $_)) }
    if ($lack) {
        Write-Host "Das Paket ist unvollstaendig / the package is incomplete: $($lack -join ', ')"
        exit 1
    }
    return $out
}

$binDir = Join-Path $PSScriptRoot 'esp32c3'
$missing = @($files | Where-Object { -not (Test-Path (Join-Path $binDir $_)) })
if ($Download -or $env:WAAGE_DOWNLOAD -eq '1' -or $missing.Count -gt 0) {
    if ($missing.Count -gt 0 -and -not $Download) {
        Write-Host 'Kein lokaler Build (esp32c3) - das Paket wird vom GitHub-Release geladen.'
        Write-Host 'No local build (esp32c3) - the package is downloaded from the GitHub release.'
    }
    $binDir = Get-BuildDir
    Write-Host 'Build vom GitHub-Release / build from the GitHub release.'
} else {
    Write-Host "Build aus dem lokalen Ordner / build from the local folder: $binDir"
}
# 1) Waage suchen / find the scale
$ports = @(Get-ScalePorts)
if ($Port) { $num = [int]($Port -replace '\D', '') }
elseif ($ports.Count -eq 1) { $num = $ports[0]; Write-Host "Waage auf / scale on COM$num" }
else {
    Write-Host 'COM-Schnittstellen / COM ports:'
    Get-CimInstance Win32_PnPEntity | Where-Object { $_.Name -match '\(COM\d+\)' } | ForEach-Object { Write-Host "  $($_.Name)" }
    if ($ports.Count -gt 1) {
        Write-Host 'Mehrere ESP32 gefunden - bitte nur einen anschliessen oder die Nummer angeben.'
        Write-Host 'Several ESP32 found - connect only one or enter the number.'
    }
    else {
        Write-Host 'Kein ESP32-C3 erkannt. Datenkabel? Neuer Chip: Boot-Taste halten, USB einstecken, erneut starten.'
        Write-Host 'No ESP32-C3 detected. Data cable? New chip: hold Boot, plug in USB, start again.'
    }
    $a = Read-Host 'COM-Port Nummer / number (z.B./e.g. 3 = COM3)'
    if (-not $a) { exit 1 }
    $num = [int]($a -replace '\D', '')
}

# 2) WLAN abfragen / ask for the WiFi
if ($interactive -or $AskWifi) { Read-Wifi }

# 3) Flashen / flash
Write-Host ''
Write-Host "Flashe / flashing COM$num ..."
Push-Location $binDir
$global:LASTEXITCODE = 1
try {
    & $esptool --chip esp32c3 --port "COM$num" --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode keep --flash_freq keep --flash_size keep 0x0 waage.ino.bootloader.bin 0x8000 waage.ino.partitions.bin 0xe000 boot_app0.bin 0x10000 waage.ino.bin
}
catch { Write-Host "esptool.exe: $($_.Exception.Message)" }
$flashOk = ($LASTEXITCODE -eq 0)
Pop-Location
if (-not $flashOk) {
    $wifiPass = $null
    Write-Host ''
    Write-Host 'Flashen fehlgeschlagen - siehe Meldung von esptool. Haeufige Ursachen:'
    Write-Host '  * Boot-Taste (GPIO9) halten, erst DANACH USB einstecken, dann erneut starten.'
    Write-Host '  * Port belegt: seriellen Monitor schliessen.  * Nur Ladekabel?  * esptool.exe vom Virenscanner blockiert.'
    Write-Host 'Flashing failed - see the esptool message. Common causes:'
    Write-Host '  * Hold the Boot button (GPIO9), only THEN plug in USB, then start again.'
    Write-Host '  * Port in use: close the serial monitor.  * Charging-only cable?  * esptool.exe blocked by the virus scanner.'
    exit 1
}

# 4) WLAN senden / send the WiFi
$result = 0
if ($wifiSsid) {
    Write-Host ''
    Write-Host 'WLAN wird an die Waage gesendet - warte auf den Start ...'
    Write-Host 'Sending the WiFi to the scale - waiting for it to start ...'
    $result = 1
    $start = Get-Date
    $hinted = $false
    while (((Get-Date) - $start).TotalSeconds -lt 90) {
        $n = (Get-ScalePorts | Select-Object -First 1)
        if (-not $n) {
            if (-not $hinted -and ((Get-Date) - $start).TotalSeconds -gt 20) {
                Write-Host 'Startet die Waage nicht von selbst? Einmal die Reset-Taste druecken.'
                Write-Host 'Does the scale not start by itself? Press the Reset button once.'
                $hinted = $true
            }
            Start-Sleep -Milliseconds 500
            continue
        }
        $sp = $null
        try {
            $sp = Open-ScalePort $n
            $reply = Send-Command $sp ("WAAGE WIFI {0} {1}" -f (ConvertTo-Hex $wifiSsid), (ConvertTo-Hex $wifiPass))
            if (-not $reply) { throw 'no reply' }
            if ($reply -notmatch '^WAAGE OK') { Write-Host "Die Waage meldet einen Fehler / the scale reports an error: $reply"; break }
            Write-Host "WLAN '$wifiSsid' gespeichert / WiFi '$wifiSsid' saved."
            [void](Send-Command $sp 'WAAGE RESTART')
            $sp.Close()
            Write-Host 'Die Waage startet neu und verbindet sich mit dem WLAN. Adresse: Router-Liste oder http://waage-xxxxxx.local/'
            Write-Host 'The scale restarts and connects to the WiFi. Address: router list or http://waage-xxxxxx.local/'
            $result = 0
            break
        } catch {
        } finally {
            if ($sp -and $sp.IsOpen) { try { $sp.Close() } catch {} }
        }
        Start-Sleep -Milliseconds 500
    }
    $wifiPass = $null
    if ($result -ne 0) {
        Write-Host 'Keine Antwort von der Waage. WLAN dann ueber den Zugangspunkt "Waage-xxxxxx" (http://192.168.4.1/) eintragen.'
        Write-Host 'No reply from the scale. Set the WiFi via the access point "Waage-xxxxxx" (http://192.168.4.1/) then.'
    }
}
exit $result
