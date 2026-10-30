# Changelog

## 1.1

**Deutsch**

- Ordner `build_waage` wie bei der Uhr: `flashESP.bat`, `waagesetup.ps1` und `publish_builds.ps1` liegen dort, die vier Flash-Dateien in `build_waage/esp32c3` sind im Repository (Download ZIP ist flashbar).
- `flashESP.bat` nimmt den lokalen Build, sonst das Paket vom neuesten Release (SHA-256 geprueft); fehlt `esptool.exe`, holt es das Skript aus dem Werkzeug-Zip.
- Release mit `waage-esp32c3.zip`, `waage_flash.zip` (mit `esptool.exe`, Lizenztext und Hinweis) und `merged.bin`; `build.bat` und `buildAll.bat` bauen ohne Benutzernamen im Binary.

**English**

- Folder `build_waage` as with the clock: `flashESP.bat`, `waagesetup.ps1` and `publish_builds.ps1` are there, the four flash files in `build_waage/esp32c3` are in the repository (Download ZIP can be flashed).
- `flashESP.bat` uses the local build, otherwise the package from the newest release (SHA-256 checked); if `esptool.exe` is missing, the script fetches it from the tools zip.
- Release with `waage-esp32c3.zip`, `waage_flash.zip` (with `esptool.exe`, license text and note) and `merged.bin`; `build.bat` and `buildAll.bat` build without a user name in the binary.

## 1.0

**Deutsch**

- Erste Veröffentlichung: Waage mit ESP32-C3 SuperMini, HX711 und vier Wägezellen; Messwerte, Status und Befehle per MQTT.
- Weboberfläche mit Tabs für Waage (Live-Gewicht, Tara, Kalibrierung), Messung, MQTT senden und empfangen, WLAN mit Hostname, System, Status, Log, Info und Doku (mit Verkabelungsplan).
- Einstellungen exportieren und importieren, MQTT-Broker per `/api/mqtt` setzen, Sicherheitsabfragen bei Tara und Kalibrierung.
- Hinweis bei fehlendem HX711 oder fehlender Wägezelle, MQTT-Status `hx711`, Offline-Anzeige der Seite.
- Update per WLAN (ArduinoOTA), Einrichtung des WLANs per USB, Skripte `buildAll.bat`, `checkDevices.ps1`, `flashAll.bat`, `flashESP.bat`.

**English**

- First release: scale with ESP32-C3 SuperMini, HX711 and four load cells; readings, status and commands via MQTT.
- Web interface with tabs for scale (live weight, tare, calibration), measurement, MQTT send and receive, WiFi with host name, system, status, log, info and docs (with wiring diagram).
- Export and import of settings, MQTT broker set via `/api/mqtt`, confirmation dialogs for tare and calibration.
- Notice for a missing HX711 or load cell, MQTT status `hx711`, offline indicator on the page.
- Update over WiFi (ArduinoOTA), WiFi setup via USB, scripts `buildAll.bat`, `checkDevices.ps1`, `flashAll.bat`, `flashESP.bat`.
