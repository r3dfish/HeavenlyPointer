// ============================================================================
//  WebControl.cpp
// ============================================================================
#include "WebControl.h"
#include "web_assets.h"
#include "config.h"
#include "Settings.h"
#include "Status.h"
#include "Sky.h"
#include "ServoHead.h"
#include "ServoSetup.h"
#include "Compass.h"
#include "Net.h"
#include <M5Unified.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <time.h>
#include <math.h>

namespace WebControl {
namespace {

WebServer server(80);
bool      s_started = false;

void sendJson(int code, const String& body) {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(code, "application/json", body);
}

String minToHHMM(int m) {
    m = ((m % 1440) + 1440) % 1440;
    char b[6]; sprintf(b, "%02d:%02d", m / 60, m % 60); return String(b);
}
int hhmmToMin(const String& s) {
    int c = s.indexOf(':');
    if (c < 0) return 0;
    int h = s.substring(0, c).toInt(), mn = s.substring(c + 1).toInt();
    if (h < 0) h = 0; if (h > 23) h = 23;
    if (mn < 0) mn = 0; if (mn > 59) mn = 59;
    return h * 60 + mn;
}

// ---- GET / : the control page (served straight from flash) ----------------
void handleIndex() {
    server.send_P(200, "text/html", INDEX_HTML);
}

// ---- favicon (SVG) : keeps the tab icon nice + avoids 302-ing /favicon.ico -
void handleFavicon() {
    server.sendHeader("Cache-Control", "max-age=86400");
    server.send_P(200, "image/svg+xml", FAVICON_SVG);
}

// ---- GET /status.json : live tracking telemetry ---------------------------
void handleStatus() {
    JsonDocument d;
    d["tracking"] = g_status.tracking;
    d["name"]     = g_status.name;
    d["az"]       = g_status.az;
    d["el"]       = g_status.el;
    d["rangeKm"]  = g_status.rangeKm;
    d["altKm"]    = g_status.altKm;
    d["velKms"]   = g_status.velKms;
    d["subLat"]   = g_status.subLat;
    d["subLon"]   = g_status.subLon;
    d["vis"]      = g_status.vis;
    d["visText"]  = Sky::visText(g_status.vis);
    d["pan"]      = g_status.pan;
    d["tilt"]     = g_status.tilt;
    d["inRange"]  = g_status.inRange;
    d["satCount"] = g_status.satCount;
    d["group"]    = settings.tleGroup;
    d["ip"]       = WiFi.localIP().toString();

    char iso[40] = "--";
    if (g_status.utc > 0) {                       // local time for display
        time_t local = g_status.utc + (time_t)settings.tzOffsetMin * 60;
        struct tm tmv; gmtime_r(&local, &tmv);
        int  ao = settings.tzOffsetMin < 0 ? -settings.tzOffsetMin : settings.tzOffsetMin;
        char sg = settings.tzOffsetMin < 0 ? '-' : '+';
        snprintf(iso, sizeof(iso), "%04d-%02d-%02d %02d:%02d:%02d UTC%c%d:%02d",
                 tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
                 tmv.tm_hour, tmv.tm_min, tmv.tm_sec, sg, ao / 60, ao % 60);
    }
    d["iso"] = iso;
    d["tzOffsetMin"] = settings.tzOffsetMin;

    d["sleeping"]  = g_sleepNow;
    d["testMode"]  = g_testMode;
    d["lowBattery"]= g_lowBattery;
    d["stalled"]   = g_stalled;
    d["battLevel"] = M5.Power.getBatteryLevel();
    d["charging"]  = (M5.Power.isCharging() != m5::Power_Class::is_discharging);
    if (g_testMode) {   // actual bus read-back, for the motor-test diagnostic
        d["panCmd"]  = ServoHead::panAngle();
        d["tiltCmd"] = ServoHead::tiltAngle();
        d["panAct"]  = ServoHead::readPan();
        d["tiltAct"] = ServoHead::readTilt();
    }
    d["geoStatus"] = g_geoStatus;
    d["geoCity"]   = g_geoCity;
    d["lat"]       = settings.lat;
    d["lon"]       = settings.lon;

    // Heading / compass live state
    d["headSource"]  = settings.headingSource;     // 0 manual, 1 auto
    d["headAuto"]    = g_heading.autoActive;        // auto source AND good fix in use
    d["magPresent"]  = g_heading.magPresent;
    d["headTrue"]    = g_heading.forwardTrue;       // applied forward bearing (deg true)
    d["headMag"]     = g_heading.magHeading;        // last magnetometer heading (deg)
    d["declination"] = g_heading.declination;
    d["headTilt"]    = g_heading.tilt;
    d["headQual"]    = g_heading.quality;
    d["hasNext"]     = g_status.hasNext;       // a reachable pass to count down to
    d["nextInReach"] = g_status.nextInReach;
    d["nextName"]    = g_status.nextName;
    if (g_status.hasNext) {
        time_t n = Net::nowUtc();
        long secs = (n > 0) ? (long)(g_status.nextAos - n) : 0;
        d["nextInSec"] = secs < 0 ? 0 : secs;
    }

    time_t now = Net::nowUtc();
    long ageMin = (now > 0 && settings.tleFetchedAt > 0 && now >= (time_t)settings.tleFetchedAt)
                  ? (long)((now - settings.tleFetchedAt) / 60) : -1;
    d["tleAgeMin"] = ageMin;

    String out; serializeJson(d, out);
    sendJson(200, out);
}

// ---- GET /config.json : current saved configuration -----------------------
void handleConfigJson() {
    JsonDocument d;
    d["group"]        = settings.tleGroup;
    d["filter"]       = settings.filter;
    d["minElevation"] = settings.minElevation;
    d["maxElevation"] = settings.maxElevation;
    d["panLimit"]     = settings.panLimit;
    d["orbitClass"]   = settings.orbitClass;
    d["ledBars"]      = settings.ledBars;
    d["sleepSchedule"]= settings.sleepSchedule;
    d["sleepStart"]   = minToHHMM(settings.sleepStartMin);
    d["sleepEnd"]     = minToHHMM(settings.sleepEndMin);
    d["facing"]       = facingName(settings.facing);
    d["headingSource"]    = settings.headingSource;
    d["manualHeading"]    = settings.manualHeadingDeg;
    d["headingOffset"]    = settings.headingOffsetDeg;
    d["declination"]      = settings.declinationDeg;
    d["declinationManual"]= settings.declinationManual;
    d["magPresent"]       = Compass::available();
    d["magCalDone"]       = settings.magCalDone;
    d["hasLocation"]  = settings.hasLocation;
    d["lat"]          = settings.lat;
    d["lon"]          = settings.lon;
    d["tzOffsetHours"]= settings.tzOffsetMin / 60.0;
    d["satCount"]     = Sky::count();
    d["version"]      = FW_VERSION;
    String out; serializeJson(d, out);
    sendJson(200, out);
}

// ---- POST /config : apply edited configuration ----------------------------
void handleConfigPost() {
    bool groupChanged = false;

    if (server.hasArg("group")) {
        String g = server.arg("group"); g.trim();
        if (g.length() && g != settings.tleGroup) { settings.tleGroup = g; groupChanged = true; }
    }
    if (server.hasArg("filter")) {
        settings.filter = server.arg("filter"); settings.filter.trim();
        Sky::setFilter(settings.filter);
    }
    if (server.hasArg("minel")) {
        double m = server.arg("minel").toDouble();
        if (m < 0)  m = 0;
        if (m > 85) m = 85;
        settings.minElevation = m;
        Sky::setMinElevation((float)m);
    }
    if (server.hasArg("maxel")) {
        double m = server.arg("maxel").toDouble();
        if (m < 5)  m = 5;
        if (m > 90) m = 90;
        settings.maxElevation = m;
    }
    if (server.hasArg("panlim")) {
        double p = server.arg("panlim").toDouble();
        if (p < 10)  p = 10;
        if (p > 120) p = 120;
        settings.panLimit = p;
    }
    if (server.hasArg("tzoff")) {
        String z = server.arg("tzoff"); z.trim();
        if (z.length()) {
            int newOff = (int)lroundf(z.toFloat() * 60.0f);
            if (newOff != settings.tzOffsetMin) {   // only pin if actually changed
                settings.tzOffsetMin = newOff;
                settings.tzManual    = true;
            }
        }
    }
    if (server.hasArg("orbit")) {
        int oc = server.arg("orbit").toInt();
        if (oc < 0) oc = 0;
        if (oc > 2) oc = 2;
        settings.orbitClass = (uint8_t)oc;
        Sky::setOrbitClass(settings.orbitClass);
    }
    if (server.hasArg("leds")) {
        String s = server.arg("leds");
        settings.ledBars = (s == "1" || s == "true" || s == "on");
    }
    if (server.hasArg("sleepsched")) {
        String s = server.arg("sleepsched");
        settings.sleepSchedule = (s == "1" || s == "true" || s == "on");
    }
    if (server.hasArg("sleepstart")) settings.sleepStartMin = hhmmToMin(server.arg("sleepstart"));
    if (server.hasArg("sleepend"))   settings.sleepEndMin   = hhmmToMin(server.arg("sleepend"));
    // The form re-submits every heading field on each save, so flag a heading
    // re-apply ONLY when a value actually changes - otherwise an unrelated edit
    // would trigger a blocking ~200ms magnetometer re-read in AUTO mode.
    bool headingChanged = false;
    if (server.hasArg("facing")) {
        String fc = server.arg("facing"); fc.toUpperCase();
        Facing nf = settings.facing;
        if      (fc.startsWith("N")) nf = FACE_NORTH;
        else if (fc.startsWith("E")) nf = FACE_EAST;
        else if (fc.startsWith("S")) nf = FACE_SOUTH;
        else if (fc.startsWith("W")) nf = FACE_WEST;
        if (nf != settings.facing || !settings.hasFacing) headingChanged = true;
        settings.facing = nf; settings.hasFacing = true;
    }
    if (server.hasArg("hsrc")) {
        uint8_t ns = (server.arg("hsrc").toInt() == 1) ? HEADING_AUTO : HEADING_MANUAL;
        if (ns != settings.headingSource) { settings.headingSource = ns; headingChanged = true; }
    }
    if (server.hasArg("mhead")) {              // precise manual bearing; blank = use N/E/S/W
        String v = server.arg("mhead"); v.trim();
        float nh = -1.0f;
        if (v.length()) { nh = v.toFloat(); while (nh < 0) nh += 360.0f; while (nh >= 360) nh -= 360.0f; }
        if (fabsf(nh - settings.manualHeadingDeg) > 0.01f) { settings.manualHeadingDeg = nh; headingChanged = true; }
    }
    if (server.hasArg("hoff")) {               // magnetometer mounting trim
        float o = server.arg("hoff").toFloat();
        while (o < -180) o += 360.0f; while (o > 180) o -= 360.0f;
        if (fabsf(o - settings.headingOffsetDeg) > 0.01f) { settings.headingOffsetDeg = o; headingChanged = true; }
    }
    if (server.hasArg("declauto")) {           // checkbox: auto-compute declination from WMM
        String s = server.arg("declauto");
        bool man = !(s == "1" || s == "true" || s == "on");
        if (man != settings.declinationManual) { settings.declinationManual = man; headingChanged = true; }
    }
    // Honor a typed declination only in manual mode (the field is auto-filled +
    // disabled when auto is on, so we must not let its value flip us to manual).
    if (server.hasArg("decl") && settings.declinationManual) {
        String v = server.arg("decl"); v.trim();
        if (v.length()) {
            float d = v.toFloat();
            if (fabsf(d - settings.declinationDeg) > 0.01f) { settings.declinationDeg = d; headingChanged = true; }
        }
    }
    if (server.hasArg("lat") && server.hasArg("lon")) {
        String la = server.arg("lat"), lo = server.arg("lon");
        la.trim(); lo.trim();
        if (la.length() && lo.length()) {
            settings.lat = la.toDouble();
            settings.lon = lo.toDouble();
            settings.hasLocation = true;
            Sky::setSite(settings.lat, settings.lon, settings.altM);
        }
    }

    // Guard against inverted limits (else nothing is ever reachable).
    if (settings.minElevation > settings.maxElevation) {
        double t = settings.minElevation;
        settings.minElevation = settings.maxElevation;
        settings.maxElevation = t;
    }
    // Push the reach limits to the head (used for clamping AND by Sky's
    // reachability test, so selection only picks satellites it can point at).
    ServoHead::setLimits(settings.panLimit, settings.minElevation, settings.maxElevation);

    settings.save();
    if (groupChanged)    g_tleRefetchNeeded = true;   // heavy refetch deferred to trackTick
    if (headingChanged)  g_applyHeadingNeeded = true;  // re-point "forward" in the main loop
    sendJson(200, "{\"ok\":true}");
}

// ---- POST /test : direct servo command (motor test mode only) -------------
void handleTest() {
    if (g_testMode) {
        float pan  = server.hasArg("pan")  ? server.arg("pan").toFloat()  : ServoHead::panAngle();
        float tilt = server.hasArg("tilt") ? server.arg("tilt").toFloat() : ServoHead::tiltAngle();
        ServoHead::testMove(pan, tilt);
        g_lastTestCmdMs = millis();          // reset the test-mode auto-relax timer
    }
    sendJson(200, "{\"ok\":true}");
}

// ---- POST /action : one-shot commands -------------------------------------
void handleAction() {
    String cmd = server.arg("cmd");
    if      (cmd == "refetch")  g_tleRefetchNeeded = true;
    else if (cmd == "relocate") g_relocateNeeded   = true;
    else if (cmd == "park")     ServoHead::park();
    else if (cmd == "next")     Sky::selectNext();
    else if (cmd == "prev")     Sky::selectPrev();
    else if (cmd == "sleep")    g_sleepNow = true;
    else if (cmd == "wake")     g_sleepNow = false;
    else if (cmd == "test")     { g_testMode = true; g_sleepNow = false; g_lastTestCmdMs = millis(); }
    else if (cmd == "testexit") g_testMode = false;
    else if (cmd == "recover")  ServoHead::recover();
    else if (cmd == "calmag")   g_calibrateMagNeeded = true;   // run compass calibration
    else if (cmd == "applyhead")g_applyHeadingNeeded = true;   // re-read heading now
    else if (cmd == "servosetup") { sendJson(200, "{\"ok\":true}"); ServoSetup::requestAndReboot(); return; }
    sendJson(200, "{\"ok\":true}");
}

// ---- unknown path -> bounce to the control page ---------------------------
void handleNotFound() {
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "");
}

} // anonymous namespace

void begin() {
    if (s_started) return;
    server.on("/",            HTTP_GET,  handleIndex);
    server.on("/favicon.svg", HTTP_GET,  handleFavicon);
    server.on("/favicon.ico", HTTP_GET,  handleFavicon);   // serve the SVG here too
    server.on("/status.json", HTTP_GET,  handleStatus);
    server.on("/config.json", HTTP_GET,  handleConfigJson);
    server.on("/config",      HTTP_POST, handleConfigPost);
    server.on("/action",      HTTP_POST, handleAction);
    server.on("/test",        HTTP_POST, handleTest);
    server.onNotFound(handleNotFound);
    server.begin();
    s_started = true;
}

void handle() { if (s_started) server.handleClient(); }
bool started() { return s_started; }

} // namespace WebControl
