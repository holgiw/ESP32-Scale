@echo off
rem SPDX-License-Identifier: GPL-3.0-only (siehe / see LICENSE)
rem Baut die Waage-Firmware mit arduino-cli (der Arduino IDE) fuer den ESP32-C3.
rem Board-Einstellungen: ESP32C3 Dev Module, USB CDC On Boot = Enabled, CPU 80 MHz, Flash QIO 4 MB,
rem Partitionsschema "Minimal SPIFFS (1.9MB APP with OTA/128KB SPIFFS)" - mit OTA, damit Updates ueber das WLAN
rem (ArduinoOTA) moeglich sind. Die fertigen Dateien liegen danach im Ordner flash (fuer flashESP.bat), die
rem Zwischenstaende in %TEMP%\waage-build (spaetere Laeufe sind schneller), das Protokoll in %TEMP%\waage-build.log.
rem   build.bat           bauen
rem   build.bat ota       bauen und danach per WLAN auf die Waage laden (Hostname/IP als zweiter Parameter)
rem                       Beispiel: build.bat ota waage-a1b2c3.local
rem
rem Builds the scale firmware with arduino-cli (from the Arduino IDE) for the ESP32-C3.
rem Board settings: ESP32C3 Dev Module, USB CDC On Boot = Enabled, CPU 80 MHz, flash QIO 4 MB,
rem partition scheme "Minimal SPIFFS (1.9MB APP with OTA/128KB SPIFFS)" - with OTA, so updates over WiFi
rem (ArduinoOTA) work. The finished files end up in the folder flash (for flashESP.bat), the intermediate
rem state in %TEMP%\waage-build (later runs are faster), the log in %TEMP%\waage-build.log.
rem   build.bat           build
rem   build.bat ota       build, then upload over WiFi to the scale (host name/IP as second parameter)
rem                       Example: build.bat ota waage-a1b2c3.local
setlocal
cd /d "%~dp0"

set "CLI=%LOCALAPPDATA%\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
set "CFG=%USERPROFILE%\.arduinoIDE\arduino-cli.yaml"
set "FQBN=esp32:esp32:esp32c3:CDCOnBoot=cdc,CPUFreq=80,FlashMode=qio,FlashSize=4M,PartitionScheme=min_spiffs,DebugLevel=none,UploadSpeed=921600"
set "OUT=%TEMP%\waage-build"
set "LOG=%TEMP%\waage-build.log"

if not exist "%CLI%" (
    echo arduino-cli.exe nicht gefunden / not found: %CLI%
    echo Arduino IDE 2 installieren oder den Pfad in dieser Datei anpassen. / Install Arduino IDE 2 or adjust the path in this file.
    set "RC=1" & goto :ende
)
if not exist "waage\waage.ino" (
    echo waage\waage.ino nicht gefunden - build.bat gehoert neben den Ordner waage. / waage\waage.ino not found - build.bat belongs next to the folder waage.
    set "RC=1" & goto :ende
)
if not exist "%OUT%" mkdir "%OUT%"
if not exist "flash" mkdir "flash"

echo Baue / building: %FQBN%
"%CLI%" compile --config-file "%CFG%" --fqbn "%FQBN%" --build-property "uhr4.cpp_flags.esp32c3=" --build-path "%OUT%" --output-dir "%~dp0flash" "%~dp0waage" > "%LOG%" 2>&1
if errorlevel 1 (
    type "%LOG%"
    echo.
    echo Build fehlgeschlagen / build failed - Protokoll / log: %LOG%
    set "RC=1" & goto :ende
)
findstr /i /c:"Sketch uses" /c:"Global variables" "%LOG%"

rem boot_app0.bin (Startauswahl der OTA-Partitionen) aus dem Core zu den Flash-Dateien kopieren
rem copy boot_app0.bin (boot selection of the OTA partitions) from the core to the flash files
set "BOOTAPP="
for /d %%D in ("%LOCALAPPDATA%\Arduino15\packages\esp32\hardware\esp32\*") do (
    if exist "%%D\tools\partitions\boot_app0.bin" set "BOOTAPP=%%D\tools\partitions\boot_app0.bin"
)
if defined BOOTAPP (
    copy /y "%BOOTAPP%" "flash\boot_app0.bin" >nul
) else (
    echo boot_app0.bin im Arduino-Core nicht gefunden / not found in the Arduino core.
    set "RC=1" & goto :ende
)
echo Fertig / done: %~dp0flash

if /i not "%~1"=="ota" (set "RC=0" & goto :ende)

rem Upload per WLAN (espota.py des Cores, Port 3232, kein Passwort)
rem Upload over WiFi (espota.py of the core, port 3232, no password)
if "%~2"=="" (
    echo OTA: Hostname oder IP der Waage als zweiten Parameter angeben, z. B. build.bat ota waage-a1b2c3.local
    echo OTA: pass the host name or IP of the scale as second parameter, e.g. build.bat ota waage-a1b2c3.local
    set "RC=1" & goto :ende
)
set "ESPOTA="
for /d %%D in ("%LOCALAPPDATA%\Arduino15\packages\esp32\hardware\esp32\*") do (
    if exist "%%D\tools\espota.exe" set "ESPOTA=%%D\tools\espota.exe"
)
if not defined ESPOTA (
    echo espota.exe im Arduino-Core nicht gefunden / not found in the Arduino core.
    set "RC=1" & goto :ende
)
echo Lade per WLAN auf / uploading over WiFi to %~2 ...
"%ESPOTA%" -i %~2 -p 3232 -f "flash\waage.ino.bin" -r
set "RC=%errorlevel%"
:ende
if not defined NOPAUSE (
    echo.
    set "ENTER="
    set /p "ENTER=Eingabetaste druecken zum Beenden / Press Enter to close . . . "
)
exit /b %RC%
