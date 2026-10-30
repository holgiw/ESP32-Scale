# ENGLISH VERSION BELOW

#######################################################################################
# Waage (ESP32-C3 SuperMini + HX711) flashen unter Windows 10/11

1. Das Zip (waage_flash.zip oder das Repository-Zip) in einen Ordner auspacken, nicht aus dem Zip heraus starten.
   flashESP.bat, waagesetup.ps1 und esptool.exe müssen zusammen liegen, die Firmware liegt im Unterordner esp32c3.
   Fehlt der Unterordner, lädt flashESP.bat das Paket vom neuesten GitHub-Release (SHA-256 geprüft, der PC braucht Internet). Fehlt esptool.exe (Repository-Zip ohne Arduino IDE), lädt es das
   Skript aus waage_flash.zip des Releases.
   Meldet der Virenscanner esptool.exe, ist das ein bekannter Fehlalarm (gepacktes Python-Programm):
   für diesen Ordner eine Ausnahme einrichten und das Zip erneut auspacken.

2. Den ESP32-C3 per USB anstecken (Datenkabel, nicht nur Ladekabel).

3. flashESP.bat per Doppelklick starten. Das Skript sucht die Waage, fragt nach dem WLAN (Enter = nicht ändern),
   flasht die Firmware und sendet das WLAN per USB. Danach meldet sich die Waage im WLAN; die Adresse steht
   im seriellen Monitor, im Router oder unter http://waage-xxxxxx.local (xxxxxx = Ende der MAC-Adresse).

   Mit Parameter ohne Rückfragen: flashESP.bat 3 (für COM3, das WLAN wird dann nicht abgefragt).
   Schalter für waagesetup.ps1: -Download (Paket vom Release erzwingen), -FetchTools (esptool.exe vom Release erzwingen).

4. Spätere Updates gehen über WLAN (ArduinoOTA, Port 3232): waage.ino.bin an die Waage senden, zum Beispiel mit
   espota.py aus dem Arduino-ESP32-Core.

Die Einstellungen (WLAN, MQTT, Kalibrierung) bleiben beim Flashen erhalten, weil der Speicher der Einstellungen nicht
überschrieben wird. Von Hand mit esptool (im Ordner esp32c3):

   esptool.exe --chip esp32c3 --port COM3 --baud 921600 write_flash 0x0 waage.ino.bootloader.bin 0x8000 waage.ino.partitions.bin 0xe000 boot_app0.bin 0x10000 waage.ino.bin

Die waage-<Version>-esp32c3-merged.bin aus dem Release schreibt das ganze Abbild bei Adresse 0x0 und löscht dabei auch die
Einstellungen:

   esptool.exe --chip esp32c3 --port COM3 --baud 921600 write_flash 0x0 waage-<Version>-esp32c3-merged.bin

Lizenzen: Die Waage steht unter der GPL-3.0 (LICENSE). esptool.exe ist esptool von Espressif (GPL-2.0-or-later, Quelltext
https://github.com/espressif/esptool, Lizenztext esptool-LICENSE.txt, Hinweis esptool-NOTICE.txt) und liegt nur im Werkzeug-Zip
unverändert bei. Weitere Lizenzen: THIRD_PARTY_LICENSES.md.

Kontakt: carleaner@gmx.de - https://github.com/holgiw/ESP32-Scale

#######################################################################################
# ENGLISH VERSION

# Flashing the scale (ESP32-C3 SuperMini + HX711) on Windows 10/11

1. Unpack the zip (waage_flash.zip or the repository zip) into a folder, do not start from inside the zip.
   flashESP.bat, waagesetup.ps1 and esptool.exe must be in the same folder, the firmware is in the subfolder esp32c3.
   If the subfolder is missing, flashESP.bat downloads the package from the newest GitHub release (SHA-256 checked, the PC needs
   internet). If esptool.exe is missing (repository zip without the Arduino IDE), the script fetches it from waage_flash.zip of
   the release.
   If the virus scanner reports esptool.exe, it is a known false alarm (packed Python program):
   add an exception for this folder and unpack the zip again.

2. Connect the ESP32-C3 by USB (data cable, not only a charging cable).

3. Double-click flashESP.bat. The script finds the scale, asks for the WiFi (Enter = leave unchanged), flashes the
   firmware and sends the WiFi over USB. Afterwards the scale joins the WiFi; its address is shown in the serial
   monitor, in the router or at http://waage-xxxxxx.local (xxxxxx = end of the MAC address).

   With a parameter and no questions: flashESP.bat 3 (for COM3, the WiFi is then not asked for).
   Switches for waagesetup.ps1: -Download (force the package from the release), -FetchTools (force esptool.exe from the release).

4. Later updates go over WiFi (ArduinoOTA, port 3232): send waage.ino.bin to the scale, for example with
   espota.py from the Arduino ESP32 core.

The settings (WiFi, MQTT, calibration) are kept when flashing because the settings storage is not overwritten. By hand
with esptool (in the folder esp32c3):

   esptool.exe --chip esp32c3 --port COM3 --baud 921600 write_flash 0x0 waage.ino.bootloader.bin 0x8000 waage.ino.partitions.bin 0xe000 boot_app0.bin 0x10000 waage.ino.bin

The waage-<version>-esp32c3-merged.bin from the release writes the whole image at address 0x0 and also erases the settings:

   esptool.exe --chip esp32c3 --port COM3 --baud 921600 write_flash 0x0 waage-<version>-esp32c3-merged.bin

Licenses: The scale is licensed under the GPL-3.0 (LICENSE). esptool.exe is esptool by Espressif (GPL-2.0-or-later, source
https://github.com/espressif/esptool, license text esptool-LICENSE.txt, note esptool-NOTICE.txt) and is included unchanged in the
tools zip only. Further licenses: THIRD_PARTY_LICENSES.md.

Contact: carleaner@gmx.de - https://github.com/holgiw/ESP32-Scale
