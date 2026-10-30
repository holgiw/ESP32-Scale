# ESP32-Scale – Waage mit ESP32-C3, HX711 und MQTT

[Deutsch](#deutsch) · [English](#english)

> **Fertige Firmware:** Das Zip von *Code → Download ZIP* enthält die vier Flash-Dateien in uild_waage/esp32c3/; zum Flashen reicht uild_waage/flashESP.bat. Die vollständige merged.bin und das Werkzeug-Zip (waage_flash.zip mit sptool.exe) liegen im neuesten [Release](../../releases) unter *Assets*.
>
> **Ready firmware:** the zip from *Code → Download ZIP* contains the four flash files in uild_waage/esp32c3/; uild_waage/flashESP.bat is enough to flash. The complete merged.bin and the tools zip (waage_flash.zip with sptool.exe) are in the newest [release](../../releases) under *Assets*.

---

## Deutsch

Eine Waage (zum Beispiel unter einem Bett oder als Personenwaage) mit einem ESP32-C3 SuperMini, dem Wägechip HX711 und vier Wägezellen. Die Messwerte gehen per MQTT an einen Broker, eingestellt wird alles über eine Weboberfläche des Geräts. Der HX711 wird ohne Bibliothek direkt angesprochen.

**Funktionen**

- Live-Gewicht, Tara und Kalibrierung in der Weboberfläche (mit Sicherheitsabfrage), Faktor und Offset bleiben nach einem Neustart erhalten.
- MQTT: Gewicht, Rohwert, Stabil-Anzeige, JSON-Status, WLAN-Signal, Laufzeit, IP, Version, Online/Offline und HX711-Status. Jedes Topic ist einzeln abschaltbar und frei benennbar. Befehle zum Tara, Kalibrieren und Neustart per MQTT.
- Weboberfläche mit Tabs: Waage, Messung, MQTT senden, MQTT empfangen, WLAN (mit Hostname), System, Status, Log, Info, Doku (mit Verkabelungsplan). Der Export und Import der Einstellungen liegt oben rechts.
- Hinweise bei fehlendem HX711 oder fehlender Wägezelle, Offline-Anzeige, wenn die Waage nicht antwortet.
- Update per WLAN (ArduinoOTA), WLAN-Einrichtung per USB beim Flashen, Zugangspunkt `Waage-xxxxxx`, wenn kein bekanntes WLAN erreichbar ist.
- Der MQTT-Broker lässt sich per `/api/mqtt` setzen (siehe unten).

### Verdrahtung (C3 SuperMini, rechte Kante)

Der HX711 ist kein SPI-Baustein: Er braucht nur zwei Signalleitungen (DT = Daten, SCK = Takt) plus Versorgung.

Das HX711-Modul mit den vier Pins (P1) um 180° gedreht neben die rechte Kante des SuperMini legen (USB oben). Dann stehen VCC, SCK und DT genau gegenüber von 3V3, GPIO4 und GPIO3. Nur GND braucht eine längere Leitung nach oben.

```
  C3 SuperMini, rechte Kante        HX711-Modul, P1 (um 180° gedreht)
  5V
  GND   ●─────────────────────────────────────────────┐ (längere Leitung)
  3V3   ●──────────────────────  VCC                  │
  GPIO4 ●──────────────────────  SCK                  │
  GPIO3 ●──────────────────────  DT                   │
  GPIO2                          GND ─────────────────┘
```

Software-Standard: DT = GPIO3, SCK = GPIO4. Die Pins sind in der Weboberfläche (Tab „Messung“) frei wählbar (GPIO 0-10, 18-21). GPIO8 und GPIO9 besser meiden (Strapping-Pins, GPIO9 ist die Boot-Taste); GPIO11-17 hängen am Flash.

Der Verkabelungsplan [verkabelung.svg](verkabelung.svg) steht auch im Tab „Doku“ der Weboberfläche.

### Wägezellen (4 Stück, je 3 Drähte)

Die Zellen sitzen an den Ecken eines Rechtecks, Nummern im Uhrzeigersinn: 1 oben links, 2 oben rechts, 3 unten rechts, 4 unten links. R = rot, W = weiß, B = schwarz.

| Verbindung | Ziel |
|---|---|
| R4 | E+ (input+ / power+) |
| R2 | E− (input− / power−) |
| R3 | A+ (signal+ / output+) |
| R1 | A− (signal− / output−) |
| W1 mit W4, W2 mit W3 | Weiß untereinander, nicht am HX711 |
| B1 mit B2, B3 mit B4 | Schwarz untereinander, nicht am HX711 |

Am HX711-Modul steht die Reihenfolge (von oben): E+, E−, A−, A+, B−, B+. B− und B+ bleiben frei.

Hinweis: Das Schema, das bei manchen Wägezellen-Sets liegt (Rot 1 an E−, Rot 2 an A−), ergab bei diesem Aufbau 0,8 V zwischen A+ und A− und der HX711 sättigte. Mit vertauschten Rot-Drähten von Zelle 1 und 2 (wie in der Tabelle) liefert er gültige Werte. Ohne Last sollte A+ gegen A− nahe 0 mV liegen.

Statt vier Zellen mit je 3 Drähten geht auch eine Zelle mit 4 Adern: Rot an E+, Schwarz an E−, Weiß an A−, Grün an A+. Tauscht man A+ und A−, wird das Gewicht negativ; die Kalibrierung gleicht das aus.

Das Fritzing-Schema [waage_verdrahtung.fzz](waage_verdrahtung.fzz) zeigt C3 SuperMini, HX711 und die vier Zellen. Es nutzt das Fritzing-Teil „HX711 Weighing Sensor Module“ (CC BY-SA 3.0), das zu Fritzing gehört.

### Erster Start

1. Firmware auf den C3 bringen: fertigen Build aus den [Releases](../../releases) per USB flashen (siehe unten) oder selbst bauen.
2. Ohne bekannte WLAN-Zugangsdaten startet der Zugangspunkt `Waage-xxxxxx`. Die Seite liegt auf http://192.168.4.1/. Dort im Tab „WLAN“ das Netz eintragen. Mit `flashESP.bat` wird das WLAN schon beim Flashen per USB abgefragt.
3. Tab „MQTT senden“: Broker eintragen.
4. HX711 anschließen, im Tab „Waage“ Tara drücken (Waage leer), ein bekanntes Gewicht auflegen und kalibrieren.

Ohne angeschlossenen HX711 sendet die Waage die aktiven Topics weiter (Gewicht und Rohwert 0) und meldet zusätzlich `hx711` = `error`; die Seite zeigt dazu einen Hinweis.

### MQTT

Standard-Topics `home/<hostname>/…`; die Namen und was gesendet wird, stellt man im Tab „MQTT senden“ ein.

| Topic | Inhalt |
|---|---|
| `weight` | Gewicht in der eingestellten Einheit |
| `raw` | Rohwert (gemittelt) |
| `stable` | `true` oder `false` |
| `state` | JSON mit Gewicht, Einheit, Rohwert, Stabil, `hx711`, WLAN-Signal, Laufzeit |
| `rssi`, `uptime`, `ip`, `version` | Geräteinformationen |
| `status` | `online` / `offline` (Last Will, retained) |
| `hx711` | `ok` oder `error` (retained) |
| `cmd/tare` | Tara (jede Nachricht) |
| `cmd/calibrate` | Kalibrieren; Nachricht = aufgelegtes Gewicht |
| `cmd/restart` | Neustart (standardmäßig aus) |

Auf die Befehls-Topics nie retained senden: Die Waage würde bei jedem Verbinden erneut tarieren.

### MQTT-Broker per API

```
curl http://<IP>/api/mqtt
curl -d "broker=192.168.1.10" http://<IP>/api/mqtt
curl -d "broker=192.168.1.10&port=1883" http://<IP>/api/mqtt
```

GET liefert `{"broker":"...","port":1883,"connected":true}`. POST setzt nur die genannten Werte (Broker höchstens 63 Zeichen, Port 1-65535, sonst Antwort 400) und verbindet neu. Alle anderen Einstellungen, auch Passwörter, bleiben unverändert.

### Bauen und Flashen

Voraussetzung: Windows, Arduino IDE 2 (liefert `arduino-cli` und den ESP32-Core 3.x) und die Bibliothek **PubSubClient**.

| Datei | Aufgabe |
|---|---|
| `buildAll.bat` | baut den ESP32-C3 (ohne Benutzernamen im Binary) und kopiert die Ergebnisse nach `build_waage\esp32c3` |
| `build_waage\flashESP.bat` | Doppelklick: Waage per USB suchen, WLAN abfragen, flashen, WLAN per USB senden; nimmt `build_waage\esp32c3`, sonst das Paket vom neuesten Release |
| `build_waage\publish_builds.ps1` | erzeugt die Release-Pakete (Firmware-Zip, Werkzeug-Zip, `merged.bin`, Prüfsummen) und lädt sie mit `-Upload` hoch |
| `build_waage\cleanup_history.ps1` | nach dem Commit eines neuen Builds: entfernt die Firmware aus allen älteren Commits (nur `HEAD` behält sie); mit `-Push` per Force-Push |
| `checkDevices.ps1` | sucht Waagen im Netz, zeigt Firmware-Stand (Build-Zeit), HX711, MQTT und Uhrzeit |
| `flashAll.bat` | prüft, fragt (Enter = Ja) und flasht veraltete Waagen per WLAN (OTA) und per USB |
| `build.bat` | älteres Einzelskript (baut nach `build_waage\esp32c3`, optional mit OTA-Upload) |
| `makeWiringHeader.ps1` | erzeugt `waage\wiring_svg.h` aus `verkabelung.svg`; `buildAll.bat` ruft es vor jedem Build auf |

```
flashAll.bat               prüfen, dann fragen und flashen
flashAll.bat -CheckOnly    nur prüfen
flashAll.bat -Yes          ohne Rückfragen
flashAll.bat -Force        auch aktuelle Waagen neu einspielen
```

Board-Einstellungen: ESP32C3 Dev Module, USB CDC On Boot = Enabled, 80 MHz, QIO, 4 MB, Partitionsschema „Minimal SPIFFS (1.9MB APP with OTA)“. In der Arduino IDE oder Visual Micro dieselben Einstellungen wählen, sonst passt das Partitionsschema nicht zu OTA. Bei Builds ohne `buildAll.bat` das Skript `makeWiringHeader.ps1` nach einer Änderung der `.svg` einmal von Hand ausführen.

`flashESP.bat` und `flashAll.bat` brauchen `esptool.exe`: Liegt keines neben den Skripten, nehmen sie das aus dem Arduino-ESP32-Core, `flashESP.bat` sonst aus `waage_flash.zip` des Releases (SHA-256 geprüft). Den Stand erkennt `checkDevices.ps1` an der Kennung `WAAGE_FW_BUILD=` in der `.bin` und an `"version"` in `/api/topbarStatus`. `flashAll` listet jedes ESP32-C3 am USB und fragt vor dem Flashen.

**Fertige Builds** stehen bei den [Releases](../../releases) (`waage-esp32c3.zip` mit Prüfsumme). Zum Flashen das Werkzeug-Zip `waage_flash.zip` auspacken und `build_waage\flashESP.bat` starten (siehe `readme.txt`); es enthält `esptool.exe` samt Lizenztext und die Firmware in `esp32c3`. `waage-<Version>-esp32c3-merged.bin` ist das ganze Abbild für Adresse 0x0. Die Pakete erzeugt `build_waage\publish_builds.ps1`. Per USB, zum Beispiel mit esptool:

```
esptool --chip esp32c3 --port COM3 --baud 921600 write_flash 0x0 waage.ino.bootloader.bin 0x8000 waage.ino.partitions.bin 0xe000 boot_app0.bin 0x10000 waage.ino.bin
```

Nicht die `merged.bin` verwenden, wenn die Einstellungen erhalten bleiben sollen: Sie überschreibt auch den Speicher der Einstellungen. Ein Update per WLAN: `waage.ino.bin` mit `espota` an Port 3232 senden oder `flashAll.bat` benutzen.

### Hinweise

- Das Log liegt nur im Arbeitsspeicher und ist nach einem Neustart leer; in den Flash wird nichts geschrieben.
- Die Weboberfläche hat kein Passwort und ist für ein vertrauenswürdiges Heimnetz gedacht. Anfragen von fremden Webseiten (Origin) lehnt die Waage ab.
- Ein Update per WLAN (ArduinoOTA) braucht kein Passwort; im Heimnetz gewollt, in fremden Netzen nicht verwenden.

### Lizenz

GNU General Public License v3.0, siehe [LICENSE](LICENSE). Die Lizenzen der eingesetzten Software stehen in [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md). `esptool.exe` (Espressif, GPL-2.0-or-later, [Quelltext](https://github.com/espressif/esptool)) liegt nicht im Repository, sondern unverändert mit Lizenztext nur im Werkzeug-Zip des Releases.

## Kontakt

E-Mail: [carleaner@gmx.de](mailto:carleaner@gmx.de)
GitHub: <https://github.com/holgiw/ESP32-Scale>

---

## English

A scale (for example under a bed, or as a bathroom scale) built from an ESP32-C3 SuperMini, the HX711 load-cell amplifier and four load cells. Readings go to an MQTT broker; everything is configured through a web interface on the device. The HX711 is driven directly, without a library.

**Features**

- Live weight, tare and calibration in the web interface (with confirmation dialogs); scale factor and offset survive a restart.
- MQTT: weight, raw value, stable flag, JSON state, WiFi signal, uptime, IP, version, online/offline and HX711 status. Every topic can be switched off and renamed. Tare, calibrate and restart commands via MQTT.
- Web interface tabs: scale, measurement, MQTT send, MQTT receive, WiFi (with host name), system, status, log, info, docs (with wiring diagram). Settings export and import are at the top right.
- Notices for a missing HX711 or load cell, an offline indicator when the scale does not answer.
- Update over WiFi (ArduinoOTA), WiFi setup via USB when flashing, access point `Waage-xxxxxx` when no known WiFi is reachable.
- The MQTT broker can be set via `/api/mqtt`.

### Wiring

The HX711 is not an SPI device: it needs two signal lines (DT = data, SCK = clock) plus power. Defaults: DT = GPIO3, SCK = GPIO4, VCC = 3V3, GND = GND (pins are configurable in the "Messung" tab; avoid GPIO8/9, GPIO11-17 are flash). On the C3 SuperMini these sit next to each other on the right edge; turn the HX711 module (P1 side) by 180° so VCC, SCK and DT face 3V3, GPIO4 and GPIO3. The wiring diagram [verkabelung.svg](verkabelung.svg) is also in the "Doku" tab of the web interface.

Four load cells with 3 wires each (R red, W white, B black), numbered clockwise: 1 top left, 2 top right, 3 bottom right, 4 bottom left.

| Connection | Target |
|---|---|
| R4 | E+ |
| R2 | E− |
| R3 | A+ |
| R1 | A− |
| W1 with W4, W2 with W3 | white to each other only |
| B1 with B2, B3 with B4 | black to each other only |

The usual vendor diagram (red 1 to E−, red 2 to A−) gave 0.8 V between A+ and A− on this build and saturated the HX711; swapping the red wires of cells 1 and 2 fixes it. Without load, A+ against A− should be close to 0 mV. A single 4-wire cell also works: red E+, black E−, white A−, green A+.

### First start

1. Flash a ready build from the [Releases](../../releases) over USB, or build it yourself.
2. Without known WiFi credentials the device starts the access point `Waage-xxxxxx`; open http://192.168.4.1/ and enter the network in the "WLAN" tab. `flashESP.bat` asks for the WiFi while flashing over USB.
3. "MQTT senden" tab: enter the broker.
4. Connect the HX711, press tare with an empty scale, put on a known weight and calibrate.

### MQTT broker via API

```
curl http://<IP>/api/mqtt
curl -d "broker=192.168.1.10&port=1883" http://<IP>/api/mqtt
```

GET returns `{"broker":"...","port":1883,"connected":true}`. POST sets only the given values (broker up to 63 characters, port 1-65535, otherwise 400) and reconnects. All other settings, including passwords, stay unchanged.

### Build and flash

Requires Windows, Arduino IDE 2 (provides `arduino-cli` and the ESP32 core 3.x) and the **PubSubClient** library. `buildAll.bat` builds the ESP32-C3 (without a user name in the binary) into `build_waage\esp32c3`; `build_waage\flashESP.bat` flashes one device over USB (local build, otherwise the package from the newest release); `build_waage\publish_builds.ps1` creates the release packages; `build_waage\cleanup_history.ps1` removes the firmware from all older commits after a new build is committed; `flashAll.bat` checks all scales on the network and updates outdated ones (OTA or USB); `checkDevices.ps1` lists them. Board: ESP32C3 Dev Module, USB CDC On Boot enabled, 80 MHz, QIO, 4 MB, partition scheme "Minimal SPIFFS (1.9MB APP with OTA)". Without `buildAll.bat`, run `makeWiringHeader.ps1` once by hand after changing `verkabelung.svg`.

Ready builds are in the [Releases](../../releases) (`waage-esp32c3.zip` with checksum). To flash, unpack the tools zip `waage_flash.zip` and start `build_waage\flashESP.bat` (see `readme.txt`); it contains `esptool.exe` with its license text and the firmware in `esp32c3`. `waage-<version>-esp32c3-merged.bin` is the whole image for address 0x0. `build_waage\publish_builds.ps1` creates the packages. Over USB, for example with esptool:

```
esptool --chip esp32c3 --port COM3 --baud 921600 write_flash 0x0 waage.ino.bootloader.bin 0x8000 waage.ino.partitions.bin 0xe000 boot_app0.bin 0x10000 waage.ino.bin
```

Do not use `merged.bin` if the settings are to be kept: it also overwrites the settings storage.

### Notes

The log is kept in RAM only. The web interface has no password and is meant for a trusted home network; requests from other web pages (Origin) are rejected. ArduinoOTA needs no password.

### License

GNU General Public License v3.0, see [LICENSE](LICENSE). Licenses of the software used are in [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md). `esptool.exe` (Espressif, GPL-2.0-or-later, [source](https://github.com/espressif/esptool)) is not in the repository; it is shipped unchanged with its license text only in the tools zip of the release.

## Contact

E-mail: [carleaner@gmx.de](mailto:carleaner@gmx.de)
GitHub: <https://github.com/holgiw/ESP32-Scale>
