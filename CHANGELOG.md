# Changelog

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
