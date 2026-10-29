/*
Claude   10/26
  Waage - ESP32-C3 + HX711 -> MQTT, komplett per Web-UI konfigurierbar
  Scale - ESP32-C3 + HX711 -> MQTT, fully configurable via web UI

  Board:      ESP32C3 Dev Module, USB CDC On Boot: Enabled, CPU 80 MHz
  Partition:  "Minimal SPIFFS (1.9MB APP with OTA/128KB SPIFFS)" - Schema MIT OTA / scheme WITH OTA
  Bibliothek: PubSubClient (HX711 wird ohne Bibliothek direkt angesprochen)

  Verdrahtung / wiring (Standard, im Web-UI aenderbar):
    HX711 DT  -> GPIO3      HX711 SCK -> GPIO4
    HX711 VCC -> 3V3        HX711 GND -> GND

  Lizenz / License: GNU General Public License v3.0 (siehe / see LICENSE), https://github.com/holgiw/ESP32-Scale
  SPDX-License-Identifier: GPL-3.0-only
  Drittsoftware / third-party software (THIRD_PARTY_LICENSES.md):
    arduino-esp32 (LGPL-2.1-or-later), ESP-IDF (Apache-2.0), PubSubClient (MIT, (c) Nicholas O'Leary)
*/

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <esp_task_wdt.h>
#include <time.h>
#include "wiring_svg.h"

#define FW_VERSION   "1.0"

// Kennung im Binary: checkDevices.ps1 liest daraus die Build-Zeit der .bin (Soll-Stand)
// Marker in the binary: checkDevices.ps1 reads the build time of the .bin from it (target state)

#define FIRMWARE_MARKER "WAAGE_FW_BUILD=" __DATE__ " " __TIME__
#define MAX_WLAN     5
#define MAX_RING     40
#define OTA_TIMEOUT_MS 180000
#define LOG_MAX      6000

// ------------------------------------------------------------------ Topics
// Gesendete Topics (Index = Position in PUB[]) / published topics (index = position in PUB[])
enum { P_WEIGHT, P_RAW, P_STABLE, P_STATE, P_RSSI, P_UPTIME, P_IP, P_VERSION, P_STATUS, P_HX, NPUB };
// Empfangene Topics / subscribed topics
enum { C_TARE, C_CAL, C_RESTART, NSUB };

struct TopicDef { const char* key; const char* label; const char* suffix; bool defEn; };
const TopicDef PUB[NPUB] = {
    {"weight",  "Gewicht",                    "weight",  true},
    {"raw",     "Rohwert (Counts)",           "raw",     false},
    {"stable",  "Stabil (true/false)",        "stable",  false},
    {"state",   "Gesamtstatus (JSON)",        "state",   true},
    {"rssi",    "WLAN-Signal (dBm)",          "rssi",    true},
    {"uptime",  "Laufzeit (s)",               "uptime",  false},
    {"ip",      "IP-Adresse",                 "ip",      true},
    {"version", "Firmware-Version",           "version", false},
    {"status",  "Online/Offline (LWT)",       "status",  true},
    {"hx711",   "HX711-Status (ok/error)",    "hx711",   true}
};
const TopicDef SUB[NSUB] = {
    {"tare",    "Tara ausfuehren",                    "cmd/tare",      true},
    {"cal",     "Kalibrieren (Payload = Gewicht)",    "cmd/calibrate", true},
    {"restart", "Neustart",                           "cmd/restart",   false}
};

// ------------------------------------------------------------------ Konfiguration
struct Config {
    String host;
    String wSsid[MAX_WLAN], wPass[MAX_WLAN];
    String mqHost; uint16_t mqPort; String mqUser, mqPass, mqId;
    bool retain; uint16_t pubInt; float thr; uint16_t infoInt;
    bool pEn[NPUB]; String pTopic[NPUB];
    bool cEn[NSUB]; String cTopic[NSUB];
    int dout, sck, gain, navg;
    String unit; int dec;
    double scale, offset;
    float band, minw, maxw;
    String ntp1, ntp2, tz;
} cfg;

String pLast[NPUB], cLast[NSUB];

Preferences prefs;
WebServer server(80);
WiFiClient wifiClient;
PubSubClient mq(wifiClient);

// ------------------------------------------------------------------ Zustand
bool otaInProgress = false;
unsigned long otaStartMillis = 0;
unsigned long restartAt = 0;

String logBuf;
String pg;

bool apMode = false;
bool wasConn = false;
bool mdnsOn = false;
unsigned long wifiLastKick = 0;
int kickIdx = 0;
bool ntpDirty = true;

bool mqEver = false;
String lastHxPub = "";
unsigned long mqLastTry = 0;
uint8_t mqFails = 0;

// HX711
portMUX_TYPE hxMux = portMUX_INITIALIZER_UNLOCKED;
long ring[MAX_RING];
int ringCnt = 0, ringPos = 0;
double avgRaw = 0;
long winMin = 0, winMax = 0, lastRaw = 0;
uint32_t hxReads = 0, hxErrors = 0, hxLastOk = 0;
bool hxReleased = false;
uint32_t hxStuck = 0;
uint32_t hxChipReads = 0, hxSatReads = 0, hxChipLastMs = 0;
bool hxConfirmed = false;
int freshCnt = 0;
bool tarePending = false, calPending = false;
double calWeight = 0;
String actionMsg = "";

// Veroeffentlichung / publishing
unsigned long lastPub = 0, lastInfo = 0;
double lastPubW = 0;

// ------------------------------------------------------------------ Hilfsfunktionen
bool timeValid() { return time(nullptr) > 1700000000; }

void logLine(const String& msg) {
    char ts[40];
    if (timeValid()) {
        struct tm t; time_t n = time(nullptr); localtime_r(&n, &t);
        size_t l = strftime(ts, sizeof(ts), "[%Y-%m-%d %H:%M:%S", &t);
        snprintf(ts + l, sizeof(ts) - l, ".%03lu] ", millis() % 1000);
    } else {
        snprintf(ts, sizeof(ts), "[%lu ms] ", millis());
    }
    String line = String(ts) + msg + "\n";
    Serial.print(line);
    logBuf += line;
    if (logBuf.length() > LOG_MAX) {
        int cut = logBuf.indexOf('\n', LOG_MAX / 4);
        logBuf.remove(0, cut < 0 ? LOG_MAX / 4 : cut + 1);
    }
}

String esc(const String& s) {
    String o; o.reserve(s.length() + 8);
    for (size_t i = 0; i < s.length(); i++) {
        char c = s[i];
        switch (c) {
        case '&': o += "&amp;"; break;
        case '<': o += "&lt;"; break;
        case '>': o += "&gt;"; break;
        case '"': o += "&quot;"; break;
        case '\'': o += "&#39;"; break;
        default: o += c;
        }
    }
    return o;
}

String jesc(const String& s) {
    String o;
    for (size_t i = 0; i < s.length(); i++) {
        char c = s[i];
        if (c == '"' || c == '\\') { o += '\\'; o += c; }
        else if ((unsigned char)c >= 32) o += c;
    }
    return o;
}

String fmtNum(double v, int dec) {
    char b[32];
    snprintf(b, sizeof(b), "%.*f", dec, v);
    return String(b);
}

double parseNum(const String& s, double def) {
    String t = s; t.trim(); t.replace(',', '.');
    if (t.length() == 0) return def;
    return t.toDouble();
}

String macSuffix() {
    uint8_t m[6]; WiFi.macAddress(m);
    char b[8]; snprintf(b, sizeof(b), "%02x%02x%02x", m[3], m[4], m[5]);
    return String(b);
}

String cleanHost(String h) {
    h.trim(); h.toLowerCase();
    String o;
    for (size_t i = 0; i < h.length() && o.length() < 31; i++) {
        char c = h[i];
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-') o += c;
    }
    return o.length() ? o : String("waage-") + macSuffix();
}

String defTopic(const char* suffix) { return "home/" + cfg.host + "/" + suffix; }

bool validPin(int p) { return (p >= 0 && p <= 10) || (p >= 18 && p <= 21); }

// ------------------------------------------------------------------ Preferences
void fixTopics() {
    for (int i = 0; i < NPUB; i++) { cfg.pTopic[i].trim(); if (!cfg.pTopic[i].length()) cfg.pTopic[i] = defTopic(PUB[i].suffix); }
    for (int i = 0; i < NSUB; i++) { cfg.cTopic[i].trim(); if (!cfg.cTopic[i].length()) cfg.cTopic[i] = defTopic(SUB[i].suffix); }
}

void loadConfig() {
    prefs.begin("waage", true);
    cfg.host = cleanHost(prefs.getString("host", ""));
    for (int i = 0; i < MAX_WLAN; i++) {
        cfg.wSsid[i] = prefs.getString(("ws" + String(i)).c_str(), "");
        cfg.wPass[i] = prefs.getString(("wp" + String(i)).c_str(), "");
    }
    cfg.mqHost = prefs.getString("mqhost", "");
    cfg.mqPort = prefs.getUShort("mqport", 1883);
    cfg.mqUser = prefs.getString("mquser", "");
    cfg.mqPass = prefs.getString("mqpass", "");
    cfg.mqId = prefs.getString("mqid", "");
    cfg.retain = prefs.getBool("retain", false);
    cfg.pubInt = prefs.getUShort("pubint", 10);
    cfg.thr = prefs.getFloat("thr", 0);
    cfg.infoInt = prefs.getUShort("infoint", 300);
    for (int i = 0; i < NPUB; i++) {
        cfg.pEn[i] = prefs.getBool(("pe" + String(i)).c_str(), PUB[i].defEn);
        cfg.pTopic[i] = prefs.getString(("pt" + String(i)).c_str(), "");
    }
    for (int i = 0; i < NSUB; i++) {
        cfg.cEn[i] = prefs.getBool(("ce" + String(i)).c_str(), SUB[i].defEn);
        cfg.cTopic[i] = prefs.getString(("ct" + String(i)).c_str(), "");
    }
    cfg.dout = prefs.getInt("dout", 3);
    cfg.sck = prefs.getInt("sck", 4);
    cfg.gain = prefs.getInt("gain", 128);
    cfg.navg = prefs.getInt("navg", 10);
    cfg.unit = prefs.getString("unit", "kg");
    cfg.dec = prefs.getInt("dec", 1);
    cfg.scale = prefs.getDouble("scale", 1.0);
    cfg.offset = prefs.getDouble("offset", 0.0);
    cfg.band = prefs.getFloat("band", 0.5f);
    cfg.minw = prefs.getFloat("minw", 0);
    cfg.maxw = prefs.getFloat("maxw", 0);
    cfg.ntp1 = prefs.getString("ntp1", "pool.ntp.org");
    cfg.ntp2 = prefs.getString("ntp2", "time.google.com");
    cfg.tz = prefs.getString("tz", "CET-1CEST,M3.5.0,M10.5.0/3");
    prefs.end();
    if (!validPin(cfg.dout) || !validPin(cfg.sck) || cfg.dout == cfg.sck) { cfg.dout = 3; cfg.sck = 4; }
    if (cfg.gain != 32 && cfg.gain != 64) cfg.gain = 128;
    cfg.navg = constrain(cfg.navg, 1, MAX_RING);
    cfg.dec = constrain(cfg.dec, 0, 4);
    if (cfg.scale == 0) cfg.scale = 1.0;
    fixTopics();
}

void saveConfig() {
    fixTopics();
    prefs.begin("waage", false);
    prefs.putString("host", cfg.host);
    for (int i = 0; i < MAX_WLAN; i++) {
        prefs.putString(("ws" + String(i)).c_str(), cfg.wSsid[i]);
        prefs.putString(("wp" + String(i)).c_str(), cfg.wPass[i]);
    }
    prefs.putString("mqhost", cfg.mqHost);
    prefs.putUShort("mqport", cfg.mqPort);
    prefs.putString("mquser", cfg.mqUser);
    prefs.putString("mqpass", cfg.mqPass);
    prefs.putString("mqid", cfg.mqId);
    prefs.putBool("retain", cfg.retain);
    prefs.putUShort("pubint", cfg.pubInt);
    prefs.putFloat("thr", cfg.thr);
    prefs.putUShort("infoint", cfg.infoInt);
    for (int i = 0; i < NPUB; i++) {
        prefs.putBool(("pe" + String(i)).c_str(), cfg.pEn[i]);
        prefs.putString(("pt" + String(i)).c_str(), cfg.pTopic[i]);
    }
    for (int i = 0; i < NSUB; i++) {
        prefs.putBool(("ce" + String(i)).c_str(), cfg.cEn[i]);
        prefs.putString(("ct" + String(i)).c_str(), cfg.cTopic[i]);
    }
    prefs.putInt("dout", cfg.dout);
    prefs.putInt("sck", cfg.sck);
    prefs.putInt("gain", cfg.gain);
    prefs.putInt("navg", cfg.navg);
    prefs.putString("unit", cfg.unit);
    prefs.putInt("dec", cfg.dec);
    prefs.putDouble("scale", cfg.scale);
    prefs.putDouble("offset", cfg.offset);
    prefs.putFloat("band", cfg.band);
    prefs.putFloat("minw", cfg.minw);
    prefs.putFloat("maxw", cfg.maxw);
    prefs.putString("ntp1", cfg.ntp1);
    prefs.putString("ntp2", cfg.ntp2);
    prefs.putString("tz", cfg.tz);
    prefs.end();
}

void saveCal() {
    prefs.begin("waage", false);
    prefs.putDouble("scale", cfg.scale);
    prefs.putDouble("offset", cfg.offset);
    prefs.end();
}

// ------------------------------------------------------------------ HX711
void hxBegin() {
    pinMode(cfg.sck, OUTPUT);
    digitalWrite(cfg.sck, LOW);
    pinMode(cfg.dout, INPUT_PULLUP);
    ringCnt = 0; ringPos = 0; freshCnt = 0;
    logLine("[HX711] Init DOUT=GPIO" + String(cfg.dout) + " SCK=GPIO" + String(cfg.sck) + " gain=" + String(cfg.gain));
}

long hxReadRaw() {
    uint32_t v = 0;
    int extra = (cfg.gain == 128) ? 1 : (cfg.gain == 64) ? 3 : 2;
    portENTER_CRITICAL(&hxMux);
    for (int i = 0; i < 24; i++) {
        digitalWrite(cfg.sck, HIGH);
        delayMicroseconds(1);
        v = (v << 1) | (uint32_t)digitalRead(cfg.dout);
        digitalWrite(cfg.sck, LOW);
        delayMicroseconds(1);
    }
    for (int i = 0; i < extra; i++) {
        digitalWrite(cfg.sck, HIGH);
        delayMicroseconds(1);
        digitalWrite(cfg.sck, LOW);
        delayMicroseconds(1);
    }
    portEXIT_CRITICAL(&hxMux);

    // Nach dem 25. Takt gibt ein echter HX711 DOUT frei (HIGH); bleibt der Pin LOW, ist dort kein HX711
    // After the 25th clock a real HX711 releases DOUT (HIGH); if the pin stays LOW there is no HX711

    delayMicroseconds(3);
    hxReleased = (digitalRead(cfg.dout) == HIGH);
    if (v & 0x800000) v |= 0xFF000000;
    return (long)(int32_t)v;
}

double curWeight() {
    double s = (cfg.scale == 0) ? 1.0 : cfg.scale;
    return (avgRaw - cfg.offset) / s;
}

bool isStable() {
    int n = min(cfg.navg, ringCnt);
    if (n < cfg.navg) return false;
    if (cfg.band <= 0) return true;
    double s = fabs(cfg.scale == 0 ? 1.0 : cfg.scale);
    return ((winMax - winMin) / s) <= cfg.band;
}

unsigned long pendingSince = 0;

void doTare() { tarePending = true; calPending = false; freshCnt = 0; pendingSince = millis(); actionMsg = "Tara laeuft ..."; logLine("[HX711] Tare requested"); }
void doCal(double w) {
    if (!(w > 0)) { actionMsg = "FAIL|Das Gewicht muss gr\xC3\xB6\xC3\x9F" "er als 0 sein"; logLine("[HX711] Calibration rejected, weight=" + fmtNum(w, 3)); return; }
    calPending = true; tarePending = false; calWeight = w; freshCnt = 0; pendingSince = millis();
    actionMsg = "Kalibrierung laeuft ..."; logLine("[HX711] Calibration requested, weight=" + fmtNum(w, 3));
}

// Tara/Kalibrierung ohne gueltige Messwerte nach 15 s abbrechen, sonst wuerde sie spaeter unbemerkt ausgefuehrt
// Cancel tare/calibration after 15 s without valid readings, otherwise it would run unnoticed later

void hxPendingTimeout() {
    if ((tarePending || calPending) && millis() - pendingSince > 15000) {
        logLine(String("[HX711] ") + (tarePending ? "Tare" : "Calibration") + " cancelled: no valid readings");
        actionMsg = String("FAIL|") + (tarePending ? "Tara" : "Kalibrierung") + " abgebrochen: keine g\xC3\xBCltigen Messwerte vom HX711";
        tarePending = false; calPending = false;
    }
}

void hxLoop() {
    // DOUT = LOW heisst: Messwert bereit (bleibt LOW, bis er ausgelesen wird)
    // DOUT = LOW means: conversion ready (stays LOW until it is read out)

    if (digitalRead(cfg.dout) != LOW) return;
    long v = hxReadRaw();
    lastRaw = v;
    if (!hxReleased) { hxStuck++; hxErrors++; return; }

    // Der Chip hat geantwortet (DOUT wurde frei); gesaettigte Werte = Waegezelle fehlt oder ueberlastet
    // The chip answered (DOUT was released); saturated values = load cell missing or overloaded

    hxChipReads++;
    hxChipLastMs = millis();
    if (v >= 0x7FFFFF || v <= -0x800000) { hxSatReads++; hxErrors++; return; }
    if (v == 0 || v == -1) { hxErrors++; return; }
    hxReads++;
    hxLastOk = millis();
    if (!hxConfirmed && hxReads >= 5) { hxConfirmed = true; logLine("[HX711] Sensor responding"); }
    ring[ringPos] = v;
    ringPos = (ringPos + 1) % MAX_RING;
    if (ringCnt < MAX_RING) ringCnt++;
    freshCnt++;
    int n = min(cfg.navg, ringCnt);
    double sum = 0; long mn = v, mx = v;
    for (int k = 0; k < n; k++) {
        long x = ring[(ringPos - 1 - k + 2 * MAX_RING) % MAX_RING];
        sum += x;
        if (x < mn) mn = x;
        if (x > mx) mx = x;
    }
    avgRaw = sum / n; winMin = mn; winMax = mx;

    if ((tarePending || calPending) && freshCnt >= cfg.navg) {
        if (tarePending) {
            cfg.offset = avgRaw; saveCal();
            actionMsg = "OK|Tara gesetzt (Offset " + fmtNum(cfg.offset, 0) + ")";
            logLine("[HX711] Tare done, offset=" + fmtNum(cfg.offset, 0));
        } else {
            double d = avgRaw - cfg.offset;
            if (fabs(d) < 100 || calWeight == 0) {
                actionMsg = "FAIL|Keine Last erkannt oder Gewicht 0 - erst Tara, dann Gewicht auflegen";
                logLine("[HX711] Calibration failed, delta=" + fmtNum(d, 0));
            } else {
                cfg.scale = d / calWeight; saveCal();
                actionMsg = "OK|Kalibriert (Faktor " + fmtNum(cfg.scale, 4) + " Counts/" + cfg.unit + ")";
                logLine("[HX711] Calibration done, scale=" + fmtNum(cfg.scale, 4));
            }
        }
        tarePending = false; calPending = false;
    }
}

// ------------------------------------------------------------------ Statusermittlung
String hxState() {
    if (hxLastOk && millis() - hxLastOk < 3000) return "ok";
    if (!hxLastOk && millis() < 8000) return "syncing";
    return "bad";
}

// "ok" = gueltiger Messwert, "nocell" = HX711 antwortet, Wert aber ungueltig (z. B. Waegezelle fehlt), "nochip" = HX711 antwortet nicht
// "ok" = valid reading, "nocell" = HX711 answers but the value is invalid (e.g. load cell missing), "nochip" = HX711 does not answer
String hxDetail() {
    if (hxState() == "ok") return "ok";
    if (hxChipLastMs && millis() - hxChipLastMs < 3000) return "nocell";
    return "nochip";
}
String hxHintText() {
    if (hxDetail() == "nocell") return "HX711 antwortet, liefert aber keinen g&uuml;ltigen Messwert - bitte die W&auml;gezellen an E+, E-, A+, A- pr&uuml;fen.";
    return "HX711 antwortet nicht - bitte DT (GPIO" + String(cfg.dout) + "), SCK (GPIO" + String(cfg.sck) + "), VCC und GND pr&uuml;fen.";
}
String hxTitleText(const char* label, const String& st) {
    if (st != "bad") return dotText(label, st);
    return String(label) + (hxDetail() == "nocell" ? ": keine gueltige Waegezelle" : ": nicht gefunden");
}
String mqState() { if (!cfg.mqHost.length()) return "na"; return mq.connected() ? "ok" : "bad"; }
String timeState() { return timeValid() ? "ok" : "na"; }

// Gleiche Texte fuer Erstrendering und Poll / same texts for first render and poll
String dotText(const char* label, const String& st) {
    String t = label; t += ": ";
    if (st == "ok") t += "OK";
    else if (st == "syncing") t += "Synchronisiere";
    else if (st == "bad") t += "Fehler";
    else t += "Nicht verfuegbar";
    return t;
}

// Build-Zeit als "yyyy-MM-dd HH:mm:ss" aus der Kennung (die Nutzung haelt die Kennung im Binary)
// Build time as "yyyy-MM-dd HH:mm:ss" from the marker (using it keeps the marker in the binary)

String fwVersion() {
    static const char mon[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    const char* s = FIRMWARE_MARKER + 15;
    char m[4] = {0};
    int d = 0, y = 0, hh = 0, mm = 0, ss = 0;
    sscanf(s, "%3s %d %d %d:%d:%d", m, &d, &y, &hh, &mm, &ss);
    const char* p = strstr(mon, m);
    int mi = p ? (int)((p - mon) / 3) + 1 : 0;
    char b[24];
    snprintf(b, sizeof(b), "%04d-%02d-%02d %02d:%02d:%02d", y, mi, d, hh, mm, ss);
    return String(b);
}

String weightText() { return fmtNum(curWeight(), cfg.dec) + " " + cfg.unit; }

String dateTimeText() {
    if (!timeValid()) return "";
    struct tm t; time_t n = time(nullptr); localtime_r(&n, &t);
    char b[24]; strftime(b, sizeof(b), "%d.%m.%Y %H:%M", &t);
    return String(b);
}

String ipText() { return apMode && WiFi.status() != WL_CONNECTED ? WiFi.softAPIP().toString() : WiFi.localIP().toString(); }

// ------------------------------------------------------------------ MQTT
void pubT(int i, const String& v) {
    if (!cfg.pEn[i] || !mq.connected()) return;
    bool r = (i == P_STATUS) || (i == P_HX) || cfg.retain;
    if (mq.publish(cfg.pTopic[i].c_str(), v.c_str(), r)) pLast[i] = v;
    else logLine("[MQTT] Publish failed: " + cfg.pTopic[i] + " len=" + String(v.length()) + " state=" + String(mq.state()));
}

String stateJson() {
    String j = "{\"weight\":" + fmtNum(curWeight(), cfg.dec);
    j += ",\"unit\":\"" + jesc(cfg.unit) + "\"";
    j += ",\"raw\":" + fmtNum(avgRaw, 0);
    j += ",\"stable\":" + String(isStable() ? "true" : "false");
    j += ",\"hx711\":\"" + String(hxState() == "ok" ? "ok" : "error") + "\"";
    j += ",\"rssi\":" + String(WiFi.RSSI());
    j += ",\"uptime\":" + String(millis() / 1000) + "}";
    return j;
}

void publishMeasure() {
    if (!mq.connected()) return;
    double w = curWeight();
    bool inRange = (cfg.minw >= cfg.maxw) || (w >= cfg.minw && w <= cfg.maxw);
    if (inRange) pubT(P_WEIGHT, fmtNum(w, cfg.dec));
    pubT(P_RAW, fmtNum(avgRaw, 0));
    pubT(P_STABLE, isStable() ? "true" : "false");
    pubT(P_STATE, stateJson());
    lastPub = millis(); lastPubW = w;
}

void publishInfo() {
    if (!mq.connected()) return;
    pubT(P_RSSI, String(WiFi.RSSI()));
    pubT(P_UPTIME, String(millis() / 1000));
    pubT(P_IP, ipText());
    pubT(P_VERSION, FW_VERSION);
    lastInfo = millis();
}

void mqttCallback(char* topic, byte* payload, unsigned int len) {
    String v; for (unsigned int i = 0; i < len && i < 64; i++) v += (char)payload[i];
    for (int i = 0; i < NSUB; i++) {
        if (!cfg.cEn[i] || cfg.cTopic[i] != topic) continue;
        cLast[i] = v;
        logLine("[MQTT] Command " + String(SUB[i].key) + " = " + v);
        if (i == C_TARE) doTare();
        else if (i == C_CAL) doCal(parseNum(v, 0));
        else if (i == C_RESTART) restartAt = millis() + 500;
    }
}

void mqttTry() {
    if (!cfg.mqHost.length() || WiFi.status() != WL_CONNECTED) return;

    // Bei dauerhaft fehlendem Broker laenger warten (5, 10, 20, 40, max. 60 s): der Verbindungsversuch blockiert die Seite
    // With a permanently missing broker wait longer (5, 10, 20, 40, max. 60 s): a connection attempt blocks the web page

    unsigned long wait = mqFails == 0 ? 5000UL : min(5000UL << min((int)mqFails, 4), 60000UL);
    if (mqLastTry && millis() - mqLastTry < wait) return;
    mqLastTry = millis();
    mq.setServer(cfg.mqHost.c_str(), cfg.mqPort);
    String id = cfg.mqId.length() ? cfg.mqId : cfg.host;
    const char* u = cfg.mqUser.length() ? cfg.mqUser.c_str() : nullptr;
    const char* p = (u && cfg.mqPass.length()) ? cfg.mqPass.c_str() : nullptr;
    bool ok;
    if (cfg.pEn[P_STATUS]) ok = mq.connect(id.c_str(), u, p, cfg.pTopic[P_STATUS].c_str(), 1, true, "offline");
    else ok = mq.connect(id.c_str(), u, p);
    if (!ok) { if (mqFails < 10) mqFails++; logLine("[MQTT] Connect failed, rc=" + String(mq.state()) + ", next try in " + String((mqFails == 0 ? 5UL : min(5UL << min((int)mqFails, 4), 60UL))) + " s"); return; }
    mqFails = 0;
    logLine("[MQTT] Connected to " + cfg.mqHost + ":" + String(cfg.mqPort));
    mqEver = true;
    pubT(P_STATUS, "online");
    lastHxPub = "";
    for (int i = 0; i < NSUB; i++) if (cfg.cEn[i]) mq.subscribe(cfg.cTopic[i].c_str());
    publishInfo();
    publishMeasure();
}

static bool mqWasConn = false;

void mqttLoop() {
    if (mqWasConn && !mq.connected()) logLine("[MQTT] Connection lost, state=" + String(mq.state()));
    mqWasConn = mq.connected();
    if (mq.connected()) {
        mq.loop();
        String hxs = hxState();
        if (hxs != "syncing") {
            String h = (hxs == "ok") ? "ok" : "error";
            if (h != lastHxPub) { pubT(P_HX, h); lastHxPub = h; if (h == "error") logLine(hxDetail() == "nocell" ? "[HX711] Chip answers but no valid reading - check load cells (E+, E-, A+, A-)" : "[HX711] No answer - check DT, SCK, VCC, GND"); }
        }
        unsigned long now = millis();
        bool due = cfg.pubInt > 0 && now - lastPub >= (unsigned long)cfg.pubInt * 1000UL;
        bool chg = cfg.thr > 0 && now - lastPub >= 1000 && fabs(curWeight() - lastPubW) >= cfg.thr;
        if (due || chg) publishMeasure();
        if (cfg.infoInt > 0 && now - lastInfo >= (unsigned long)cfg.infoInt * 1000UL) publishInfo();
    } else {
        mqttTry();
    }
}

void mqttReconnect() { if (mq.connected()) mq.disconnect(); mqLastTry = 0; mqFails = 0; }

// ------------------------------------------------------------------ WLAN
void wifiScanStart() {
    if (otaInProgress) return;
    WiFi.scanDelete();
    WiFi.scanNetworks(true, false, false, 300);
    logLine("[WiFi] Scanning for WiFi networks..");
}

int knownSlots() { int n = 0; for (int i = 0; i < MAX_WLAN; i++) if (cfg.wSsid[i].length()) n++; return n; }

bool wifiTry(int slot, int timeoutS) {
    logLine("[WiFi] Connect to: " + cfg.wSsid[slot]);
    logLine("[WiFi] Attempting connection with a timeout of " + String(timeoutS) + " seconds..");
    WiFi.begin(cfg.wSsid[slot].c_str(), cfg.wPass[slot].c_str());
    for (int i = 0; i < timeoutS * 4; i++) {
        esp_task_wdt_reset();
        if (WiFi.status() == WL_CONNECTED) return true;
        delay(250);
    }
    logLine("[WiFi] Connection failed or timed out");
    return false;
}

void wifiConnectBest() {
    int n = WiFi.scanComplete();
    int order[MAX_WLAN], score[MAX_WLAN], cnt = 0;
    for (int i = 0; i < MAX_WLAN; i++) {
        if (!cfg.wSsid[i].length()) continue;
        int rs = -999;
        for (int k = 0; k < n; k++) if (WiFi.SSID(k) == cfg.wSsid[i] && WiFi.RSSI(k) > rs) rs = WiFi.RSSI(k);
        order[cnt] = i; score[cnt] = rs; cnt++;
    }
    for (int a = 0; a < cnt; a++) for (int b = a + 1; b < cnt; b++)
        if (score[b] > score[a]) { int t = order[a]; order[a] = order[b]; order[b] = t; t = score[a]; score[a] = score[b]; score[b] = t; }
    for (int a = 0; a < cnt; a++) if (wifiTry(order[a], 15)) return;
}

void onWifiUp() {
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);
    logLine("[WiFi] Connected to: " + WiFi.SSID());
    logLine("[WiFi] IP address: " + WiFi.localIP().toString());
    if (apMode) { WiFi.softAPdisconnect(true); WiFi.mode(WIFI_STA); apMode = false; logLine("[WiFi] Setup access point stopped"); }
    if (!mdnsOn && MDNS.begin(cfg.host.c_str())) { MDNS.addService("http", "tcp", 80); mdnsOn = true; }
    ntpDirty = true;
    mqLastTry = 0;
}

void startAP() {
    String name = "Waage-" + macSuffix();
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(name.c_str());
    apMode = true;
    logLine("[WiFi] Setup access point '" + name + "' at " + WiFi.softAPIP().toString());
}

void wifiLoop() {
    bool conn = WiFi.status() == WL_CONNECTED;
    if (conn && !wasConn) { wasConn = true; onWifiUp(); }
    if (!conn && wasConn) { wasConn = false; logLine("[WiFi] Connection lost"); }
    if (!conn && millis() - wifiLastKick > 60000) {
        wifiLastKick = millis();
        if (knownSlots() == 0) return;
        for (int k = 0; k < MAX_WLAN; k++) {
            kickIdx = (kickIdx + 1) % MAX_WLAN;
            if (cfg.wSsid[kickIdx].length()) {
                logLine("[WiFi] Retry: " + cfg.wSsid[kickIdx]);
                WiFi.begin(cfg.wSsid[kickIdx].c_str(), cfg.wPass[kickIdx].c_str());
                break;
            }
        }
    }
    if (conn && ntpDirty) {
        ntpDirty = false;
        configTzTime(cfg.tz.c_str(), cfg.ntp1.c_str(), cfg.ntp2.length() ? cfg.ntp2.c_str() : nullptr);
        logLine("[NTP] Time sync started, server " + cfg.ntp1);
    }
}

// ------------------------------------------------------------------ Web: Bausteine
const char CSS[] PROGMEM = R"X(
:root{--bg:#10151a;--panel:#1a2129;--panel-border:#2a333c;--text:#e8edf2;--muted:#8f9ba7;--accent:#f5a623;--accent-dim:#7a530f;--ok:#3ddc84;--bad:#ff5c5c;--sans:system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;--mono:ui-monospace,"SFMono-Regular",Menlo,Consolas,monospace;}
body{font-family:Arial,Helvetica,sans-serif;text-align:center;margin:0;background:var(--bg);color:var(--text);}
main{padding:0 16px 40px;}
input,select,button{margin:10px;padding:10px;width:80%;box-sizing:border-box;background:#0d1216;color:var(--text);border:1px solid var(--panel-border);border-radius:6px;}
input[type=checkbox],input[type=radio]{width:auto;}
button,button[type=submit],input[type=submit]{background:var(--accent);color:#1a1200;font-weight:bold;border:none;cursor:pointer;}
button:hover,input[type=submit]:hover{background:#ffb84d;}
h1,h2,h3{color:var(--text);}
hr{border:0;height:1px;background-color:var(--panel-border);margin:20px 0;}
table{margin:auto;border-collapse:collapse;}
th,td{padding:8px;text-align:center;border:1px solid var(--panel-border);}
a{color:var(--accent);}
small{color:var(--muted);}
.topbar{display:flex;flex-wrap:wrap;align-items:center;gap:.6rem 1.2rem;padding:.9rem 1rem;border-bottom:1px solid var(--panel-border);position:sticky;top:0;background:var(--bg);z-index:5;}
.brand{display:flex;align-items:baseline;gap:.5rem;margin-right:auto;}
.brand-mark{font-family:var(--mono);color:var(--accent);font-size:.8rem;letter-spacing:.05em;}
.topbar h1{font-size:1.15rem;margin:0;letter-spacing:.02em;}
h1 a.host-link{color:inherit;text-decoration:none;border-bottom:1px dashed var(--muted);}
h1 a.host-link:hover{color:var(--accent);border-bottom-color:var(--accent);}
.ip-hint{font-family:var(--mono);font-size:.7rem;color:var(--muted);}
.ip-hint a{color:inherit;font-size:inherit;text-decoration:none;border-bottom:1px dashed var(--muted);}
.ip-hint a:hover{color:var(--accent);border-bottom-color:var(--accent);}
.status-strip{display:flex;gap:.9rem;flex-wrap:wrap;}
.status{display:flex;align-items:center;gap:.4rem;font-size:.8rem;color:var(--muted);}
.status[hidden]{display:none;}
.statval{font-family:var(--mono);color:var(--text);}
.dot{width:.55rem;height:.55rem;border-radius:50%;background:var(--bad);box-shadow:0 0 0 3px rgba(255,92,92,.15);}
.dot.ok{background:var(--ok);box-shadow:0 0 0 3px rgba(61,220,132,.18);}
.dot.na{background:var(--muted);box-shadow:0 0 0 3px rgba(143,155,167,.18);}
.dot.syncing{background:var(--accent);box-shadow:0 0 0 3px rgba(245,166,35,.18);animation:dotBlink 1s ease-in-out infinite;}
@keyframes dotBlink{0%,100%{opacity:1;}50%{opacity:.25;}}
@media (prefers-reduced-motion:reduce){.dot.syncing{animation:none;opacity:.6;}}
.datetime{font-family:var(--mono);font-size:.8rem;color:var(--muted);}
.offline-hint{display:none;color:var(--bad);font-size:.75rem;margin-left:.3rem;}
.offline-hint.show{display:inline;}
.offline-banner{display:none;background:#f8d7da;color:#721c24;border:1px solid #f5c6cb;border-radius:6px;padding:10px 14px;margin:12px auto 0;max-width:700px;font-weight:bold;}
.offline-banner.show{display:block;}
body.offline .big,body.offline .statval{opacity:.3;}
.refresh-btn{display:inline-flex;align-items:center;justify-content:center;text-decoration:none;background:transparent;border:1px solid var(--panel-border);color:var(--text);border-radius:.4rem;width:2rem;height:2rem;padding:0;margin:0;font-size:1rem;line-height:1;cursor:pointer;}
.refresh-btn:hover{background:transparent;border-color:var(--accent);color:var(--accent);}
.reset-btn{background:transparent;border:1px solid var(--panel-border);color:var(--bad);border-radius:.4rem;width:2rem;height:2rem;padding:0;margin:0;font-size:1rem;line-height:1;cursor:pointer;}
.reset-btn:hover{border-color:var(--bad);background:rgba(255,92,92,.12);}
.danger{background:var(--bad)!important;color:#fff!important;}
.tabctrl{display:none;}
.tabnav{margin:20px auto;max-width:900px;display:flex;flex-wrap:wrap;justify-content:center;gap:4px;}
.tabnav label{background:var(--panel);border:1px solid var(--panel-border);color:var(--muted);padding:8px 16px;border-radius:8px 8px 0 0;cursor:pointer;font-weight:bold;}
.tabpanel{display:none;}
.card{background:var(--panel);border:1px solid var(--panel-border);border-radius:10px;max-width:700px;margin:15px auto;padding:12px 16px;text-align:left;}
.card h3{margin:.2em 0 .6em;}
.grid{display:grid;grid-template-columns:minmax(120px,220px) 1fr;gap:8px 12px;align-items:center;}
.grid input,.grid select{width:100%;margin:0;}
.grid input[type=checkbox]{width:auto;}
.big{font-family:var(--mono);font-size:3rem;color:var(--accent);margin:.2em 0;}
.badge{background:var(--accent-dim);color:var(--text);border-radius:4px;padding:1px 6px;font-size:.7rem;margin-left:6px;}
.okbox{background:#d4edda;color:#155724;border:1px solid #c3e6cb;border-radius:6px;padding:8px 12px;margin:10px auto;max-width:500px;}
.errbox{background:#f8d7da;color:#721c24;border:1px solid #f5c6cb;border-radius:6px;padding:8px 12px;margin:10px auto;max-width:500px;}
pre.mono{background:var(--panel);border:1px solid var(--panel-border);border-radius:10px;max-width:900px;margin:15px auto;padding:12px 16px;text-align:left;white-space:pre-wrap;word-break:break-word;font-family:monospace;font-size:.85rem;}
td.last{font-family:var(--mono);font-size:.8rem;color:var(--muted);max-width:160px;overflow:hidden;text-overflow:ellipsis;}
.tabrow input[type=text]{width:100%;margin:0;}
)X";

void out(const String& s) {
    pg += s;
    if (pg.length() > 1200) { server.sendContent(pg); pg = ""; }
}
void outFlush() { if (pg.length()) { server.sendContent(pg); pg = ""; } }

void rowText(const String& label, const char* name, const String& val, const char* type = "text", const char* extra = "") {
    out("<label>" + label + "</label><input type='" + type + "' name='" + name + "' value='" + esc(val) + "' " + extra + ">");
}

void rowCheck(const char* label, const char* name, bool on) {
    out("<label>" + String(label) + "</label><span><input type='checkbox' name='" + name + "' value='1'" + (on ? " checked" : "") + "></span>");
}

void formOpen(const char* tab, const char* sect) {
    out("<form method='POST' action='/save'><input type='hidden' name='tab' value='" + String(tab) + "'><input type='hidden' name='sect' value='" + sect + "'>");
}
void formClose() { out("<button type='submit'>Speichern</button></form>"); }

void statusSpan(const char* key, const char* label, const String& st, bool present) {
    out("<span class='status' id='status-" + String(key) + "'" + (present ? "" : " hidden") + "><i id='dot-" + key + "' class='dot " + (st == "ok" ? "ok" : st == "syncing" ? "syncing" : st == "na" ? "na" : "") +
        "' role='img' title='" + (String(key) == "hx" ? hxTitleText(label, st) : dotText(label, st)) + "' aria-label='" + (String(key) == "hx" ? hxTitleText(label, st) : dotText(label, st)) + "'></i>" + label + "</span>");
}

void topbar() {
    out("<header class='topbar'><div class='brand'><span class='brand-mark'>WAAGE</span><h1><a class='host-link' href='http://" + esc(cfg.host) + ".local/'>" + esc(cfg.host) +
        "</a></h1><span class='ip-hint'><a href='http://" + ipText() + "/'>" + ipText() + "</a></span></div><div class='status-strip'>");
    // Reihenfolge: Messwert, Hardware/Verbindungen, Systemzeit zuletzt / order: reading, hardware/links, system time last
    out("<span class='status' id='status-weight'" + String(hxConfirmed ? "" : " hidden") + ">Gewicht: <span id='value-weight' class='statval'>" + esc(weightText()) + "</span></span>");
    statusSpan("hx", "HX711", hxState(), true);
    if (cfg.mqHost.length()) statusSpan("mq", "MQTT", mqState(), mqEver);
    statusSpan("time", "Zeit", timeState(), true);
    out("</div><div id='topbar-datetime' class='datetime'>" + dateTimeText() + "</div><span id='topbar-offline-hint' class='offline-hint'>&#9888; Verbindung verloren</span>");
    out("<a class='refresh-btn' href='/export' title='Einstellungen exportieren'>&#11123;</a>");
    out("<label class='refresh-btn' title='Einstellungen importieren' style='cursor:pointer'>&#11121;<input type='file' accept='.txt,text/plain' style='display:none' onchange='importSettings(this)'></label>");
    out("<button type='button' class='reset-btn' onclick=\"if(confirm('Waage jetzt neu starten?')){fetch('/restart',{method:'POST'});setTimeout(function(){location.reload();},5000);}\" title='Neu starten'>&#9211;</button>");
    out("</header><div id='offline-banner' class='offline-banner'>&#9888; Waage nicht erreichbar (offline) - angezeigt werden die zuletzt bekannten Werte.</div>");
}

const char TOPJS[] PROGMEM = R"X(<script>
(function(){
function dot(id,s,t){var e=document.getElementById(id);if(!e)return;e.classList.toggle('ok',s==='ok');e.classList.toggle('syncing',s==='syncing');e.classList.toggle('na',s==='na');if(t){e.title=t;e.setAttribute('aria-label',t);}}
function pres(id,p){var e=document.getElementById(id);if(e)e.hidden=!p;}
function val(id,t){var e=document.getElementById(id);if(e)e.textContent=t;}
var title0=document.title;
function online(ok){var h=document.getElementById('topbar-offline-hint');if(h)h.classList.toggle('show',!ok);var b=document.getElementById('offline-banner');if(b)b.classList.toggle('show',!ok);document.body.classList.toggle('offline',!ok);document.title=ok?title0:'[offline] '+title0;}
var fails=0;
window.waageFail=function(){fails++;if(fails>=2)online(false);};
window.waageOk=function(){fails=0;online(true);};
window.importSettings=function(inp){
 var f=inp.files[0];if(!f)return;
 if(!confirm('Einstellungen aus "'+f.name+'" importieren?\nBestehende Einstellungen werden ueberschrieben, Passwoerter bleiben unveraendert.')){inp.value='';return;}
 var r=new FileReader();
 r.onload=function(){
  fetch('/import',{method:'POST',headers:{'Content-Type':'text/plain'},body:r.result}).then(function(x){return x.text();}).then(function(t){
   alert(t.indexOf('OK|')===0?t.substring(3):t.indexOf('FAIL|')===0?t.substring(5):t);location.reload();
  }).catch(function(){alert('Import fehlgeschlagen');});
  inp.value='';
 };
 r.readAsText(f);
};
function poll(){fetch('/api/status',{cache:'no-store',signal:AbortSignal.timeout(4000)}).then(function(r){return r.json();}).then(function(s){
fails=0;online(true);
dot('dot-hx',s.hx,s.hxTitle);pres('status-hx',true);
dot('dot-mq',s.mq,s.mqTitle);pres('status-mq',s.mqPresent);
dot('dot-time',s.time,s.timeTitle);
pres('status-weight',s.weightPresent);val('value-weight',s.weightText);
val('topbar-datetime',s.datetime);
}).catch(function(){fails++;if(fails>=2)online(false);});}
var t=null;
function start(){if(t)return;poll();t=setInterval(poll,5000);}
function stop(){if(!t)return;clearInterval(t);t=null;}
document.addEventListener('visibilitychange',function(){if(document.hidden)stop();else start();});
if(!document.hidden)start();
})();
</script>)X";

// ------------------------------------------------------------------ Web: Tabs
const char* TABS[][2] = {
    {"waage", "Waage"}, {"messung", "Messung"}, {"mqtt", "MQTT senden"}, {"empfang", "MQTT empfangen"},
    {"wlan", "WLAN"}, {"system", "System"}, {"status", "Status"}, {"log", "Log"}, {"info", "Info"}, {"doku", "Doku"}
};
const int NTABS = 10;

void panelWaage() {
    out("<div class='tabpanel panel-waage'><div class='card' style='text-align:center'><div id='bigW' class='big'>" + esc(weightText()) + "</div>"
        "<div><i id='stDot' class='dot " + String(isStable() ? "ok" : "syncing") + "' style='display:inline-block'></i> <span id='stTxt'>" + (isStable() ? "stabil" : "unruhig") + "</span></div>"
        "<div id='hxHint' class='errbox' style='display:" + String(hxState() == "ok" ? "none" : "block") + "'>" + hxHintText() + "</div>"
        "<p><small>Rohwert (Mittel): <span id='rawV'>" + fmtNum(avgRaw, 0) + "</span> &middot; Offset: <span id='offV'>" + fmtNum(cfg.offset, 0) +
        "</span> &middot; Faktor: <span id='scaleV'>" + fmtNum(cfg.scale, 4) + "</span></small></p>"
        "<button type='button' onclick='act(\"/api/tare\",\"\",\"Tara jetzt ausf\u00fchren? Die Waage muss dabei leer sein.\")'>Tara (Nullstellen)</button></div>");
    out("<div class='card'><h3>Kalibrieren</h3><p><small>1. Waage leeren und Tara ausf&uuml;hren. 2. Bekanntes Gewicht auflegen, Wert eintragen, Kalibrieren dr&uuml;cken. Der Faktor wird dauerhaft gespeichert.</small></p>"
        "<div class='grid'><label>Bekanntes Gewicht (" + esc(cfg.unit) + ")</label><input type='text' id='calW' inputmode='decimal' placeholder='z. B. 5.0'></div>"
        "<div style='text-align:center'><button type='button' data-unit='" + esc(cfg.unit) + "' onclick='calib(this)'>Kalibrieren</button></div>"
        "<div id='calRes'></div></div></div>");
}

void panelMessung() {
    out("<div class='tabpanel panel-messung'><div class='card'><h3>HX711 / Wägezelle</h3>");
    formOpen("messung", "messung");
    out("<div class='grid'>");
    rowText("DOUT (DT) GPIO", "dout", String(cfg.dout), "number", "min='0' max='21'");
    rowText("SCK GPIO", "sck", String(cfg.sck), "number", "min='0' max='21'");
    out("<label>Verst&auml;rkung (Gain)</label><select name='gain'>");
    const int gs[3] = {128, 64, 32};
    for (int i = 0; i < 3; i++) out("<option value='" + String(gs[i]) + "'" + (cfg.gain == gs[i] ? " selected" : "") + ">" + String(gs[i]) + (gs[i] == 32 ? " (Kanal B)" : " (Kanal A)") + "</option>");
    out("</select>");
    rowText("Messwerte mitteln (1-" + String(MAX_RING) + ")", "navg", String(cfg.navg), "number", ("min='1' max='" + String(MAX_RING) + "'").c_str());
    rowText("Einheit", "unit", cfg.unit, "text", "maxlength='8'");
    rowText("Nachkommastellen (0-4)", "dec", String(cfg.dec), "number", "min='0' max='4'");
    rowText("Faktor (Counts je Einheit)", "scale", fmtNum(cfg.scale, 6));
    rowText("Offset (Counts)", "offset", fmtNum(cfg.offset, 0));

    // Ursprungswerte merken: Faktor und Offset werden nur uebernommen, wenn sie hier wirklich geaendert wurden
    // (Tara/Kalibrierung laufen ohne Seitenwechsel; sonst wuerde ein altes Formular sie ueberschreiben)
    // Remember the original values: scale and offset are only taken over if really edited here
    // (tare/calibration run without a page reload; otherwise an old form would overwrite them)

    out("<input type='hidden' name='scale_o' value='" + fmtNum(cfg.scale, 6) + "'><input type='hidden' name='offset_o' value='" + fmtNum(cfg.offset, 0) + "'>");
    rowText("Stabilit&auml;tsband (Einheit)", "band", fmtNum(cfg.band, 2));
    rowText("Plausibel von (Einheit)", "minw", fmtNum(cfg.minw, 2));
    rowText("Plausibel bis (Einheit)", "maxw", fmtNum(cfg.maxw, 2));
    out("</div><small>Plausibilit&auml;tsbereich: liegt das Gewicht au&szlig;erhalb, wird es nicht per MQTT gesendet. Von = Bis (z. B. 0/0) schaltet die Pr&uuml;fung ab. "
        "Das Stabilit&auml;tsband legt fest, wie stark die gemittelten Messwerte schwanken d&uuml;rfen, damit &bdquo;stabil&ldquo; gemeldet wird (0 = immer stabil). "
        "Pin- oder Gain-&Auml;nderungen wirken sofort.</small>");
    formClose();
    out("</div></div>");
}

void panelMqtt() {
    out("<div class='tabpanel panel-mqtt'><div class='card' style='max-width:900px'><h3>Broker</h3>");
    formOpen("mqtt", "mqtt");
    out("<div class='grid'>");
    rowText("Broker (leer = MQTT aus)", "mqhost", cfg.mqHost);
    rowText("Port", "mqport", String(cfg.mqPort), "number", "min='1' max='65535'");
    rowText("Benutzer", "mquser", cfg.mqUser);
    out("<label>Passwort</label><input type='password' name='mqpass' value='' autocomplete='new-password' placeholder='" + String(cfg.mqPass.length() ? "(gesetzt)" : "") + "'>");
    rowText("Client-ID (leer = Hostname)", "mqid", cfg.mqId);
    rowCheck("Retained senden", "retain", cfg.retain);
    rowText("Messwerte alle (s, 0 = nur bei &Auml;nderung)", "pubint", String(cfg.pubInt), "number", "min='0' max='65535'");
    rowText("Zus&auml;tzlich senden bei &Auml;nderung ab", "thr", fmtNum(cfg.thr, 2));
    rowText("Info-Topics alle (s, 0 = aus)", "infoint", String(cfg.infoInt), "number", "min='0' max='65535'");
    out("</div><small>Passwort wird nicht angezeigt. Leer lassen, um das aktuelle zu behalten.</small>");
    out("<h3>Gesendete Topics</h3><table class='tabrow'><tr><th>Senden</th><th>Inhalt</th><th>Topic</th><th>Letzter Wert</th></tr>");
    for (int i = 0; i < NPUB; i++) {
        out("<tr><td><input type='checkbox' name='pe" + String(i) + "' value='1'" + (cfg.pEn[i] ? " checked" : "") + "></td><td>" + PUB[i].label +
            "</td><td><input type='text' name='pt" + String(i) + "' value='" + esc(cfg.pTopic[i]) + "'></td><td class='last' title='" + esc(pLast[i]) + "'>" + esc(pLast[i]) + "</td></tr>");
    }
    out("</table><small>Leeres Topic = Standard <code>home/" + esc(cfg.host) + "/&hellip;</code>.</small>");
    formClose();
    out("</div></div>");
}

void panelEmpfang() {
    out("<div class='tabpanel panel-empfang'><div class='card' style='max-width:900px'><h3>Empfangene Topics (Befehle)</h3>");
    formOpen("empfang", "empfang");
    out("<table class='tabrow'><tr><th>Aktiv</th><th>Befehl</th><th>Topic</th><th>Letzter Wert</th></tr>");
    for (int i = 0; i < NSUB; i++) {
        out("<tr><td><input type='checkbox' name='ce" + String(i) + "' value='1'" + (cfg.cEn[i] ? " checked" : "") + "></td><td>" + SUB[i].label +
            "</td><td><input type='text' name='ct" + String(i) + "' value='" + esc(cfg.cTopic[i]) + "'></td><td class='last' title='" + esc(cLast[i]) + "'>" + esc(cLast[i]) + "</td></tr>");
    }
    out("</table><small>Tara und Neustart reagieren auf jede Nachricht. Kalibrieren erwartet das aufgelegte Gewicht als Zahl (z. B. 500).</small>");
    formClose();
    out("</div></div>");
}

void panelWlan() {
    out("<div class='tabpanel panel-wlan'><div class='card'><h3>WLAN und Hostname</h3>");
    formOpen("wlan", "wlan");
    out("<div class='grid'>");
    rowText("Hostname", "host", cfg.host, "text", "maxlength='31'");
    out("</div><small>Name im Netzwerk (http://Hostname.local/, OTA, MQTT-Standardtopics). Erlaubt: a-z, 0-9, Bindestrich. Wirkt erst nach einem Neustart.</small><hr><h3>Gespeicherte Netzwerke</h3>");
    String cur = WiFi.status() == WL_CONNECTED ? WiFi.SSID() : String("");
    for (int i = 0; i < MAX_WLAN; i++) {
        bool filled = cfg.wSsid[i].length() > 0;
        out("<div style='display:flex;gap:6px;align-items:center;flex-wrap:wrap;justify-content:center;'>");
        out("<select class='ssidsel' onchange=\"document.getElementById('ws" + String(i) + "').value=this.value\" style='max-width:180px;width:180px;margin:4px'></select>");
        out("<input name='ws" + String(i) + "' id='ws" + String(i) + "' placeholder='SSID' value='" + esc(cfg.wSsid[i]) + "' style='width:140px;margin:4px'>");
        out("<input name='wp" + String(i) + "' type='password' placeholder='Passwort' value='' autocomplete='new-password' style='width:140px;margin:4px'>");
        if (filled && cfg.wSsid[i] == cur) out("<span class='badge'>aktiv verbunden</span>");
        out("</div>");
        if (!filled) break;
    }
    out("<p><small>Passwort wird nicht angezeigt. Leer lassen, um das aktuelle zu behalten. SSID leeren = Eintrag l&ouml;schen. Reihenfolge ist frei; beim Start wird das st&auml;rkste bekannte Netz bevorzugt.</small></p>");
    out("<p><button type='button' onclick='scanNow()' style='width:auto'>Netze neu suchen</button> <small id='scanInfo'></small></p>");
    formClose();
    out("</div></div>");
}

void panelSystem() {
    const char* tzv[][2] = {
        {"CET-1CEST,M3.5.0,M10.5.0/3", "Mitteleuropa (Sommerzeit automatisch)"},
        {"CEST-2", "Mitteleuropa (feste Sommerzeit)"},
        {"CET-1", "Mitteleuropa (feste Winterzeit)"},
        {"GMT0BST,M3.5.0/1,M10.5.0", "Gro&szlig;britannien (Sommerzeit automatisch)"},
        {"EET-2EEST,M3.5.0/3,M10.5.0/4", "Osteuropa (Sommerzeit automatisch)"},
        {"UTC0", "UTC"}
    };
    out("<div class='tabpanel panel-system'><div class='card'><h3>Ger&auml;t &amp; Zeit</h3>");
    formOpen("system", "system");
    out("<div class='grid'>");
    rowText("NTP-Server 1", "ntp1", cfg.ntp1);
    rowText("NTP-Server 2", "ntp2", cfg.ntp2);
    out("<label>Zeitzone (Vorlage)</label><select onchange=\"document.getElementById('tzin').value=this.value\">");
    for (int i = 0; i < 6; i++) out("<option value='" + String(tzv[i][0]) + "'" + (cfg.tz == tzv[i][0] ? " selected" : "") + ">" + tzv[i][1] + "</option>");
    out("</select>");
    out("<label>Zeitzone (POSIX)</label><input type='text' id='tzin' name='tz' value='" + esc(cfg.tz) + "'>");
    out("</div>");
    formClose();
    out("<hr><form method='POST' action='/factory' onsubmit=\"return confirm('ALLE Einstellungen l&ouml;schen?') && confirm('Wirklich sicher? Das kann nicht r&uuml;ckg&auml;ngig gemacht werden.');\">"
        "<button type='submit' class='danger'>Werkseinstellungen</button></form>");
    out("</div></div>");
}

void stLine(const String& k, const String& v) { out(esc(k) + ": " + esc(v) + "\n"); }

void panelStatus() {
    out("<div class='tabpanel panel-status'><pre class='mono'>");
    stLine("HX711 Status", hxState() + (hxConfirmed ? "" : " (not confirmed)"));
    stLine("HX711 Reads", String(hxReads));
    stLine("HX711 Errors", String(hxErrors));
    stLine("HX711 Stuck-Low Count", String(hxStuck));
    stLine("HX711 Chip Reads", String(hxChipReads));
    stLine("HX711 Saturated Reads", String(hxSatReads));
    stLine("HX711 Detail", hxDetail());
    stLine("HX711 DOUT Level", digitalRead(cfg.dout) ? "HIGH (idle or not connected)" : "LOW (data ready or stuck low)");
    stLine("HX711 Last Raw", String(lastRaw));
    stLine("HX711 Average Raw", fmtNum(avgRaw, 1));
    stLine("HX711 Window Min/Max", String(winMin) + " / " + String(winMax));
    stLine("Calibration Offset", fmtNum(cfg.offset, 1));
    stLine("Calibration Scale", fmtNum(cfg.scale, 6));
    stLine("Weight", weightText());
    stLine("Stable", isStable() ? "yes" : "no");
    stLine("WiFi Status", WiFi.status() == WL_CONNECTED ? "connected" : (apMode ? "access point" : "disconnected"));
    stLine("WiFi SSID", WiFi.SSID());
    stLine("WiFi RSSI", String(WiFi.RSSI()) + " dBm");
    stLine("WiFi IP", ipText());
    stLine("MQTT Status", cfg.mqHost.length() ? (mq.connected() ? "connected" : "disconnected (rc=" + String(mq.state()) + ")") : "not configured");
    stLine("Time Status", timeValid() ? dateTimeText() : "not synced");
    stLine("Uptime", String(millis() / 1000) + " s");
    stLine("Free Heap", String(ESP.getFreeHeap()) + " bytes");
    stLine("Min Free Heap", String(ESP.getMinFreeHeap()) + " bytes");
    out("</pre></div>");
}

void panelLog() {
    out("<div class='tabpanel panel-log'><div style='display:flex;align-items:center;gap:14px;flex-wrap:wrap;justify-content:center'>"
        "<label style='display:flex;align-items:center;gap:6px;white-space:nowrap;cursor:pointer;'><input type='checkbox' id='logAuto' style='width:auto;margin:0;'>Auto-Aktualisierung (10 s)</label>"
        "<button type='button' id='logNow' style='background:var(--panel);border:1px solid var(--panel-border);color:var(--text);border-radius:.4rem;padding:4px 12px;font-size:.8rem;width:auto;margin:0'>Jetzt aktualisieren</button>"
        "<span id='logErr' class='offline-hint'>&#9888; Aktualisierung fehlgeschlagen</span></div>"
        "<pre id='logContent' class='mono' style='height:400px;overflow-y:auto;'>Loading&hellip;</pre></div>");
}

void panelInfo() {
    out("<div class='tabpanel panel-info'><pre class='mono'>");
    stLine("Chip Model", String(ESP.getChipModel()));
    stLine("Chip Revision", String(ESP.getChipRevision()));
    stLine("Chip Cores", String(ESP.getChipCores()));
    stLine("CPU Frequency", String(ESP.getCpuFreqMHz()) + " MHz");
    stLine("Flash Size", String(ESP.getFlashChipSize() / 1024) + " KB");
    stLine("SRAM Size", String(ESP.getHeapSize() / 1024) + " KB");
    stLine("PSRAM Size", String(ESP.getPsramSize() / 1024) + " KB");
    stLine("MAC Address", WiFi.macAddress());
    stLine("mDNS", cfg.host + ".local");
    stLine("SDK Version", String(ESP.getSdkVersion()));
    stLine("Sketch Size", String(ESP.getSketchSize() / 1024) + " KB");
    stLine("Firmware Version", String(FW_VERSION));
    stLine("Firmware Build", fwVersion());
    stLine("License", "GPL-3.0-only (details in the Doku tab)");
    stLine("Compiled", String(__DATE__) + " " + __TIME__);
    stLine("Reset Reason", String((int)esp_reset_reason()));
    stLine("OTA", "ArduinoOTA, host " + cfg.host + ", no password");
    stLine("HX711 DOUT GPIO", String(cfg.dout));
    stLine("HX711 SCK GPIO", String(cfg.sck));
    out("</pre></div>");
}

void panelDoku() {
    out("<div class='tabpanel panel-doku'><div class='card'>"
        "<h3>Waage</h3><p>Live-Gewicht, Tara und Kalibrierung. Tara setzt den aktuellen Wert auf 0. Zum Kalibrieren ein bekanntes Gewicht auflegen und dessen Wert eintragen.</p>"
        "<h3>Messung</h3><p>Pins, Verst&auml;rkung, Mittelung, Einheit, Nachkommastellen, Faktor/Offset (manuell &auml;nderbar), Stabilit&auml;tsband und Plausibilit&auml;tsbereich.</p>"
        "<h3>MQTT senden</h3><p>Broker-Zugang, Sendeintervall, Sofort-Senden bei &Auml;nderung und alle Topics. Jedes Topic ist einzeln abschaltbar und frei benennbar; die Spalte &bdquo;Letzter Wert&ldquo; zeigt, was zuletzt gesendet wurde. Der Broker l&auml;sst sich auch per API setzen, ohne das Formular: <code>curl -d \"broker=192.168.1.10&amp;port=1883\" http://&lt;IP&gt;/api/mqtt</code> (GET liefert Broker, Port und Verbindungsstatus).</p>"
        "<h3>MQTT empfangen</h3><p>Befehls-Topics f&uuml;r Tara, Kalibrieren und Neustart.</p>"
        "<h3>WLAN</h3><p>Hostname und bis zu " + String(MAX_WLAN) + " Netzwerke. Ohne Verbindung startet das Ger&auml;t den Zugangspunkt &bdquo;Waage-xxxxxx&ldquo; (Adresse 192.168.4.1).</p>"
        "<h3>System</h3><p>NTP-Server und Zeitzone. Werkseinstellungen l&ouml;schen alle Einstellungen.</p>"
        "<h3>Status / Log / Info</h3><p>Nur lesend, f&uuml;r Diagnose. Das Log liegt nur im Arbeitsspeicher und ist nach einem Neustart leer. Firmware-Update per Arduino-OTA (Hostname, kein Passwort).</p>"
        "<h3>Export / Import</h3><p>Die Pfeile oben rechts speichern alle Einstellungen ohne Passw&ouml;rter als Textdatei und lesen sie wieder ein. Faktor und Offset der Kalibrierung sind enthalten.</p>"
        "</div>");

    // Verkabelungsbild: die GPIO-Nummern im Bild sind die eingestellten Pins.
    // Wiring picture: the GPIO numbers in the picture are the configured pins.

    String svg = FPSTR(WIRING_SVG);
    svg.replace("GPIO 3 (DT)", "GPIO " + String(cfg.dout) + " (DT)");
    svg.replace("GPIO 4 (SCK)", "GPIO " + String(cfg.sck) + " (SCK)");
    out("<div class='card' style='max-width:900px'><h3>Verkabelung</h3><p>Anschl&uuml;sse von ESP32-C3, HX711 und den vier W&auml;gezellen. Die GPIO-Nummern im Bild sind die im Tab &bdquo;Messung&ldquo; eingestellten Pins.</p>");
    out("<div style='background:#fff;border-radius:8px;padding:6px;overflow-x:auto'>" + svg + "</div></div>");

    // Lizenz und Software von Dritten (Inhalt wie THIRD_PARTY_LICENSES.md im Quelltext)
    // License and third-party software (content as THIRD_PARTY_LICENSES.md in the source)

    out("<div class='card' style='max-width:900px'><h3>Lizenz</h3>"
        "<p>Die Waage-Firmware steht unter der <b>GNU General Public License v3.0</b>. Quelltext, Lizenztext und fertige Builds gibt es auf "
        "<a href='https://github.com/holgiw/ESP32-Scale' target='_blank' rel='noopener'>github.com/holgiw/ESP32-Scale</a>. "
        "Ver&auml;nderte Versionen d&uuml;rfen nur zusammen mit dem Quelltext und unter derselben Lizenz weitergegeben werden.</p>"
        "<h3>Software von Dritten</h3><p>Die Firmware enth&auml;lt Teile der folgenden Software. Die Lizenztexte stehen in <code>THIRD_PARTY_LICENSES.md</code> im Quelltext.</p>"
        "<table><tr><th>Software</th><th>Lizenz</th><th>Verwendung</th></tr>"
        "<tr><td>arduino-esp32 (WiFi, WebServer, ArduinoOTA, Preferences, ESPmDNS)</td><td>LGPL-2.1 oder neuer</td><td>Arduino-Core</td></tr>"
        "<tr><td>ESP-IDF (FreeRTOS, lwIP und weitere Komponenten)</td><td>Apache-2.0, teils MIT/BSD</td><td>Grundlage des Cores</td></tr>"
        "<tr><td>PubSubClient, &copy; Nicholas O'Leary</td><td>MIT</td><td>MQTT-Client</td></tr></table>"
        "<p><small>Weboberfl&auml;che, Verkabelungsplan und HX711-Treiber sind eigener Code ohne externe Bibliotheken, Schriften oder Bilder.</small></p></div></div>");
}

const char TABJS[] PROGMEM = R"X(<script>
(function(){
var m=location.search.match(/[?&]tab=([a-z]+)/);
if(m){var el=document.getElementById('tab-'+m[1]);if(el)el.checked=true;}
var fl=document.getElementById('flash');if(fl)setTimeout(function(){fl.style.display='none';},5000);
// Hinweise ohne Fehler nach 5 s ausblenden, Fehler bleiben stehen
// Hide notes that are no errors after 5 s, errors stay
var noteT=null;
function showNote(ok,text){var b=document.getElementById('calRes');b.innerHTML='<div class="'+(ok?'okbox':'errbox')+'">'+(ok?'&#10004; ':'&#10008; ')+String(text).replace(/&/g,'&amp;').replace(/</g,'&lt;')+'</div>';if(noteT){clearTimeout(noteT);noteT=null;}if(ok)noteT=setTimeout(function(){b.innerHTML='';},5000);}
window.calib=function(btn){
 var v=document.getElementById('calW').value;
 act('/api/calibrate','w='+encodeURIComponent(v),'Mit '+v+' '+btn.getAttribute('data-unit')+' kalibrieren?\nDas Gewicht muss jetzt auf der Waage liegen. Die bisherige Kalibrierung wird überschrieben.');
};
window.act=function(url,body,question){
 if(question&&!confirm(question))return;
 fetch(url,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body}).then(function(r){return r.text();}).then(function(t){
  showNote(t.indexOf('OK')===0,t.replace(/^(OK|FAIL)\|?/,''));

  pollW(true);
 }).catch(function(){});
};
var pending=false;
function pollW(force){
 var tab=document.getElementById('tab-waage');
 if(!force&&(!tab||!tab.checked||document.hidden))return;
 fetch('/api/status',{cache:'no-store',signal:AbortSignal.timeout(4000)}).then(function(r){return r.json();}).then(function(s){
  if(window.waageOk)window.waageOk();
  document.getElementById('bigW').textContent=s.weightText;
  document.getElementById('rawV').textContent=s.raw;
  document.getElementById('offV').textContent=s.offset;
  document.getElementById('scaleV').textContent=s.scale;
  var d=document.getElementById('stDot');d.classList.toggle('ok',s.stable);d.classList.toggle('syncing',!s.stable);
  document.getElementById('stTxt').textContent=s.stable?'stabil':'unruhig';
  var hh=document.getElementById('hxHint');hh.style.display=(s.hx==='ok')?'none':'block';if(s.hxHint)hh.innerHTML=s.hxHint;
  if(window._seenMsg===undefined){window._seenMsg=true;window._lastMsg=s.msg;}
  if(s.msg&&s.msg!==window._lastMsg&&(s.msg.indexOf('OK|')===0||s.msg.indexOf('FAIL|')===0)){
    window._lastMsg=s.msg;showNote(s.msg.indexOf('OK')===0,s.msg.replace(/^(OK|FAIL)\|/,''));
  }
 }).catch(function(){if(window.waageFail)window.waageFail();});
}
setInterval(pollW,1000);
// WLAN-Scan / WiFi scan
function fillScan(list){
 document.querySelectorAll('.ssidsel').forEach(function(sel){
  sel.innerHTML='<option value="">-- Netz w&auml;hlen --</option>';
  list.forEach(function(n){var o=document.createElement('option');o.value=n.ssid;o.textContent=n.ssid+' ('+n.rssi+' dBm) '+(n.open?'🔓':'🔒');sel.appendChild(o);});
 });
 var i=document.getElementById('scanInfo');if(i)i.textContent=list.length+' Netze gefunden';
}
function loadScan(){fetch('/api/scan').then(function(r){return r.json();}).then(function(d){if(d.scanning){setTimeout(loadScan,2000);}else fillScan(d.list);}).catch(function(){});}
window.scanNow=function(){fetch('/api/scan?start=1').then(function(){setTimeout(loadScan,2500);});};
loadScan();
// Log
var cb=document.getElementById('logAuto'),pre=document.getElementById('logContent'),er=document.getElementById('logErr'),btn=document.getElementById('logNow'),tm=null;
var st=null;try{st=localStorage.getItem('waageLogAuto');}catch(e){}
cb.checked=(st==='1');
function rl(){fetch('/log',{cache:'no-store'}).then(function(r){return r.text();}).then(function(t){er.classList.remove('show');pre.textContent=t;pre.scrollTop=pre.scrollHeight;}).catch(function(){er.classList.add('show');});}
function stopT(){if(tm){clearInterval(tm);tm=null;}}
function ap(){stopT();if(cb.checked&&!document.hidden)tm=setInterval(rl,10000);}
btn.addEventListener('click',rl);
cb.addEventListener('change',function(){try{localStorage.setItem('waageLogAuto',cb.checked?'1':'0');}catch(e){}if(cb.checked)rl();ap();});
document.addEventListener('visibilitychange',function(){if(document.hidden)stopT();else{if(cb.checked)rl();ap();}});
rl();ap();
})();
</script>)X";

void handleRoot() {
    server.sendHeader("Cache-Control", "no-store");
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");
    pg = "";
    out("<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>Waage " + esc(cfg.host) + "</title><style>");
    out(FPSTR(CSS));
    for (int i = 0; i < NTABS; i++) {
        out(String("#tab-") + TABS[i][0] + ":checked ~ .tabnav label[for='tab-" + TABS[i][0] + "']{background:var(--accent);color:#1a1200;border-color:var(--accent);}");
        out(String("#tab-") + TABS[i][0] + ":checked ~ .panel-" + TABS[i][0] + "{display:block;}");
    }
    out("</style></head><body>");
    topbar();
    out("<noscript><p class='errbox'>JavaScript wird f&uuml;r Teile dieser Seite ben&ouml;tigt.</p></noscript><main>");
    if (server.hasArg("msg")) {
        String m = server.arg("msg");
        String t = m == "saved" ? "Gespeichert" : m == "restart" ? "Gespeichert - Neustart n&ouml;tig (Hostname)" : "";
        if (t.length()) out("<div id='flash' class='okbox'>&#10004; " + t + "</div>");
    }
    for (int i = 0; i < NTABS; i++) out(String("<input type='radio' name='tabs' id='tab-") + TABS[i][0] + "' class='tabctrl'" + (i == 0 ? " checked" : "") + ">");
    out("<div class='tabnav'>");
    for (int i = 0; i < NTABS; i++) out(String("<label for='tab-") + TABS[i][0] + "'>" + TABS[i][1] + "</label>");
    out("</div>");
    panelWaage(); panelMessung(); panelMqtt(); panelEmpfang(); panelWlan(); panelSystem();
    panelStatus(); panelLog(); panelInfo(); panelDoku();
    out("</main>");
    out(FPSTR(TOPJS));
    out(FPSTR(TABJS));
    out("</body></html>");
    outFlush();
    server.sendContent("");
}

// ------------------------------------------------------------------ Web: API / Aktionen
void handleStatusApi() {
    server.sendHeader("Cache-Control", "no-store");
    String hx = hxState(), mqs = mqState(), tm = timeState();
    String j = "{\"version\":\"" + fwVersion() + "\",\"hx\":\"" + hx + "\",\"weightPresent\":" + (hxConfirmed ? "true" : "false") + ",\"hxTitle\":\"" + hxTitleText("HX711", hx) + "\",\"hxDetail\":\"" + hxDetail() + "\",\"hxHint\":\"" + hxHintText() + "\"";
    j += ",\"mq\":\"" + mqs + "\",\"mqPresent\":" + (mqEver ? "true" : "false") + ",\"mqTitle\":\"" + dotText("MQTT", mqs) + "\"";
    j += ",\"time\":\"" + tm + "\",\"timeTitle\":\"" + dotText("Zeit", tm) + "\"";
    j += ",\"weightText\":\"" + jesc(weightText()) + "\",\"datetime\":\"" + dateTimeText() + "\"";
    j += ",\"raw\":\"" + fmtNum(avgRaw, 0) + "\",\"offset\":\"" + fmtNum(cfg.offset, 0) + "\",\"scale\":\"" + fmtNum(cfg.scale, 4) + "\"";
    j += ",\"stable\":" + String(isStable() ? "true" : "false") + ",\"msg\":\"" + jesc(actionMsg) + "\"}";
    server.send(200, "application/json", j);
}

void handleScanApi() {
    server.sendHeader("Cache-Control", "no-store");
    if (server.hasArg("start")) { wifiScanStart(); server.send(200, "application/json", "{\"scanning\":true}"); return; }
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) { server.send(200, "application/json", "{\"scanning\":true}"); return; }
    if (n < 0) { wifiScanStart(); server.send(200, "application/json", "{\"scanning\":true}"); return; }
    String j = "{\"scanning\":false,\"list\":[";
    for (int i = 0; i < n && i < 20; i++) {
        if (i) j += ",";
        j += "{\"ssid\":\"" + jesc(WiFi.SSID(i)) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + ",\"open\":" + (WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "true" : "false") + "}";
    }
    j += "]}";
    server.send(200, "application/json", j);
}

// Schutz gegen fremde Webseiten (CSRF): ein Browser sendet bei POSTs von anderen Seiten einen abweichenden Origin
// Protection against other web pages (CSRF): a browser sends a different Origin with POSTs from other pages

bool originOk() {
    String o = server.header("Origin");
    if (!o.length()) return true;
    String h = server.hostHeader();
    return o == "http://" + h || o == "https://" + h;
}
#define NEED_SAME_ORIGIN() if (!originOk()) { server.send(403, "text/plain", "FAIL|Anfrage von fremder Seite abgelehnt"); return; }

void handleSave() {
    NEED_SAME_ORIGIN()
    String sect = server.arg("sect"), tab = server.arg("tab");
    bool hostChanged = false;
    if (sect == "messung") {
        if (server.hasArg("dout") && server.hasArg("sck")) {
            int d = server.arg("dout").toInt(), s = server.arg("sck").toInt();
            if (validPin(d) && validPin(s) && d != s) { cfg.dout = d; cfg.sck = s; }
        }
        int g = server.arg("gain").toInt(); cfg.gain = (g == 32 || g == 64) ? g : 128;
        cfg.navg = constrain((int)server.arg("navg").toInt(), 1, MAX_RING);
        String u = server.arg("unit"); u.trim(); cfg.unit = u.length() ? u : "kg";
        cfg.dec = constrain((int)server.arg("dec").toInt(), 0, 4);
        if (server.arg("scale") != server.arg("scale_o")) { double sc = parseNum(server.arg("scale"), cfg.scale); cfg.scale = (sc == 0) ? 1.0 : sc; }
        if (server.arg("offset") != server.arg("offset_o")) cfg.offset = parseNum(server.arg("offset"), cfg.offset);
        cfg.band = (float)parseNum(server.arg("band"), cfg.band);
        cfg.minw = (float)parseNum(server.arg("minw"), cfg.minw);
        cfg.maxw = (float)parseNum(server.arg("maxw"), cfg.maxw);
        hxBegin();
    } else if (sect == "mqtt") {
        cfg.mqHost = server.arg("mqhost"); cfg.mqHost.trim();
        int p = server.arg("mqport").toInt(); cfg.mqPort = (p > 0 && p < 65536) ? p : 1883;
        cfg.mqUser = server.arg("mquser");
        if (server.arg("mqpass").length()) cfg.mqPass = server.arg("mqpass");
        cfg.mqId = server.arg("mqid");
        cfg.retain = server.hasArg("retain");
        cfg.pubInt = constrain((int)server.arg("pubint").toInt(), 0, 65535);
        cfg.thr = (float)parseNum(server.arg("thr"), 0);
        cfg.infoInt = constrain((int)server.arg("infoint").toInt(), 0, 65535);
        for (int i = 0; i < NPUB; i++) { cfg.pEn[i] = server.hasArg("pe" + String(i)); cfg.pTopic[i] = server.arg("pt" + String(i)); }
        fixTopics();
        mqttReconnect();
    } else if (sect == "empfang") {
        for (int i = 0; i < NSUB; i++) { cfg.cEn[i] = server.hasArg("ce" + String(i)); cfg.cTopic[i] = server.arg("ct" + String(i)); }
        fixTopics();
        mqttReconnect();
    } else if (sect == "wlan") {
        String ns[MAX_WLAN], np[MAX_WLAN]; int c = 0;
        for (int i = 0; i < MAX_WLAN; i++) {
            String s = server.arg("ws" + String(i)); s.trim();
            if (!s.length()) continue;
            String p = server.arg("wp" + String(i));
            if (!p.length() && s == cfg.wSsid[i]) p = cfg.wPass[i];
            ns[c] = s; np[c] = p; c++;
        }
        for (int i = 0; i < MAX_WLAN; i++) { cfg.wSsid[i] = i < c ? ns[i] : ""; cfg.wPass[i] = i < c ? np[i] : ""; }

        // Hostname nur uebernehmen, wenn das Feld mitgesendet wurde (eine alte, noch offene Seite hat es nicht)
        // Take the host name only if the field was sent (an old page that is still open does not have it)

        if (server.hasArg("host")) {
            String h = cleanHost(server.arg("host"));
            hostChanged = (h != cfg.host);
            cfg.host = h;
        }
    } else if (sect == "system") {
        cfg.ntp1 = server.arg("ntp1"); cfg.ntp1.trim(); if (!cfg.ntp1.length()) cfg.ntp1 = "pool.ntp.org";
        cfg.ntp2 = server.arg("ntp2"); cfg.ntp2.trim();
        cfg.tz = server.arg("tz"); cfg.tz.trim(); if (!cfg.tz.length()) cfg.tz = "UTC0";
        ntpDirty = true;
    }
    saveConfig();
    logLine("[CFG] Saved section '" + sect + "'");
    if (!tab.length()) tab = "waage";
    server.sendHeader("Location", "/?tab=" + tab + "&msg=" + (hostChanged ? "restart" : "saved"));
    server.send(303);
}

void handleExport() {
    String t = "# Waage settings export (passwords omitted)\n";
    t += "host=" + cfg.host + "\n";
    for (int i = 0; i < MAX_WLAN; i++) if (cfg.wSsid[i].length()) t += "wlan" + String(i) + "_ssid=" + cfg.wSsid[i] + "\n";
    t += "mqtt_host=" + cfg.mqHost + "\nmqtt_port=" + String(cfg.mqPort) + "\nmqtt_user=" + cfg.mqUser + "\nmqtt_clientid=" + cfg.mqId + "\n";
    t += "mqtt_retain=" + String(cfg.retain) + "\npublish_interval_s=" + String(cfg.pubInt) + "\npublish_threshold=" + String(cfg.thr) + "\ninfo_interval_s=" + String(cfg.infoInt) + "\n";
    for (int i = 0; i < NPUB; i++) t += String("pub_") + PUB[i].key + "=" + (cfg.pEn[i] ? "on " : "off ") + cfg.pTopic[i] + "\n";
    for (int i = 0; i < NSUB; i++) t += String("sub_") + SUB[i].key + "=" + (cfg.cEn[i] ? "on " : "off ") + cfg.cTopic[i] + "\n";
    t += "hx711_dout=" + String(cfg.dout) + "\nhx711_sck=" + String(cfg.sck) + "\nhx711_gain=" + String(cfg.gain) + "\naverage=" + String(cfg.navg) + "\n";
    t += "unit=" + cfg.unit + "\ndecimals=" + String(cfg.dec) + "\nscale=" + fmtNum(cfg.scale, 6) + "\noffset=" + fmtNum(cfg.offset, 1) + "\n";
    t += "stable_band=" + String(cfg.band) + "\nrange_min=" + String(cfg.minw) + "\nrange_max=" + String(cfg.maxw) + "\n";
    t += "ntp1=" + cfg.ntp1 + "\nntp2=" + cfg.ntp2 + "\ntimezone=" + cfg.tz + "\n";
    server.sendHeader("Content-Disposition", "attachment; filename=\"waage-settings.txt\"");
    server.send(200, "text/plain", t);
}

// Einstellungen aus einer Exportdatei einlesen (key=value je Zeile). Passwoerter bleiben unveraendert,
// WLAN-Passwoerter nur bei gleicher SSID. Hostname gilt nach einem Neustart.
// Read settings from an export file (key=value per line). Passwords stay unchanged, WiFi passwords only
// for the same SSID. The host name applies after a restart.

void handleImport() {
    NEED_SAME_ORIGIN()
    String body = server.arg("plain");
    if (body.indexOf("Waage settings export") < 0) { server.send(200, "text/plain", "FAIL|Das ist keine Exportdatei der Waage."); return; }
    int n = 0;
    bool hostChanged = false, wlanSeen = false;
    String newSsid[MAX_WLAN]; int nw = 0;
    int impDout = cfg.dout, impSck = cfg.sck;
    bool haveDout = false, haveSck = false;
    int pos = 0;
    while (pos < (int)body.length()) {
        int e = body.indexOf('\n', pos);
        if (e < 0) e = body.length();
        String line = body.substring(pos, e);
        pos = e + 1;
        line.trim();
        if (!line.length() || line[0] == '#') continue;
        int eq = line.indexOf('=');
        if (eq < 1) continue;
        String k = line.substring(0, eq), v = line.substring(eq + 1);
        k.trim(); v.trim();
        bool ok = true;
        if (k == "host") { String h = cleanHost(v); hostChanged = (h != cfg.host); cfg.host = h; }
        else if (k.startsWith("wlan") && k.endsWith("_ssid")) { wlanSeen = true; if (nw < MAX_WLAN && v.length()) newSsid[nw++] = v; }
        else if (k == "mqtt_host") cfg.mqHost = v;
        else if (k == "mqtt_port") { int p = v.toInt(); cfg.mqPort = (p > 0 && p < 65536) ? p : 1883; }
        else if (k == "mqtt_user") cfg.mqUser = v;
        else if (k == "mqtt_clientid") cfg.mqId = v;
        else if (k == "mqtt_retain") cfg.retain = (v == "1");
        else if (k == "publish_interval_s") cfg.pubInt = constrain((int)v.toInt(), 0, 65535);
        else if (k == "publish_threshold") cfg.thr = (float)parseNum(v, 0);
        else if (k == "info_interval_s") cfg.infoInt = constrain((int)v.toInt(), 0, 65535);
        else if (k.startsWith("pub_") || k.startsWith("sub_")) {
            bool pub = k.startsWith("pub_");
            String key = k.substring(4);
            int sp = v.indexOf(' ');
            bool en = v.startsWith("on");
            String topic = sp >= 0 ? v.substring(sp + 1) : String("");
            topic.trim();
            ok = false;
            if (pub) { for (int i = 0; i < NPUB; i++) if (key == PUB[i].key) { cfg.pEn[i] = en; cfg.pTopic[i] = topic; ok = true; } }
            else { for (int i = 0; i < NSUB; i++) if (key == SUB[i].key) { cfg.cEn[i] = en; cfg.cTopic[i] = topic; ok = true; } }
        }
        else if (k == "hx711_dout") { impDout = v.toInt(); haveDout = true; }
        else if (k == "hx711_sck") { impSck = v.toInt(); haveSck = true; }
        else if (k == "hx711_gain") { int g = v.toInt(); cfg.gain = (g == 32 || g == 64) ? g : 128; }
        else if (k == "average") cfg.navg = constrain((int)v.toInt(), 1, MAX_RING);
        else if (k == "unit") cfg.unit = v.length() ? v : String("kg");
        else if (k == "decimals") cfg.dec = constrain((int)v.toInt(), 0, 4);
        else if (k == "scale") { double sc = parseNum(v, 1.0); cfg.scale = (sc == 0) ? 1.0 : sc; }
        else if (k == "offset") cfg.offset = parseNum(v, 0);
        else if (k == "stable_band") cfg.band = (float)parseNum(v, cfg.band);
        else if (k == "range_min") cfg.minw = (float)parseNum(v, cfg.minw);
        else if (k == "range_max") cfg.maxw = (float)parseNum(v, cfg.maxw);
        else if (k == "ntp1") cfg.ntp1 = v.length() ? v : String("pool.ntp.org");
        else if (k == "ntp2") cfg.ntp2 = v;
        else if (k == "timezone") cfg.tz = v.length() ? v : String("UTC0");
        else ok = false;
        if (ok) n++;
    }

    // Beide Pins gemeinsam pruefen, damit auch ein Tausch von DT und SCK moeglich ist
    // Check both pins together so swapping DT and SCK is possible

    if ((haveDout || haveSck) && validPin(impDout) && validPin(impSck) && impDout != impSck) { cfg.dout = impDout; cfg.sck = impSck; }
    if (wlanSeen) {
        String oldS[MAX_WLAN], oldP[MAX_WLAN];
        for (int i = 0; i < MAX_WLAN; i++) { oldS[i] = cfg.wSsid[i]; oldP[i] = cfg.wPass[i]; cfg.wSsid[i] = ""; cfg.wPass[i] = ""; }
        for (int i = 0; i < nw; i++) {
            cfg.wSsid[i] = newSsid[i];
            for (int j = 0; j < MAX_WLAN; j++) if (oldS[j] == newSsid[i]) cfg.wPass[i] = oldP[j];
        }
    }
    fixTopics();
    saveConfig();
    hxBegin();
    mqttReconnect();
    ntpDirty = true;
    logLine("[CFG] Imported " + String(n) + " settings");
    server.send(200, "text/plain", "OK|" + String(n) + " Werte importiert." + (hostChanged ? " Der Hostname gilt erst nach einem Neustart." : ""));
}

// HTTP-API: MQTT-Broker und Port von aussen lesen und setzen, ohne das ganze Einstellungsformular zu senden.
//   GET  /api/mqtt                           -> {"broker":"...","port":1883,"connected":true}
//   POST /api/mqtt  broker=<host> [port=<n>]  -> setzt nur die genannten Werte, Antwort wie GET
// HTTP API: read and set the MQTT broker and port from outside without sending the whole settings form.

String apiMqttJson() {
    return "{\"broker\":\"" + jesc(cfg.mqHost) + "\",\"port\":" + String(cfg.mqPort) + ",\"connected\":" + (mq.connected() ? "true" : "false") + "}";
}

void handleApiMqtt() {
    server.sendHeader("Cache-Control", "no-store");
    if (server.method() == HTTP_POST) {
        NEED_SAME_ORIGIN()
        bool setB = server.hasArg("broker"), setP = server.hasArg("port");
        String b = server.arg("broker"); b.trim();
        String ps = server.arg("port"); ps.trim();
        if (setB && (!b.length() || b.length() > 63)) { server.send(400, "application/json", "{\"error\":\"broker invalid\"}"); return; }
        long p = ps.toInt();
        bool digits = ps.length() > 0;
        for (size_t i = 0; i < ps.length(); i++) if (ps[i] < '0' || ps[i] > '9') digits = false;
        if (setP && (!digits || p < 1 || p > 65535)) { server.send(400, "application/json", "{\"error\":\"port invalid\"}"); return; }
        if (setB) cfg.mqHost = b;
        if (setP) cfg.mqPort = (uint16_t)p;
        if (setB || setP) {

            // Nur diese beiden Schluessel schreiben, gleiche Schluessel und Typen wie das Formular
            // Write only these two keys, same keys and types as the form

            prefs.begin("waage", false);
            prefs.putString("mqhost", cfg.mqHost);
            prefs.putUShort("mqport", cfg.mqPort);
            prefs.end();
            logLine("[MQTT] Broker set via API: " + cfg.mqHost + ":" + String(cfg.mqPort));
            if (!apMode) mqttReconnect();
        }
    }
    server.send(200, "application/json", apiMqttJson());
}

void setupRoutes() {
    static const char* hdrKeys[] = { "Origin" };
    server.collectHeaders(hdrKeys, 1);
    server.on("/api/mqtt", handleApiMqtt);
    server.on("/import", HTTP_POST, handleImport);
    server.on("/", HTTP_GET, handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/api/status", HTTP_GET, handleStatusApi);
    server.on("/api/topbarStatus", HTTP_GET, handleStatusApi);
    server.on("/api/scan", HTTP_GET, handleScanApi);
    server.on("/api/tare", HTTP_POST, []() { NEED_SAME_ORIGIN() doTare(); server.send(200, "text/plain", "OK|Tara gestartet"); });
    server.on("/api/calibrate", HTTP_POST, []() {
        NEED_SAME_ORIGIN()
        double w = parseNum(server.arg("w"), 0);
        if (w <= 0) { server.send(200, "text/plain", "FAIL|Bitte ein Gewicht gr\xC3\xB6\xC3\x9F" "er 0 eingeben"); return; }
        doCal(w); server.send(200, "text/plain", "OK|Kalibrierung gestartet");
    });
    server.on("/log", HTTP_GET, []() { server.sendHeader("Cache-Control", "no-store"); server.send(200, "text/plain; charset=utf-8", logBuf); });
    server.on("/export", HTTP_GET, handleExport);
    server.on("/restart", HTTP_POST, []() { NEED_SAME_ORIGIN() server.send(200, "text/plain", "OK"); restartAt = millis() + 500; });
    server.on("/factory", HTTP_POST, []() {
        NEED_SAME_ORIGIN()
        prefs.begin("waage", false); prefs.clear(); prefs.end();
        server.send(200, "text/html", "<html><body style='background:#10151a;color:#e8edf2;font-family:sans-serif;text-align:center'><h2>Zur&uuml;ckgesetzt - Neustart ...</h2><script>setTimeout(function(){location.href='/';},6000)</script></body></html>");
        restartAt = millis() + 800;
    });
    server.on("/favicon.ico", HTTP_GET, []() {
        server.sendHeader("Cache-Control", "public, max-age=86400");
        server.send(200, "image/svg+xml",
            "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'><circle cx='16' cy='16' r='14' fill='#10151a' stroke='#f5a623' stroke-width='2.5'/>"
            "<line x1='16' y1='16' x2='16' y2='7' stroke='#f5a623' stroke-width='2.5' stroke-linecap='round'/><line x1='16' y1='16' x2='21' y2='16' stroke='#f5a623' stroke-width='2.5' stroke-linecap='round'/></svg>");
    });
    server.onNotFound([]() { server.sendHeader("Location", "/"); server.send(302); });
}

// ------------------------------------------------------------------ OTA
void setupOta() {
    ArduinoOTA.setHostname(cfg.host.c_str());
    ArduinoOTA.onStart([]() { otaInProgress = true; otaStartMillis = millis(); logLine("[OTA] Start"); });
    ArduinoOTA.onEnd([]() { otaInProgress = false; logLine("[OTA] End"); });
    ArduinoOTA.onProgress([](unsigned int p, unsigned int t) { esp_task_wdt_reset(); });
    ArduinoOTA.onError([](ota_error_t e) { otaInProgress = false; logLine("[OTA] Error " + String((int)e)); });
    ArduinoOTA.begin();
}

// ------------------------------------------------------------------ USB-Einrichtung
// Befehle von flashESP.bat ueber USB: WAAGE INFO | WAAGE WIFI <ssid-hex> <pass-hex> | WAAGE RESTART
// Commands from flashESP.bat via USB: WAAGE INFO | WAAGE WIFI <ssid-hex> <pass-hex> | WAAGE RESTART

String hexDecode(const String& h) {
    String o;
    for (size_t i = 0; i + 1 < h.length(); i += 2) o += (char)strtol(h.substring(i, i + 2).c_str(), nullptr, 16);
    return o;
}

void handleSerialLine(String line) {
    line.trim();
    if (!line.startsWith("WAAGE ")) return;
    if (line == "WAAGE INFO") {
        Serial.println("WAAGE OK INFO " + cfg.host);
    } else if (line.startsWith("WAAGE WIFI ")) {
        int sp = line.indexOf(' ', 11);
        String ssid = hexDecode(sp < 0 ? line.substring(11) : line.substring(11, sp));
        String pass = sp < 0 ? String("") : hexDecode(line.substring(sp + 1));
        if (!ssid.length() || ssid.length() > 32 || pass.length() > 63) { Serial.println("WAAGE ERROR WIFI"); return; }

        // Netz an Position 0, gleiche SSID wird ersetzt
        // network goes to slot 0, same SSID is replaced

        String ns[MAX_WLAN], np[MAX_WLAN]; int c = 1;
        ns[0] = ssid; np[0] = pass;
        for (int i = 0; i < MAX_WLAN && c < MAX_WLAN; i++) {
            if (cfg.wSsid[i].length() && cfg.wSsid[i] != ssid) { ns[c] = cfg.wSsid[i]; np[c] = cfg.wPass[i]; c++; }
        }
        for (int i = 0; i < MAX_WLAN; i++) { cfg.wSsid[i] = ns[i]; cfg.wPass[i] = np[i]; }
        saveConfig();
        logLine("[CFG] WiFi '" + ssid + "' set via USB");
        Serial.println("WAAGE OK WIFI");
    } else if (line == "WAAGE RESTART") {
        Serial.println("WAAGE OK RESTART");
        restartAt = millis() + 1000;
    } else {
        Serial.println("WAAGE ERROR UNKNOWN");
    }
}

void serialLoop() {
    static String buf;
    while (Serial.available()) {
        char ch = (char)Serial.read();
        if (ch == '\n') { handleSerialLine(buf); buf = ""; }
        else if (buf.length() < 300) buf += ch;
        else buf = "";
    }
}

// ------------------------------------------------------------------ Arduino
void setup() {

    // Takt 80 MHz reicht fuer WLAN, HX711 und Web-UI und spart Strom (unabhaengig von der Board-Einstellung)
    // 80 MHz is enough for WiFi, HX711 and web UI and saves power (independent of the board setting)

    setCpuFrequencyMhz(80);
    Serial.begin(115200);
    delay(300);
    esp_task_wdt_config_t twdt = { .timeout_ms = 20000, .idle_core_mask = 0, .trigger_panic = true };
    esp_task_wdt_reconfigure(&twdt);
    esp_task_wdt_add(NULL);

    loadConfig();
    logLine("[BOOT] Waage " + String(FW_VERSION) + " host " + cfg.host);
    hxBegin();

    WiFi.persistent(false);
    WiFi.setHostname(cfg.host.c_str());
    WiFi.mode(WIFI_STA);
    wifiScanStart();
    for (int i = 0; i < 100 && WiFi.scanComplete() == WIFI_SCAN_RUNNING; i++) { esp_task_wdt_reset(); delay(100); }
    int n = WiFi.scanComplete();
    logLine("[WiFi] found " + String(n < 0 ? 0 : n) + " WiFi networks");
    wifiConnectBest();
    if (WiFi.status() != WL_CONNECTED) startAP();

    wifiClient.setConnectionTimeout(1500);
    mq.setBufferSize(512);
    mq.setSocketTimeout(3);
    mq.setCallback(mqttCallback);

    setupRoutes();
    server.begin();
    setupOta();
    logLine("[HTTP] Web server started");
}

void loop() {
    esp_task_wdt_reset();
    ArduinoOTA.handle();
    if (otaInProgress) {
        if (millis() - otaStartMillis > OTA_TIMEOUT_MS) ESP.restart();
        return;
    }
    server.handleClient();
    serialLoop();
    hxLoop();
    hxPendingTimeout();
    wifiLoop();
    mqttLoop();
    if (restartAt && millis() > restartAt) { delay(100); ESP.restart(); }
    delay(2);
}
