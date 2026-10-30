@echo off
rem SPDX-License-Identifier: GPL-3.0-only (siehe / see LICENSE)
rem Baut die Waage-Firmware mit arduino-cli (der Arduino IDE) fuer den ESP32-C3.
rem   buildAll.bat              baut den ESP32-C3 (einziges Ziel)
rem   buildAll.bat c3           dasselbe, Chip ausdruecklich genannt
rem Nach einem erfolgreichen Build werden die fertigen Dateien nach build_waage\esp32c3 kopiert (fuer flashAll.ps1 und
rem checkDevices.ps1) (auch fuer flashESP.bat). Der Zwischenstand liegt in %TEMP%\waage-build\[chip]
rem (spaetere Laeufe sind schneller), das Protokoll in %TEMP%\waage-build\[chip].log. Chips werden nacheinander gebaut:
rem parallele Laeufe teilen sich Zwischenordner im Sketch-Cache und stoeren einander.
rem Board: ESP32C3 Dev Module, USB CDC On Boot = Enabled, 80 MHz, QIO, 4 MB, Partitionsschema "Minimal SPIFFS (1.9MB APP
rem with OTA/128KB SPIFFS)" - mit OTA, damit Updates ueber das WLAN (ArduinoOTA) moeglich sind.
rem
rem Builds the scale firmware with arduino-cli (from the Arduino IDE) for the ESP32-C3.
rem   buildAll.bat              builds the ESP32-C3 (only target)
rem   buildAll.bat c3           the same, chip named explicitly
rem After a successful build the finished files are copied to build_waage\esp32c3 (for flashAll.ps1 and checkDevices.ps1)
rem (also for flashESP.bat). The intermediate state is in %TEMP%\waage-build\[chip] (later runs are
rem faster), the log in %TEMP%\waage-build\[chip].log. Chips are built one after another: parallel runs share
rem intermediate folders in the sketch cache and disturb each other.
rem Board: ESP32C3 Dev Module, USB CDC On Boot = Enabled, 80 MHz, QIO, 4 MB, partition scheme "Minimal SPIFFS (1.9MB APP
rem with OTA/128KB SPIFFS)" - with OTA, so updates over WiFi (ArduinoOTA) work.
setlocal EnableDelayedExpansion
cd /d "%~dp0"

set "CLI=%LOCALAPPDATA%\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
set "CFG=%USERPROFILE%\.arduinoIDE\arduino-cli.yaml"
set "OUT=%TEMP%\waage-build"

if not exist "%CLI%" (
    echo arduino-cli.exe nicht gefunden / not found: %CLI%
    echo Arduino IDE 2 installieren oder den Pfad in dieser Datei anpassen. / Install Arduino IDE 2 or adjust the path in this file.
    exit /b 1
)
if not exist "waage\waage.ino" (
    echo waage\waage.ino nicht gefunden - buildAll.bat gehoert in den Projektordner. / waage\waage.ino not found - buildAll.bat belongs in the project folder.
    exit /b 1
)
if not exist "%OUT%" mkdir "%OUT%"
rem Verkabelungsbild fuer die Doku der Weboberflaeche aus verkabelung.svg erzeugen / create the wiring picture for the doc of the web interface from verkabelung.svg
if exist "makeWiringHeader.ps1" powershell -NoProfile -ExecutionPolicy Bypass -File "makeWiringHeader.ps1" >nul

set "WANT="
for %%A in (%*) do set "WANT=!WANT! %%A"
if "!WANT!"=="" set "WANT=c3"

set "RESULT="
set "FAILED=0"
for %%C in (!WANT!) do (
    call :build %%C
    if errorlevel 1 set "FAILED=1"
)

echo.
echo ===== Ergebnis / Result =====
echo !RESULT!
if not defined NOPAUSE (
    echo.
    set "ENTER="
    set /p "ENTER=Eingabetaste druecken zum Beenden / Press Enter to close . . . "
)
exit /b !FAILED!


:build
set "CHIP=%~1"
set "FQBN="
set "PROP="
set "DIR="
rem -ffile-prefix-map entfernt den Benutzerordner aus den Pfaden im Binary (__FILE__), damit die Builds keinen Benutzernamen enthalten
rem -ffile-prefix-map removes the user folder from the paths in the binary (__FILE__) so the builds contain no user name
rem uhr4.cpp_flags.esp32c3 leer setzen: Platzhalter aus der platform.local.txt des Cores, sonst Linkerfehler beim C3
rem set uhr4.cpp_flags.esp32c3 empty: placeholder from the core platform.local.txt, otherwise a linker error on the C3
if /i "%CHIP%"=="c3" (
    set "DIR=esp32c3"
    set "FQBN=esp32:esp32:esp32c3:CDCOnBoot=cdc,CPUFreq=80,FlashMode=qio,FlashSize=4M,PartitionScheme=min_spiffs,DebugLevel=none,UploadSpeed=921600"
    set "PROP=--build-property uhr4.cpp_flags.esp32c3= --build-property compiler.c.extra_flags=-ffile-prefix-map=%USERPROFILE%= --build-property compiler.cpp.extra_flags=-ffile-prefix-map=%USERPROFILE%="
)
if not defined FQBN (
    echo Unbekannter Chip / unknown chip: %CHIP% ^(c3^)
    set "RESULT=!RESULT! %CHIP%=unbekannt/unknown"
    exit /b 1
)

echo.
echo [%TIME:~0,8%] Baue / building %CHIP% ... ^(Protokoll / log: %OUT%\%CHIP%.log^)
if exist "%OUT%\%CHIP%\waage.ino.bin" del /q "%OUT%\%CHIP%\waage.ino.bin"
"%CLI%" compile --config-file "%CFG%" --fqbn "%FQBN%" %PROP% --build-path "%OUT%\%CHIP%" "%~dp0waage" > "%OUT%\%CHIP%.log" 2>&1
set "BUILDERR=!errorlevel!"
if not exist "%OUT%\%CHIP%\waage.ino.bin" set "BUILDERR=1"
if not "!BUILDERR!"=="0" (
    echo [%TIME:~0,8%] FEHLER / ERROR %CHIP%:
    findstr /i /c:"error" "%OUT%\%CHIP%.log"
    set "RESULT=!RESULT! %CHIP%=FEHLER/ERROR"
    exit /b 1
)
for %%F in ("%OUT%\%CHIP%\waage.ino.bin") do echo [%TIME:~0,8%] %CHIP% fertig / done: %%~zF Bytes, %%~tF
set "RESULT=!RESULT! %CHIP%=OK"

rem boot_app0.bin (Startauswahl der OTA-Partitionen) aus dem Core / boot_app0.bin (boot selection of the OTA partitions) from the core
set "BOOTAPP="
for /d %%D in ("%LOCALAPPDATA%\Arduino15\packages\esp32\hardware\esp32\*") do (
    if exist "%%D\tools\partitions\boot_app0.bin" set "BOOTAPP=%%D\tools\partitions\boot_app0.bin"
)

for %%T in ("build_waage\%DIR%") do (
    if not exist "%%~T" mkdir "%%~T"
    for %%F in (waage.ino.bin waage.ino.bootloader.bin waage.ino.partitions.bin waage.ino.merged.bin) do copy /y "%OUT%\%CHIP%\%%F" "%%~T\" >nul
    if defined BOOTAPP copy /y "!BOOTAPP!" "%%~T\boot_app0.bin" >nul
)
echo %CHIP%: nach build_waage\%DIR% kopiert / copied to build_waage\%DIR%
exit /b 0
