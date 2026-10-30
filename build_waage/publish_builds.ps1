# SPDX-License-Identifier: GPL-3.0-only (siehe / see LICENSE)
# Erzeugt die Release-Pakete aus build_waage\esp32c3 (von buildAll.bat):
#   waage-esp32c3.zip / .sha256   Bootloader, Partitionstabelle, boot_app0.bin, App (flach; die Quelle fuer flashESP.bat im Download-Fall)
#   waage_flash.zip / .sha256     Ordner build_waage mit flashESP.bat, waagesetup.ps1, readme.txt, esptool.exe, Lizenztexten und der Firmware (esp32c3),
#                                 dazu LICENSE und THIRD_PARTY_LICENSES.md
#   waage-<Tag>-esp32c3-merged.bin  komplettes Abbild fuer das Flashen von Hand bei Adresse 0x0 (loescht die Einstellungen)
# Ohne -Upload nur nach -OutDir; mit -Upload [-Tag vX.Y] per gh release upload --clobber (Standard: neuestes Release).
# esptool.exe liegt nicht im Repository (Fremdsoftware, GPL-2.0-or-later): aus build_waage oder dem Arduino-Core ins Werkzeug-Zip, mit Lizenztext.
#
# Creates the release packages from build_waage\esp32c3 (made by buildAll.bat):
#   waage-esp32c3.zip / .sha256   bootloader, partition table, boot_app0.bin, app (flat; the source for flashESP.bat when it downloads)
#   waage_flash.zip / .sha256     folder build_waage with flashESP.bat, waagesetup.ps1, readme.txt, esptool.exe, license texts and the firmware
#                                 (esp32c3), plus LICENSE and THIRD_PARTY_LICENSES.md
#   waage-<tag>-esp32c3-merged.bin  complete image for flashing by hand at address 0x0 (erases the settings)
# Without -Upload only to -OutDir; with -Upload [-Tag vX.Y] via gh release upload --clobber (default: newest release).
# esptool.exe is not in the repository (third-party software, GPL-2.0-or-later): from build_waage or the Arduino core into the tools zip, with its license text.
param(
    [switch]$Upload,
    [string]$Tag,
    [string]$Repo = 'holgiw/ESP32-Scale',
    [string]$OutDir = (Join-Path $env:TEMP 'waage-release')
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$root = Split-Path $PSScriptRoot -Parent
$src = Join-Path $PSScriptRoot 'esp32c3'
$firmware = 'waage.ino.bootloader.bin', 'waage.ino.partitions.bin', 'boot_app0.bin', 'waage.ino.bin'
foreach ($f in $firmware + 'waage.ino.merged.bin') { if (-not (Test-Path (Join-Path $src $f))) { throw "missing: $src\$f (run buildAll.bat first)" } }

# Der Build darf keinen Benutzernamen enthalten (buildAll.bat setzt -ffile-prefix-map) / the build must not contain a user name
$user = [Environment]::UserName
$latin1 = [Text.Encoding]::GetEncoding(28591)
$bad = foreach ($f in $firmware + 'waage.ino.merged.bin') {
    if ($latin1.GetString([IO.File]::ReadAllBytes((Join-Path $src $f))).Contains($user)) { $f }
}
if ($bad) { throw "user name '$user' found in: $($bad -join ', ') (rebuild with buildAll.bat)" }

if (-not $Tag) { $Tag = (& gh release view --repo $Repo --json tagName -q .tagName).Trim() }
if (-not $Tag) { throw 'no release found, give -Tag' }

if (Test-Path $OutDir) { Remove-Item $OutDir -Recurse -Force }
New-Item -ItemType Directory $OutDir | Out-Null

# Zip mit Schraegstrichen in den Eintragsnamen (CreateFromDirectory schreibt in Windows PowerShell 5.1 Rueckstriche)
# Zip with forward slashes in the entry names (CreateFromDirectory writes backslashes in Windows PowerShell 5.1)
function New-Zip([string]$zip, $entries) {
    $za = [IO.Compression.ZipFile]::Open($zip, 'Create')
    try { foreach ($k in ($entries.Keys | Sort-Object)) { [void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile($za, $entries[$k], $k, 'Optimal') } }
    finally { $za.Dispose() }
}
function Write-Sha([string]$zip) {
    $name = Split-Path $zip -Leaf
    $hash = (Get-FileHash $zip -Algorithm SHA256).Hash.ToLower()
    [IO.File]::WriteAllText(($zip -replace '\.zip$', '.sha256'), "$hash  $name`n")
    Write-Host ("{0}  {1}  {2:N0} KB" -f $name, $hash, ((Get-Item $zip).Length / 1KB))
}

# 1) Firmware-Paket / firmware package
$fw = Join-Path $OutDir 'waage-esp32c3.zip'
$e = @{}
foreach ($f in $firmware) { $e[$f] = Join-Path $src $f }
New-Zip $fw $e
Write-Sha $fw

# 2) Werkzeug-Paket / tools package
$esp = Join-Path $PSScriptRoot 'esptool.exe'
if (-not (Test-Path $esp)) {
    $core = Get-ChildItem "$env:LOCALAPPDATA\Arduino15\packages\esp32\tools\esptool_py\*\esptool.exe" -ErrorAction SilentlyContinue | Sort-Object FullName | Select-Object -Last 1
    if ($core) { $esp = $core.FullName }
}
if (-not (Test-Path $esp)) { throw 'esptool.exe not found (build_waage or Arduino ESP32 core)' }
# Lizenztext von esptool (GPL-2.0): neben esptool.exe oder aus einer anderen Version im Core
# esptool license text (GPL-2.0): next to esptool.exe or from another version in the core
$lic = Get-ChildItem "$env:LOCALAPPDATA\Arduino15\packages\esp32\tools\esptool_py\*\LICENSE" -ErrorAction SilentlyContinue | Sort-Object FullName | Select-Object -Last 1
if (Test-Path (Join-Path (Split-Path $esp) 'LICENSE')) { $lic = Get-Item (Join-Path (Split-Path $esp) 'LICENSE') }
if (-not $lic) { throw 'esptool LICENSE text not found (Arduino ESP32 core esptool_py)' }
$notice = Join-Path $OutDir 'esptool-NOTICE.txt'
$text = "esptool.exe gehoert nicht zur Waage. Es ist esptool von Espressif (https://github.com/espressif/esptool), lizenziert unter`nGPL-2.0-or-later (Lizenztext: esptool-LICENSE.txt); der Quelltext liegt dort. Es wird hier unveraendert nur zum Flashen mitgeliefert.`nDie Lizenz der Waage (GPL-3.0) liegt als LICENSE bei, weitere Lizenzen stehen in THIRD_PARTY_LICENSES.md.`n`nesptool.exe is not part of the scale firmware. It is esptool by Espressif (https://github.com/espressif/esptool), licensed under`nGPL-2.0-or-later (license text: esptool-LICENSE.txt); the source code is there. It is shipped here unchanged, only for flashing.`nThe license of the scale (GPL-3.0) is included as LICENSE, further licenses are in THIRD_PARTY_LICENSES.md.`n"
[IO.File]::WriteAllText($notice, $text, (New-Object Text.UTF8Encoding($false)))

$tools = Join-Path $OutDir 'waage_flash.zip'
$e = @{}
$e['LICENSE'] = Join-Path $root 'LICENSE'
$e['THIRD_PARTY_LICENSES.md'] = Join-Path $root 'THIRD_PARTY_LICENSES.md'
foreach ($f in 'flashESP.bat', 'waagesetup.ps1', 'readme.txt') { $e["build_waage/$f"] = Join-Path $PSScriptRoot $f }
$e['build_waage/esptool.exe'] = $esp
$e['build_waage/esptool-LICENSE.txt'] = $lic.FullName
$e['build_waage/esptool-NOTICE.txt'] = $notice
foreach ($f in $firmware) { $e["build_waage/esp32c3/$f"] = Join-Path $src $f }
New-Zip $tools $e
Write-Sha $tools

# 3) Komplettes Abbild / complete image
$merged = Join-Path $OutDir "waage-$Tag-esp32c3-merged.bin"
Copy-Item (Join-Path $src 'waage.ino.merged.bin') $merged
Write-Host ("{0}  {1:N0} KB (esptool.exe: {2})" -f (Split-Path $merged -Leaf), ((Get-Item $merged).Length / 1KB), $esp)

# 4) Hochladen / upload
if ($Upload) {
    Write-Host "Uploading to $Repo $Tag ..."
    gh release upload $Tag $fw ($fw -replace '\.zip$', '.sha256') $tools ($tools -replace '\.zip$', '.sha256') $merged --repo $Repo --clobber
    if ($LASTEXITCODE -ne 0) { throw 'gh release upload failed' }
    Write-Host 'Done'
}
else { Write-Host "Packages in $OutDir (nothing uploaded)" }
