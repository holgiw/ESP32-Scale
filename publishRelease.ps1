# SPDX-License-Identifier: GPL-3.0-only (siehe / see LICENSE)
# Packt den Build aus build_waage\esp32c3 zu waage-esp32c3.zip (Ordner build_waage mit flashESP.bat, waagesetup.ps1, esptool.exe,
# readme.txt und der Firmware im Unterordner flash, dazu LICENSE und THIRD_PARTY_LICENSES.md) samt Pruefsumme. Mit -Upload landen
# die Dateien beim GitHub-Release -Tag (gh). esptool.exe liegt nicht im Repository, es kommt aus dem Projektordner oder dem Arduino-Core.
#   .\publishRelease.ps1                  Paket nach %TEMP%\waage-release erzeugen
#   .\publishRelease.ps1 -Upload -Tag v1.0   zusaetzlich beim Release v1.0 hochladen (ersetzt vorhandene Dateien)
#
# Packs the build from build_waage\esp32c3 into waage-esp32c3.zip (folder build_waage with flashESP.bat, waagesetup.ps1, esptool.exe,
# readme.txt and the firmware in the subfolder flash, plus LICENSE and THIRD_PARTY_LICENSES.md) with a checksum. With -Upload the
# files go to the GitHub release -Tag (gh). esptool.exe is not in the repository, it comes from the project folder or the Arduino core.
param(
    [switch]$Upload,
    [string]$Tag = 'v1.0',
    [string]$Repo = 'holgiw/ESP32-Scale',
    [string]$OutDir = (Join-Path $env:TEMP 'waage-release')
)
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
Add-Type -AssemblyName System.IO.Compression.FileSystem

$src = Join-Path $PSScriptRoot 'build_waage\esp32c3'
$firmware = 'waage.ino.bin', 'waage.ino.bootloader.bin', 'waage.ino.partitions.bin', 'boot_app0.bin'
foreach ($f in $firmware) { if (-not (Test-Path (Join-Path $src $f))) { throw "missing: $src\$f (run buildAll.bat first)" } }

# Der Build darf keinen Benutzernamen enthalten (buildAll.bat setzt -ffile-prefix-map) / the build must not contain a user name
$bad = foreach ($f in $firmware) {
    $t = [Text.Encoding]::GetEncoding(28591).GetString([IO.File]::ReadAllBytes((Join-Path $src $f)))
    if ($t.Contains($([Environment]::UserName))) { $f }
}
if ($bad) { throw "user name '$([Environment]::UserName)' found in: $($bad -join ', ')" }

$esp = Join-Path $PSScriptRoot 'esptool.exe'
if (-not (Test-Path $esp)) {
    $core = Get-ChildItem "$env:LOCALAPPDATA\Arduino15\packages\esp32\tools\esptool_py\*\esptool.exe" -ErrorAction SilentlyContinue | Sort-Object FullName | Select-Object -Last 1
    if ($core) { $esp = $core.FullName }
}
if (-not (Test-Path $esp)) { throw 'esptool.exe not found (project folder or Arduino ESP32 core)' }

if (Test-Path $OutDir) { Remove-Item $OutDir -Recurse -Force }
$stage = Join-Path $OutDir 'stage'
$bw = Join-Path $stage 'build_waage'
New-Item -ItemType Directory (Join-Path $bw 'flash') -Force | Out-Null
foreach ($f in $firmware) { Copy-Item (Join-Path $src $f) (Join-Path $bw 'flash') }
Copy-Item flashESP.bat, waagesetup.ps1 $bw
Copy-Item flash_readme.txt (Join-Path $bw 'readme.txt')
Copy-Item $esp (Join-Path $bw 'esptool.exe')
Copy-Item LICENSE, THIRD_PARTY_LICENSES.md $stage

$notice = "esptool.exe gehoert nicht zur Waage. Es ist esptool von Espressif (https://github.com/espressif/esptool), lizenziert unter`nGPL-2.0-or-later; der Quelltext liegt dort. Es wird hier unveraendert nur zum Flashen mitgeliefert. Die Lizenz der Waage`n(GPL-3.0) liegt als LICENSE bei, weitere Lizenzen stehen in THIRD_PARTY_LICENSES.md.`n`nesptool.exe is not part of the scale firmware. It is esptool by Espressif (https://github.com/espressif/esptool), licensed under`nGPL-2.0-or-later; the source code is there. It is shipped here unchanged, only for flashing. The license of the scale`n(GPL-3.0) is included as LICENSE, further licenses are in THIRD_PARTY_LICENSES.md.`n"
[IO.File]::WriteAllText((Join-Path $bw 'esptool-NOTICE.txt'), $notice, (New-Object Text.UTF8Encoding($false)))

$zip = Join-Path $OutDir 'waage-esp32c3.zip'
# Eintraege mit Schraegstrich anlegen (CreateFromDirectory schreibt in Windows PowerShell 5.1 Rueckstriche) / entries with forward slashes
$za = [IO.Compression.ZipFile]::Open($zip, 'Create')
try {
    foreach ($f in Get-ChildItem $stage -Recurse -File) {
        $name = $f.FullName.Substring($stage.Length + 1).Replace('\', '/')
        [void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile($za, $f.FullName, $name, 'Optimal')
    }
}
finally { $za.Dispose() }
Remove-Item $stage -Recurse -Force
$hash = (Get-FileHash $zip -Algorithm SHA256).Hash.ToLower()
$sha = Join-Path $OutDir 'waage-esp32c3.sha256'
[IO.File]::WriteAllText($sha, "$hash  waage-esp32c3.zip`n")
Write-Host ("waage-esp32c3.zip  {0}  {1:N0} KB" -f $hash, ((Get-Item $zip).Length / 1KB))

if ($Upload) {
    gh release upload $Tag $zip $sha --repo $Repo --clobber
    if ($LASTEXITCODE -ne 0) { throw 'gh release upload failed' }
    Write-Host "Uploaded to $Repo $Tag"
}
