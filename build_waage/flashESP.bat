@echo off
rem SPDX-License-Identifier: GPL-3.0-only (siehe / see LICENSE)
rem Flasht die Waage (ESP32-C3). Doppelklick: sucht die Waage, fragt das WLAN ab, flasht sie und sendet das WLAN
rem per USB. Die Dateien liegen im Unterordner esp32c3 (von buildAll.bat); fehlen sie, wird das Paket vom neuesten GitHub-Release
rem geladen. Enter bei der WLAN-Frage = WLAN nicht aendern.
rem Aufruf mit Parameter ohne Rueckfragen: flashESP.bat 3  (fuer COM3, das WLAN wird dann nicht abgefragt).
rem Flashes the scale (ESP32-C3). Double-click: finds the scale, asks for the WiFi, flashes it and sends the WiFi via USB.
rem The files are in the subfolder esp32c3 (from buildAll.bat); if they are missing, the package is downloaded from the newest
rem GitHub release. Enter at the WiFi question = leave the WiFi unchanged.
rem Usage with a parameter without questions: flashESP.bat 3  (for COM3, the WiFi is then not asked for).
setlocal
cd /d "%~dp0"
if not exist "%~dp0waagesetup.ps1" (
    echo waagesetup.ps1 fehlt. / waagesetup.ps1 is missing.
    pause
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0waagesetup.ps1" -Port "%~1"
if not defined NOPAUSE (
    echo.
    set "ENTER="
    set /p "ENTER=Eingabetaste druecken zum Beenden / Press Enter to close . . . "
)
