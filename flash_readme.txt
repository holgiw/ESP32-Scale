# ENGLISH VERSION BELOW

#######################################################################################
# Waage (ESP32-C3 SuperMini + HX711) flashen unter Windows 10/11

1. Das Zip in einen Ordner auspacken, nicht aus dem Zip heraus starten.
   flashESP.bat, waagesetup.ps1 und esptool.exe müssen zusammen liegen, die Firmware liegt im Unterordner flash.
   Meldet der Virenscanner esptool.exe, ist das ein bekannter Fehlalarm (gepacktes Python-Programm):
   für diesen Ordner eine Ausnahme einrichten und das Zip erneut auspacken.

2. Den ESP32-C3 per USB anstecken (Datenkabel, nicht nur Ladekabel).

3. flashESP.bat per Doppelklick starten. Das Skript sucht die Waage, fragt nach dem WLAN (Enter = nicht ändern),
   flasht die Firmware und sendet das WLAN per USB. Danach meldet sich die Waage im WLAN; die Adresse steht
   im seriellen Monitor, im Router oder unter http://waage-xxxxxx.local (xxxxxx = Ende der MAC-Adresse).

   Mit Parameter ohne Rückfragen: flashESP.bat 3 (für COM3, das WLAN wird dann nicht abgefragt).

4. Spätere Updates gehen über WLAN (ArduinoOTA, Port 3232): waage.ino.bin an die Waage senden, zum Beispiel mit
   espota.py aus dem Arduino-ESP32-Core.

Die Einstellungen (WLAN, MQTT, Kalibrierung) bleiben beim Flashen erhalten, weil der Speicher der Einstellungen nicht
überschrieben wird. Von Hand mit esptool:

   esptool.exe --chip esp32c3 --port COM3 --baud 921600 write_flash 0x0 flash\waage.ino.bootloader.bin 0x8000 flash\waage.ino.partitions.bin 0xe000 flash\boot_app0.bin 0x10000 flash\waage.ino.bin

esptool.exe ist esptool von Espressif (GPL-2.0-or-later), siehe esptool-NOTICE.txt. Lizenz der Waage: LICENSE (GPL-3.0),
weitere Lizenzen: THIRD_PARTY_LICENSES.md.

Kontakt: carleaner@gmx.de - https://github.com/holgiw/ESP32-Scale

#######################################################################################
# ENGLISH VERSION

# Flashing the scale (ESP32-C3 SuperMini + HX711) on Windows 10/11

1. Unpack the zip into a folder, do not start from inside the zip.
   flashESP.bat, waagesetup.ps1 and esptool.exe must be in the same folder, the firmware is in the subfolder flash.
   If the virus scanner reports esptool.exe, it is a known false alarm (packed Python program):
   add an exception for this folder and unpack the zip again.

2. Connect the ESP32-C3 by USB (data cable, not only a charging cable).

3. Double-click flashESP.bat. The script finds the scale, asks for the WiFi (Enter = leave unchanged), flashes the
   firmware and sends the WiFi over USB. Afterwards the scale joins the WiFi; its address is shown in the serial
   monitor, in the router or at http://waage-xxxxxx.local (xxxxxx = end of the MAC address).

   With a parameter and no questions: flashESP.bat 3 (for COM3, the WiFi is then not asked for).

4. Later updates go over WiFi (ArduinoOTA, port 3232): send waage.ino.bin to the scale, for example with
   espota.py from the Arduino ESP32 core.

The settings (WiFi, MQTT, calibration) are kept when flashing because the settings storage is not overwritten. By hand
with esptool:

   esptool.exe --chip esp32c3 --port COM3 --baud 921600 write_flash 0x0 flash\waage.ino.bootloader.bin 0x8000 flash\waage.ino.partitions.bin 0xe000 flash\boot_app0.bin 0x10000 flash\waage.ino.bin

esptool.exe is esptool by Espressif (GPL-2.0-or-later), see esptool-NOTICE.txt. License of the scale: LICENSE (GPL-3.0),
further licenses: THIRD_PARTY_LICENSES.md.

Contact: carleaner@gmx.de - https://github.com/holgiw/ESP32-Scale
