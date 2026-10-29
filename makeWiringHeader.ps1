# SPDX-License-Identifier: GPL-3.0-only (siehe / see LICENSE)
# Erzeugt waage\wiring_svg.h aus verkabelung.svg, damit das Verkabelungsbild im Tab Doku der Weboberflaeche erscheint.
# Wird von buildAll.bat vor jedem Build aufgerufen; das Bild wird nur in verkabelung.svg gepflegt.
#
# Creates waage\wiring_svg.h from verkabelung.svg so that the wiring picture appears in the Doku tab of the web interface.
# Called by buildAll.bat before every build; the picture is maintained only in verkabelung.svg.
Set-Location $PSScriptRoot
$svg = [IO.File]::ReadAllText("$PSScriptRoot\verkabelung.svg", [Text.Encoding]::UTF8).Replace("`r`n", "`n")

# Feste Groesse entfernen, damit sich das Bild der Seitenbreite anpasst.
# Remove the fixed size so the picture adapts to the page width.
$svg = ([regex]::new('<svg ([^>]*?)width="\d+" height="\d+" ')).Replace($svg, '<svg $1style="width:100%;height:auto;max-width:960px;display:block;margin:0 auto" ', 1)
if ($svg.Contains(')svg"')) { throw 'verkabelung.svg contains the raw string terminator' }
$head = "// Automatisch aus verkabelung.svg erzeugt (makeWiringHeader.ps1, von buildAll.bat aufgerufen); nicht von Hand aendern.`n// Generated automatically from verkabelung.svg (makeWiringHeader.ps1, called by buildAll.bat); do not edit by hand.`n`n#pragma once`n`n"
$text = $head + 'static const char WIRING_SVG[] PROGMEM = R"svg(' + $svg.TrimEnd() + ')svg";' + "`n"

# Der Sketch liegt im Unterordner waage, die Arduino-IDE kompiliert die Header-Datei dort mit.
# The sketch is in the subfolder waage, the Arduino IDE compiles the header file there.
$target = "$PSScriptRoot\waage\wiring_svg.h"
$old = if (Test-Path $target) { [IO.File]::ReadAllText($target) } else { '' }
if ($old -ne $text) { [IO.File]::WriteAllText($target, $text, (New-Object Text.UTF8Encoding($false))); 'wiring_svg.h updated' } else { 'wiring_svg.h up to date' }
