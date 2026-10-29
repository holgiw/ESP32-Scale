// Automatisch aus verkabelung.svg erzeugt (makeWiringHeader.ps1, von buildAll.bat aufgerufen); nicht von Hand aendern.
// Generated automatically from verkabelung.svg (makeWiringHeader.ps1, called by buildAll.bat); do not edit by hand.

#pragma once

static const char WIRING_SVG[] PROGMEM = R"svg(<svg xmlns="http://www.w3.org/2000/svg" style="width:100%;height:auto;max-width:960px;display:block;margin:0 auto" viewBox="0 0 960 900" font-family="Arial,Helvetica,sans-serif">
  <title>Waage - Verkabelung ESP32-C3 SuperMini / HX711 / 4 W&#228;gezellen</title>
  <rect width="960" height="900" fill="#ffffff"/>
  <text x="480" y="34" text-anchor="middle" font-size="22" font-weight="bold" fill="#1a2129">Waage - Verkabelung / wiring</text>

  <!-- 1. Steuerung: ESP32-C3 - HX711 -->
  <text x="40" y="72" font-size="16" font-weight="bold" fill="#1a2129">1. Steuerung: ESP32-C3 SuperMini - HX711 (Signalleitungen DT und SCK, kein SPI)</text>

  <rect x="40" y="90" width="230" height="200" rx="10" fill="#e8f1fb" stroke="#1f5fa8" stroke-width="3"/>
  <text x="155" y="116" text-anchor="middle" font-size="17" font-weight="bold" fill="#14487f">ESP32-C3 SuperMini</text>
  <text x="155" y="136" text-anchor="middle" font-size="12" fill="#14487f">rechte Kante, USB oben</text>
  <circle cx="270" cy="150" r="5" fill="#9aa3ab"/>
  <text x="256" y="154" text-anchor="end" font-size="14" fill="#6b737c">5V (frei)</text>
  <circle cx="270" cy="175" r="5" fill="#222222"/>
  <text x="256" y="179" text-anchor="end" font-size="14" fill="#1a2129">GND</text>
  <circle cx="270" cy="200" r="5" fill="#cc1414"/>
  <text x="256" y="204" text-anchor="end" font-size="14" fill="#1a2129">3V3</text>
  <circle cx="270" cy="225" r="5" fill="#e6b800"/>
  <text x="256" y="229" text-anchor="end" font-size="14" fill="#1a2129">GPIO 4 (SCK)</text>
  <circle cx="270" cy="250" r="5" fill="#1f6fd1"/>
  <text x="256" y="254" text-anchor="end" font-size="14" fill="#1a2129">GPIO 3 (DT)</text>

  <rect x="560" y="90" width="300" height="200" rx="10" fill="#fdecea" stroke="#a82a2a" stroke-width="3"/>
  <text x="710" y="116" text-anchor="middle" font-size="17" font-weight="bold" fill="#7a1c1c">HX711-Modul</text>
  <text x="710" y="136" text-anchor="middle" font-size="12" fill="#7a1c1c">P1 links, W&#228;gezellen-Klemmen rechts</text>
  <circle cx="560" cy="200" r="5" fill="#cc1414"/>
  <text x="574" y="204" font-size="14" fill="#1a2129">VCC</text>
  <circle cx="560" cy="225" r="5" fill="#e6b800"/>
  <text x="574" y="229" font-size="14" fill="#1a2129">SCK</text>
  <circle cx="560" cy="250" r="5" fill="#1f6fd1"/>
  <text x="574" y="254" font-size="14" fill="#1a2129">DT</text>
  <circle cx="560" cy="275" r="5" fill="#222222"/>
  <text x="574" y="279" font-size="14" fill="#1a2129">GND</text>
  <circle cx="860" cy="150" r="5" fill="#cc1414"/>
  <text x="846" y="154" text-anchor="end" font-size="14" fill="#1a2129">E+</text>
  <circle cx="860" cy="175" r="5" fill="#222222"/>
  <text x="846" y="179" text-anchor="end" font-size="14" fill="#1a2129">E-</text>
  <circle cx="860" cy="200" r="5" fill="#ffffff" stroke="#6b737c" stroke-width="2"/>
  <text x="846" y="204" text-anchor="end" font-size="14" fill="#1a2129">A-</text>
  <circle cx="860" cy="225" r="5" fill="#ffffff" stroke="#6b737c" stroke-width="2"/>
  <text x="846" y="229" text-anchor="end" font-size="14" fill="#1a2129">A+</text>
  <circle cx="860" cy="250" r="5" fill="#9aa3ab"/>
  <text x="846" y="254" text-anchor="end" font-size="14" fill="#6b737c">B- (frei)</text>
  <circle cx="860" cy="275" r="5" fill="#9aa3ab"/>
  <text x="846" y="279" text-anchor="end" font-size="14" fill="#6b737c">B+ (frei)</text>

  <!-- Leitungen C3 - HX711; GND wird um die drei Signalleitungen herumgefuehrt -->
  <polyline points="270,175 420,175 420,275 560,275" fill="none" stroke="#222222" stroke-width="5" stroke-linejoin="round"/>
  <line x1="270" y1="200" x2="560" y2="200" stroke="#cc1414" stroke-width="5"/>
  <line x1="270" y1="225" x2="560" y2="225" stroke="#e6b800" stroke-width="5"/>
  <line x1="270" y1="250" x2="560" y2="250" stroke="#1f6fd1" stroke-width="5"/>
  <text x="430" y="168" text-anchor="middle" font-size="12" fill="#1a2129">GND (l&#228;ngere Leitung)</text>

  <!-- 2. Waegezellen -->
  <text x="40" y="340" font-size="16" font-weight="bold" fill="#1a2129">2. W&#228;gezellen (4 x 50 kg, je 3 Dr&#228;hte: R rot, W wei&#223;, B schwarz) an den HX711</text>
  <rect x="40" y="355" width="880" height="470" rx="16" fill="none" stroke="#8a7a5a" stroke-width="3" stroke-dasharray="14 8"/>
  <text x="480" y="815" text-anchor="middle" font-size="12" fill="#8a7a5a">Bettrahmen / Waagenrahmen, Draufsicht (nicht ma&#223;st&#228;blich)</text>

  <!-- HX711 Eingang in der Mitte -->
  <rect x="400" y="520" width="160" height="170" rx="10" fill="#fdecea" stroke="#a82a2a" stroke-width="3"/>
  <text x="480" y="545" text-anchor="middle" font-size="15" font-weight="bold" fill="#7a1c1c">HX711 Eingang</text>
  <circle cx="400" cy="570" r="6" fill="#ffffff" stroke="#6b737c" stroke-width="2"/>
  <text x="414" y="575" font-size="14" fill="#1a2129">A-</text>
  <circle cx="400" cy="650" r="6" fill="#cc1414"/>
  <text x="414" y="655" font-size="14" fill="#1a2129">E+</text>
  <circle cx="560" cy="570" r="6" fill="#222222"/>
  <text x="546" y="575" text-anchor="end" font-size="14" fill="#1a2129">E-</text>
  <circle cx="560" cy="650" r="6" fill="#ffffff" stroke="#6b737c" stroke-width="2"/>
  <text x="546" y="655" text-anchor="end" font-size="14" fill="#1a2129">A+</text>
  <text x="480" y="682" text-anchor="middle" font-size="12" fill="#6b737c">B- und B+ bleiben frei</text>

  <!-- Wei&#223;- und Schwarz-Verbindungen entlang der Rahmenkanten (nur untereinander, nicht am HX711) -->
  <line x1="240" y1="392" x2="720" y2="392" stroke="#222222" stroke-width="5"/>
  <line x1="240" y1="778" x2="720" y2="778" stroke="#222222" stroke-width="5"/>
  <line x1="130" y1="440" x2="130" y2="730" stroke="#9aa3ab" stroke-width="9"/>
  <line x1="130" y1="440" x2="130" y2="730" stroke="#ffffff" stroke-width="5"/>
  <line x1="830" y1="440" x2="830" y2="730" stroke="#9aa3ab" stroke-width="9"/>
  <line x1="830" y1="440" x2="830" y2="730" stroke="#ffffff" stroke-width="5"/>
  <text x="480" y="384" text-anchor="middle" font-size="13" fill="#1a2129">B1 - B2</text>
  <text x="480" y="800" text-anchor="middle" font-size="13" fill="#1a2129">B4 - B3</text>
  <text x="142" y="590" font-size="13" fill="#1a2129">W1 - W4</text>
  <text x="818" y="590" text-anchor="end" font-size="13" fill="#1a2129">W2 - W3</text>

  <!-- Rote Leitungen direkt an den HX711 -->
  <line x1="240" y1="428" x2="400" y2="570" stroke="#cc1414" stroke-width="5"/>
  <line x1="240" y1="742" x2="400" y2="650" stroke="#cc1414" stroke-width="5"/>
  <line x1="720" y1="428" x2="560" y2="570" stroke="#cc1414" stroke-width="5"/>
  <line x1="720" y1="742" x2="560" y2="650" stroke="#cc1414" stroke-width="5"/>
  <text x="256" y="500" font-size="13" fill="#a01010">R1 &#8594; A-</text>
  <text x="262" y="752" font-size="13" fill="#a01010">R4 &#8594; E+</text>
  <text x="704" y="500" text-anchor="end" font-size="13" fill="#a01010">R2 &#8594; E-</text>
  <text x="698" y="752" text-anchor="end" font-size="13" fill="#a01010">R3 &#8594; A+</text>

  <!-- Zellen 1 (oben links), 2 (oben rechts), 3 (unten rechts), 4 (unten links) -->
  <rect x="90" y="380" width="150" height="60" rx="6" fill="#d9dde1" stroke="#6b737c" stroke-width="3"/>
  <text x="150" y="416" text-anchor="middle" font-size="15" font-weight="bold" fill="#1a2129">Zelle 1</text>
  <circle cx="240" cy="392" r="7" fill="#222222"/>
  <circle cx="240" cy="428" r="7" fill="#cc1414"/>
  <circle cx="130" cy="440" r="7" fill="#ffffff" stroke="#6b737c" stroke-width="2"/>

  <rect x="720" y="380" width="150" height="60" rx="6" fill="#d9dde1" stroke="#6b737c" stroke-width="3"/>
  <text x="780" y="416" text-anchor="middle" font-size="15" font-weight="bold" fill="#1a2129">Zelle 2</text>
  <circle cx="720" cy="392" r="7" fill="#222222"/>
  <circle cx="720" cy="428" r="7" fill="#cc1414"/>
  <circle cx="830" cy="440" r="7" fill="#ffffff" stroke="#6b737c" stroke-width="2"/>

  <rect x="720" y="730" width="150" height="60" rx="6" fill="#d9dde1" stroke="#6b737c" stroke-width="3"/>
  <text x="780" y="766" text-anchor="middle" font-size="15" font-weight="bold" fill="#1a2129">Zelle 3</text>
  <circle cx="720" cy="778" r="7" fill="#222222"/>
  <circle cx="720" cy="742" r="7" fill="#cc1414"/>
  <circle cx="830" cy="730" r="7" fill="#ffffff" stroke="#6b737c" stroke-width="2"/>

  <rect x="90" y="730" width="150" height="60" rx="6" fill="#d9dde1" stroke="#6b737c" stroke-width="3"/>
  <text x="150" y="766" text-anchor="middle" font-size="15" font-weight="bold" fill="#1a2129">Zelle 4</text>
  <circle cx="240" cy="778" r="7" fill="#222222"/>
  <circle cx="240" cy="742" r="7" fill="#cc1414"/>
  <circle cx="130" cy="730" r="7" fill="#ffffff" stroke="#6b737c" stroke-width="2"/>

  <!-- Hinweise -->
  <text x="40" y="852" font-size="13" fill="#1a2129">Rot geht einzeln an den HX711: Zelle 4 = E+, Zelle 2 = E-, Zelle 3 = A+, Zelle 1 = A-. Wei&#223; und Schwarz nur zwischen den Zellen verbinden.</text>
  <text x="40" y="874" font-size="13" fill="#1a2129">Kontrolle ohne Last: Spannung A+ gegen A- nahe 0 mV. Rund 0,8 V hei&#223;t: Rot-Dr&#228;hte von Zelle 1 und 2 sind vertauscht.</text>
</svg>)svg";
