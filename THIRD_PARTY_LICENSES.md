# Drittlizenzen / Third-party licenses

Die Waage-Firmware ist unter der GNU General Public License v3.0 lizenziert (siehe [LICENSE](LICENSE), SPDX: `GPL-3.0-only`).
Sie nutzt die folgende Software, die nicht Teil dieses Repositories ist und mit eigener Lizenz geliefert wird. Weboberfläche (HTML, CSS, JavaScript), Verkabelungsplan und HX711-Treiber sind eigener Code ohne externe Bibliotheken, Schriften oder Bilder.

The scale firmware is licensed under the GNU General Public License v3.0 (see [LICENSE](LICENSE), SPDX: `GPL-3.0-only`).
It uses the following software, which is not part of this repository and comes with its own license. The web interface (HTML, CSS, JavaScript), the wiring diagram and the HX711 driver are own code without external libraries, fonts or images.

| Software | Version | Lizenz / License | Verwendung / Use |
|---|---|---|---|
| [arduino-esp32](https://github.com/espressif/arduino-esp32) (WiFi, WebServer, ArduinoOTA, Preferences, ESPmDNS, Network, Update) | 3.3.x | LGPL-2.1-or-later | Arduino-Core, in der Firmware enthalten / Arduino core, included in the firmware |
| [ESP-IDF](https://github.com/espressif/esp-idf) (FreeRTOS, lwIP, newlib, Systemkomponenten / system components) | mit dem Core / with the core | Apache-2.0, teils MIT/BSD / partly MIT/BSD | Grundlage des Cores, in der Firmware enthalten / base of the core, included in the firmware |
| [PubSubClient](https://github.com/knolleary/pubsubclient) | 2.8 | MIT | MQTT-Client, in der Firmware enthalten / MQTT client, included in the firmware |
| [esptool](https://github.com/espressif/esptool) | 4.8.1 (Werkzeug-Zip) / 4.8.1 (tools zip) | GPL-2.0-or-later | nicht im Repository; unveraendert nur im Werkzeug-Zip `waage_flash.zip` des Releases, mit Lizenztext `esptool-LICENSE.txt` und Hinweis `esptool-NOTICE.txt` / not in the repository; unchanged only in the tools zip `waage_flash.zip` of the release, with the license text `esptool-LICENSE.txt` and the note `esptool-NOTICE.txt` |
| [Fritzing](https://fritzing.org) Teil "HX711 Weighing Sensor Module" | Fritzing-Kern / Fritzing core | CC BY-SA 3.0 | nur als Verweis in `waage_verdrahtung.fzz`, nicht enthalten / only referenced in `waage_verdrahtung.fzz`, not included |

## esptool (GPL-2.0-or-later)

Copyright (c) Fredrik Ahlberg, Angus Gratton, Espressif Systems (Shanghai) CO LTD, other contributors as noted.

esptool.exe liegt nicht in diesem Repository. Das Werkzeug-Zip des Releases (waage_flash.zip) enthaelt es unveraendert nur zum Flashen, zusammen mit dem Lizenztext (esptool-LICENSE.txt, GNU General Public License Version 2) und einem Hinweis (esptool-NOTICE.txt). Der Quelltext steht unter https://github.com/espressif/esptool.

esptool.exe is not in this repository. The tools zip of the release (waage_flash.zip) contains it unchanged, only for flashing, together with the license text (esptool-LICENSE.txt, GNU General Public License version 2) and a note (esptool-NOTICE.txt). The source code is at https://github.com/espressif/esptool.

## arduino-esp32 (LGPL-2.1-or-later)

Copyright (c) Espressif Systems and the arduino-esp32 contributors (u. a. Ivan Grokhotkov, Hristo Gochkov).

Die Bibliotheken des Cores stehen unter der GNU Lesser General Public License, Version 2.1 oder (nach Wahl) einer neueren Version. Die Firmware wird zusammen mit ihrem vollständigen Quelltext veröffentlicht; sie lässt sich mit der Arduino IDE aus diesem Quelltext neu bauen und mit einer geänderten Version des Cores binden (`buildAll.bat`).

The core libraries are licensed under the GNU Lesser General Public License, version 2.1 or (at your option) any later version. The firmware is published together with its complete source code; it can be rebuilt from this source with the Arduino IDE and linked against a modified version of the core (`buildAll.bat`).

Lizenztext / license text: <https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html>

## ESP-IDF (Apache-2.0)

Copyright (c) Espressif Systems (Shanghai) Co., Ltd. and the ESP-IDF contributors.

Licensed under the Apache License, Version 2.0. You may obtain a copy of the License at <https://www.apache.org/licenses/LICENSE-2.0>. Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND.

Mitgelieferte Komponenten stehen teils unter MIT- oder BSD-Lizenzen (zum Beispiel FreeRTOS, lwIP, newlib); die Angaben stehen im ESP-IDF-Quelltext: <https://github.com/espressif/esp-idf>.

## PubSubClient (MIT)

Copyright (c) 2008-2020 Nicholas O'Leary

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## Fritzing-Teil / Fritzing part (CC BY-SA 3.0)

`waage_verdrahtung.fzz` verweist auf das Teil "HX711 Weighing Sensor Module" aus dem Fritzing-Kern (Autor: Richard Bruneau, korrigiert von f.althaus; Lizenz CC BY-SA 3.0). Das Teil ist nicht in diesem Repository enthalten. Die Teile "ESP32-C3 SuperMini", "Wägezelle" und "Bettrahmen" in der Datei sind eigene Zeichnungen.

`waage_verdrahtung.fzz` references the part "HX711 Weighing Sensor Module" from the Fritzing core (author Richard Bruneau, corrected by f.althaus; license CC BY-SA 3.0). The part is not included in this repository. The parts "ESP32-C3 SuperMini", "load cell" and "bed frame" inside the file are own drawings.
