// ============================================================================
//  HeavenlyPointer  -  Stack-chan satellite tracker
//
//  Flow:
//    BOOT -> (no creds) PROVISION -> CONNECT -> FETCH (time+location+TLEs)
//         -> (no facing) CALIBRATE -> TRACK  (forever)
//
//  Once TLEs + time are cached the tracker keeps working even if WiFi drops:
//  SGP4 propagation is fully local. WiFi is only needed to refresh the
//  catalog and re-sync the clock.
// ============================================================================
#include <M5Unified.h>
#include <M5StackChan.h>
#include <LittleFS.h>
#include <time.h>

#include "config.h"
#include "Settings.h"
#include "Net.h"
#include "Provision.h"
#include "Sky.h"
#include "ServoHead.h"
#include "UI.h"
#include "Status.h"
#include "WebControl.h"

enum State { ST_PROVISION, ST_CONNECT, ST_FETCH, ST_CALIBRATE, ST_TRACK, ST_SLEEP };
static State state;

// Shared live state + web-UI request flags (declared extern in Status.h).
LiveStatus g_status;
bool       g_tleRefetchNeeded = false;
bool       g_relocateNeeded   = false;
bool       g_sleepNow         = false;
bool       g_testMode         = false;
int        g_geoStatus        = GEO_NONE;
String     g_geoCity;

static int  s_lastLed = -1;    // last RGB-bar state pushed (-1 = force refresh)
static bool s_forceRedraw = false;   // request an immediate HUD redraw (e.g. on wake)
// Idle servo-power state: when no satellite is reachable we home the head once
// then cut torque (s_idleRelaxed) so the tilt motor isn't holding against
// gravity. Reset to 0 forces a fresh park->relax cycle on the next idle tick.
static uint32_t s_idleSince   = 0;
static bool     s_idleRelaxed = false;

// --------------------------------------------------------------------------
static uint32_t tleAgeSeconds(time_t now) {
    if (now == 0 || settings.tleFetchedAt == 0) return UINT32_MAX;
    if (now < (time_t)settings.tleFetchedAt)     return UINT32_MAX;
    return (uint32_t)(now - settings.tleFetchedAt);
}

// Sync time + location, (re)download TLEs if missing/stale, load the catalog.
static void doFetch() {
    UI::status("Syncing UTC time...", TFT_YELLOW);
    Net::syncTime();

    // Learn the location the first time, and refresh the local-time offset on
    // every boot (unless the user pinned it by hand) so the clock auto-localises
    // even when the location was already saved from a previous run.
    if (!settings.hasLocation || !settings.tzManual) {
        bool needLoc = !settings.hasLocation;
        if (needLoc) UI::status("Finding location...", TFT_YELLOW);
        double la, lo, al; int off = 0; String city;
        if (Net::geolocate(la, lo, al, off, city)) {
            if (needLoc) {
                settings.lat = la; settings.lon = lo; settings.altM = al;
                settings.hasLocation = true;
            }
            if (!settings.tzManual) settings.tzOffsetMin = off / 60;
            settings.save();
            g_geoStatus = GEO_OK; g_geoCity = city;
        } else {
            g_geoStatus = GEO_FAIL;
        }
    }
    Sky::setSite(settings.lat, settings.lon, settings.altM);
    Sky::setMinElevation(settings.minElevation);
    Sky::setFilter(settings.filter);
    Sky::setOrbitClass(settings.orbitClass);
    ServoHead::setLimits(settings.panLimit, settings.minElevation, settings.maxElevation);

    time_t now = Net::nowUtc();
    bool stale = !LittleFS.exists(TLE_FILE_PATH) ||
                 tleAgeSeconds(now) > TLE_MAX_AGE_HOURS * 3600UL;
    if (stale) {
        UI::status("Downloading", TFT_YELLOW, "satellite data...");
        int n = Net::fetchTLEs(settings.tleGroup);
        if (n > 0 && now != 0) { settings.tleFetchedAt = now; settings.save(); }
    }

    int loaded = Sky::load();
    UI::status((String(loaded) + " satellites loaded").c_str(),
               loaded > 0 ? TFT_GREEN : TFT_RED);
    delay(1000);

    // WiFi is up here - bring up the browser control server.
    WebControl::begin();
}

// Re-download the TLE catalog for the current group, then reload + re-apply
// tracking prefs. Blocking (a few seconds); the head holds position meanwhile.
static void refetchCatalog(time_t now) {
    if (!Net::isConnected()) return;
    UI::status("Updating catalog...", TFT_YELLOW);
    int n = Net::fetchTLEs(settings.tleGroup);
    if (n > 0 && now != 0) { settings.tleFetchedAt = now; settings.save(); }
    Sky::load();
    Sky::setMinElevation(settings.minElevation);
    Sky::setFilter(settings.filter);
    Sky::setOrbitClass(settings.orbitClass);
}

// One tracking iteration (called continuously while in ST_TRACK).
static void trackTick() {
    static uint32_t lastTrack = 0, lastServo = 0, lastFetchAttempt = 0, nextPassMs = 0;
    static Sky::NextPass nextPass;

    if (g_testMode) {                        // manual motor test - tracking paused
        static uint32_t lastTestDraw = 0;
        uint32_t tms = millis();
        if (tms - lastTestDraw > 250) {
            lastTestDraw = tms;
            UI::drawTest(ServoHead::panAngle(), ServoHead::tiltAngle(),
                         ServoHead::readPan(), ServoHead::readTilt());
        }
        g_status.tracking = false;
        s_idleSince = 0;                     // on test exit, re-run the park->relax cycle
        return;                              // servos are driven by the web /test handler
    }

    time_t now = Net::nowUtc();

    if (now == 0) {                       // clock not set yet -> try NTP
        UI::status("Waiting for time...", TFT_YELLOW);
        if (Net::isConnected()) Net::syncTime();
        delay(500);
        return;
    }

    uint32_t ms = millis();

    // --- requests from the web UI -----------------------------------------
    if (g_relocateNeeded) {
        g_relocateNeeded = false;
        double la, lo, al; int off = 0; String city;
        if (Net::isConnected() && Net::geolocate(la, lo, al, off, city)) {
            settings.lat = la; settings.lon = lo; settings.altM = al;
            settings.hasLocation = true;
            settings.tzOffsetMin = off / 60;
            settings.tzManual = false;          // auto-locate re-enables auto tz
            settings.save();
            Sky::setSite(la, lo, al);
            g_geoStatus = GEO_OK; g_geoCity = city;
        } else {
            g_geoStatus = GEO_FAIL;
        }
    }
    if (g_tleRefetchNeeded) {
        g_tleRefetchNeeded = false;
        refetchCatalog(now);
        lastFetchAttempt = ms;
    }
    // Opportunistic refresh when the catalog is stale - throttled to every
    // 5 min so a failing download doesn't hammer Celestrak (or block the loop)
    // on every iteration.
    else if (Net::isConnected() && tleAgeSeconds(now) > TLE_MAX_AGE_HOURS * 3600UL
             && ms - lastFetchAttempt > 300000UL) {
        refetchCatalog(now);
        lastFetchAttempt = ms;
    }

    // Recompute the tracking solution at TRACK_INTERVAL_MS (or right after wake).
    if (s_forceRedraw || ms - lastTrack >= TRACK_INTERVAL_MS) {
        s_forceRedraw = false;
        lastTrack = ms;
        Target t = Sky::update(now);

        g_status.satCount = Sky::count();
        g_status.utc      = now;

        if (t.valid) {                         // valid => reachable (within az/el limits)
            ServoHead::aimAt((float)t.az, (float)t.el);
            s_idleSince       = 0;             // left idle: re-home/relax on next gap
            g_status.tracking = true;
            g_status.hasNext  = false;
            nextPass.valid    = false;         // recompute next time we're parked
            strncpy(g_status.name, t.name, sizeof(g_status.name) - 1);
            g_status.name[sizeof(g_status.name) - 1] = 0;
            g_status.az = t.az; g_status.el = t.el;
            g_status.rangeKm = t.rangeKm; g_status.altKm = t.altKm; g_status.velKms = t.velKms;
            g_status.subLat = t.subLat; g_status.subLon = t.subLon; g_status.vis = t.vis;
            UI::drawTracking(t, ServoHead::panAngle(), ServoHead::tiltAngle(),
                             ServoHead::inRange(), now, g_status.satCount);
        } else {
            // Idle: home the head ONCE, then de-energize the servos. Re-issuing
            // park() every second kept the tilt motor holding the head up
            // against gravity continuously - on a sagging battery that constant
            // current cooked the motor. Letting it go limp removes that load.
            if (s_idleSince == 0)    { s_idleSince = ms; s_idleRelaxed = false; ServoHead::park(); }
            else if (!s_idleRelaxed && ms - s_idleSince > 2500) { ServoHead::relax(); s_idleRelaxed = true; }
            g_status.tracking = false;
            g_status.name[0] = 0;
            // Predict the next pass occasionally; just count down in between.
            if (!nextPass.valid || now >= nextPass.aosUnix || ms - nextPassMs > 60000UL) {
                nextPass = Sky::computeNextPass(now);
                nextPassMs = ms;
            }
            g_status.hasNext = nextPass.valid;
            if (nextPass.valid) {
                strncpy(g_status.nextName, nextPass.name, sizeof(g_status.nextName) - 1);
                g_status.nextName[sizeof(g_status.nextName) - 1] = 0;
                g_status.nextAos = nextPass.aosUnix;
            }
            UI::drawSearching(now, g_status.satCount, nextPass);
        }
        // Servo readout reflects the command just issued (aim or park).
        g_status.pan     = ServoHead::panAngle();
        g_status.tilt    = ServoHead::tiltAngle();
        g_status.inRange = ServoHead::inRange();

        // RGB bars: green while tracking, red while waiting (off if disabled).
        // Only push to the LEDs when the state changes.
        int wantLed = !settings.ledBars ? 0 : (g_status.tracking ? 1 : 2);
        if (wantLed != s_lastLed) {
            s_lastLed = wantLed;
            if      (wantLed == 1) M5StackChan.showRgbColor(0, 80, 0);   // green
            else if (wantLed == 2) M5StackChan.showRgbColor(90, 0, 0);   // red
            else                   M5StackChan.showRgbColor(0, 0, 0);    // off
        }
    }

    // Smooth the servos faster than the 1 Hz solution.
    if (ms - lastServo >= 20) { lastServo = ms; ServoHead::tick(); }
}

// --- Sleep / quiet hours --------------------------------------------------
static void enterSleep() {
    ServoHead::relax();                      // cut servo torque - no holding current while asleep
    M5StackChan.showRgbColor(0, 0, 0);       // LEDs off
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setBrightness(0);             // screen off
    g_status.tracking = false;
    g_status.hasNext  = false;
    s_lastLed = 0;
    state = ST_SLEEP;
}

static void exitSleep() {
    M5.Display.setBrightness(SCREEN_BRIGHTNESS);
    s_lastLed = -1;                          // force LED refresh
    s_forceRedraw = true;                    // redraw the HUD immediately, not in up to 1 s
    s_idleSince = 0;                          // re-home once on wake, then relax if still idle
    state = ST_TRACK;
}

// Flip g_sleepNow at the edges of the configured local-time window. The manual
// SLEEP/WAKE button writes the same flag, so a manual wake holds until the next
// window edge, when the schedule takes over again.
static void updateSleepSchedule(time_t now) {
    static bool prevIn = false, first = true;
    if (now == 0) return;

    bool in = false;
    if (settings.sleepSchedule && settings.sleepStartMin != settings.sleepEndMin) {
        long lsec = (long)((now + (time_t)settings.tzOffsetMin * 60) % 86400);
        if (lsec < 0) lsec += 86400;
        int m = (int)(lsec / 60);
        int s = settings.sleepStartMin, e = settings.sleepEndMin;
        in = (s < e) ? (m >= s && m < e) : (m >= s || m < e);   // handles midnight wrap
    }

    if (first)            { if (settings.sleepSchedule) g_sleepNow = in; first = false; }
    else if (in != prevIn){ g_sleepNow = in; }
    prevIn = in;
}

// --------------------------------------------------------------------------
void setup() {
    // Brings up M5Unified AND the Stack-chan body: powers the servo rail via the
    // IO expander, initialises the serial-bus servos, touch panel and RGB LEDs.
    M5StackChan.begin();
    UI::begin();
    UI::splash("Heavenly", "Pointer", "satellite tracker");

    if (!LittleFS.begin(true)) {           // format on first boot if needed
        UI::status("Filesystem error", TFT_RED);
        delay(2000);
    }

    settings.load();
    ServoHead::begin();                    // parks the head
    if (settings.hasFacing) ServoHead::setFacing(settings.facing);
    ServoHead::setLimits(settings.panLimit, settings.minElevation, settings.maxElevation);

    delay(700);
    state = settings.hasWifi() ? ST_CONNECT : ST_PROVISION;
}

void loop() {
    M5.update();
    WebControl::handle();          // serve browser requests (no-op until started)

    // Hold the bottom button ~3 s to wipe config & re-provision.
    if (M5.BtnA.pressedFor(3000)) {
        UI::status("Factory reset...", TFT_RED);
        settings.factoryReset();
        LittleFS.remove(TLE_FILE_PATH);
        delay(800);
        ESP.restart();
    }

    // Schedule drives auto-sleep at the window edges (not while motor-testing).
    if (!g_testMode && (state == ST_TRACK || state == ST_SLEEP)) updateSleepSchedule(Net::nowUtc());

    // Touch: browse prev/next while tracking; any tap wakes from sleep.
    if (state == ST_TRACK && !g_testMode && M5.Touch.getCount()) {
        auto d = M5.Touch.getDetail();
        if (d.wasPressed() && d.y < 44) {
            if (d.x < 70)       Sky::selectPrev();
            else if (d.x > 250) Sky::selectNext();
        }
    } else if (state == ST_SLEEP && M5.Touch.getCount()) {
        auto d = M5.Touch.getDetail();
        if (d.wasPressed()) g_sleepNow = false;     // tap the dark screen to wake
    }

    switch (state) {
        case ST_PROVISION:
            Provision::run();              // blocks until creds saved
            state = ST_CONNECT;
            break;

        case ST_CONNECT:
            UI::status("Connecting to WiFi...", TFT_YELLOW);
            if (Net::connect(settings.ssid, settings.pass)) {
                state = ST_FETCH;
            } else {
                UI::status("WiFi failed - re-setup", TFT_RED);
                delay(1500);
                settings.clearWifi();
                state = ST_PROVISION;
            }
            break;

        case ST_FETCH:
            doFetch();
            state = settings.hasFacing ? ST_TRACK : ST_CALIBRATE;
            break;

        case ST_CALIBRATE: {
            Facing f = UI::askFacing();
            settings.facing = f; settings.hasFacing = true; settings.save();
            ServoHead::setFacing(f);
            state = ST_TRACK;
            break;
        }

        case ST_TRACK:
            if (!g_testMode && g_sleepNow) enterSleep();   // test mode overrides sleep
            else                           trackTick();     // handles tracking or test screen
            break;

        case ST_SLEEP:
            // Stay dark/parked; the web server keeps running so WAKE NOW works.
            if (!g_sleepNow) exitSleep();
            break;
    }
}
