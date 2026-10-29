@echo off
rem SPDX-License-Identifier: GPL-3.0-only (siehe / see LICENSE)
rem Prueft alle Waagen im Netz und bringt veraltete auf den Stand des Builds in build_waage (gebaut mit buildAll.bat):
rem   per WLAN (ArduinoOTA); Waagen am USB werden mit waagesetup.ps1 geflasht. Vor jedem Flash wird gefragt (Enter = Ja).
rem   flashAll.bat               pruefen, dann fragen und flashen
rem   flashAll.bat -CheckOnly    nur pruefen
rem   flashAll.bat -Yes          ohne Rueckfragen
rem   flashAll.bat -Force        auch aktuelle Waagen neu einspielen
rem Checks all scales on the network and brings outdated ones to the state of the build in build_waage (built with
rem buildAll.bat): over WiFi (ArduinoOTA); scales on USB are flashed with waagesetup.ps1. It asks before every flash
rem (Enter = yes).
rem   flashAll.bat               check, then ask and flash
rem   flashAll.bat -CheckOnly    only check
rem   flashAll.bat -Yes          without questions
rem   flashAll.bat -Force        also re-flash up-to-date scales
setlocal
cd /d "%~dp0"
if not exist "%~dp0flashAll.ps1" (
    echo flashAll.ps1 fehlt / is missing.
    pause
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0flashAll.ps1" %*
set "RESULT=%errorlevel%"
if not defined NOPAUSE (
    echo.
    set "ENTER="
    set /p "ENTER=Eingabetaste druecken zum Beenden / Press Enter to close . . . "
)
exit /b %RESULT%
