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
#include "ServoSetup.h"
#include "Compass.h"
#include "Wmm.h"
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
uint32_t   g_lastTestCmdMs    = 0;
bool       g_lowBattery       = false;
bool       g_stalled          = false;
bool       g_calibrateMagNeeded = false;
bool       g_applyHeadingNeeded = false;
int        g_geoStatus        = GEO_NONE;
String     g_geoCity;
HeadingState g_heading;

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

// Recompute & cache magnetic declination for the current location/date, unless
// the user pinned it by hand. Offline (embedded WMM2025). Cheap.
static void refreshDeclination(time_t now) {
    if (settings.declinationManual || !settings.hasLocation) return;
    settings.declinationDeg =
        Wmm::declination((float)settings.lat, (float)settings.lon, 0.0f, Wmm::decimalYear(now));
}

// Set the head's forward bearing from the current heading settings. In AUTO
// mode this reads the magnetometer with the servos relaxed and the LEDs off
// (both sit next to the sensor), adds the WMM declination, and falls back to
// manual if the magnetometer read is poor. Updates g_heading for the web page.
static void applyForwardBearing() {
    g_heading.magPresent  = Compass::available();
    g_heading.declination = settings.declinationDeg;

    bool used = false;
    if (settings.headingSource == HEADING_AUTO && settings.magCalDone && Compass::available()) {
        ServoHead::relax();                  // silence the pan servo
        M5StackChan.showRgbColor(0, 0, 0);   // and the LEDs, both next to the sensor
        delay(120);
        float mh = 0, tilt = 0, q = 0;
        if (Compass::readHeading(&mh, &tilt, &q) && q >= 40.0f) {
            ServoHead::setForwardBearing(mh + settings.declinationDeg);  // magnetic -> true
            g_heading.autoActive  = true;
            g_heading.magHeading  = mh;
            g_heading.tilt        = tilt;
            g_heading.quality     = q;
            used = true;
        } else {
            g_heading.tilt = tilt; g_heading.quality = q;   // record why we fell back
        }
        s_lastLed = -1; s_forceRedraw = true;   // restore LEDs/HUD next tick
    }
    if (!used) {
        g_heading.autoActive = false;
        float b = (settings.manualHeadingDeg >= 0.0f) ? settings.manualHeadingDeg
                                                       : facingBearing(settings.facing);
        ServoHead::setForwardBearing(b);
    }
    g_heading.forwardTrue = ServoHead::forwardBearing();
}

// Interactive magnetometer calibration: relax the head, cut the LEDs, and ask
// the user to slowly rotate the device through a full turn while M5Unified's
// sphere fit accumulates. Persists offsets to NVS and re-applies the heading.
static void runCompassCalibration() {
    if (!Compass::available()) { UI::status("No magnetometer", TFT_RED); delay(1200); return; }
    ServoHead::relax();
    M5StackChan.showRgbColor(0, 0, 0);
    s_lastLed = -1;
    Compass::startCalibration();
    uint32_t start = millis();
    for (;;) {
        M5.update();
        WebControl::handle();                  // keep the browser responsive
        Compass::updateCalibration();
        float p = Compass::calProgress();
        UI::drawCompassCal(p);
        uint32_t el = millis() - start;
        if (M5.Touch.getCount()) { auto d = M5.Touch.getDetail(); if (d.wasPressed() && el > 2000) break; }
        if ((p >= (float)COMPASS_CAL_SECTORS / 12.0f && el > 6000) || el > 30000) break;
        delay(15);
    }
    // Only commit if the figure-8 actually covered enough of the sky. Otherwise
    // DON'T persist the half-baked sphere-fit (it would mispoint every pass) -
    // discard it, restore the prior offsets, and leave magCalDone untouched.
    bool ok = Compass::calProgress() >= (float)COMPASS_CAL_SECTORS / 12.0f;
    if (ok) {
        Compass::finishCalibration();
        settings.magCalDone = true;
        settings.save();
        UI::status("Compass calibrated", TFT_GREEN);
    } else {
        Compass::cancelCalibration();
        UI::status("Calibration incomplete", TFT_RED, "sweep further & retry");
    }
    delay(1200);
    applyForwardBearing();
    s_forceRedraw = true;
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

    // Now that location + date are known, refresh declination and point the
    // head's "forward" using the chosen heading source.
    refreshDeclination(now);
    settings.save();
    applyForwardBearing();

    // WiFi is up here - bring up the browser control server.
    WebControl::begin();
}

// Can we track right now WITHOUT WiFi? Needs a cached TLE catalog, a known
// location, and a valid clock (restored from the battery-backed RTC on boot).
static bool offlineCapable() {
    return LittleFS.exists(TLE_FILE_PATH) && settings.hasLocation && Net::nowUtc() > 0;
}

// Offline start: use the cached TLEs + saved location + RTC clock, no WiFi.
// Returns false if there's nothing usable on disk (caller falls back online).
static bool doFetchOffline() {
    if (!LittleFS.exists(TLE_FILE_PATH)) return false;
    UI::status("Offline mode", TFT_CYAN, "using cached data");
    Sky::setSite(settings.lat, settings.lon, settings.altM);
    Sky::setMinElevation(settings.minElevation);
    Sky::setFilter(settings.filter);
    Sky::setOrbitClass(settings.orbitClass);
    ServoHead::setLimits(settings.panLimit, settings.minElevation, settings.maxElevation);
    int loaded = Sky::load();
    if (loaded == 0) {
        // File exists but is empty/corrupt: quarantine it so offlineCapable() stops
        // offering Offline mode (otherwise the button would just bounce us back here).
        LittleFS.remove(TLE_FILE_PATH);
        UI::status("Cached data unusable", TFT_RED, "connect to refresh");
        delay(1500);
        return false;
    }
    time_t now = Net::nowUtc();
    refreshDeclination(now);
    applyForwardBearing();
    UI::status((String(loaded) + " sats (offline)").c_str(), TFT_GREEN);
    delay(1200);
    return true;
    // No web server offline - there's no shared network to reach it on.
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
    static uint32_t lastPowerCheck = 0, s_stallSince = 0, s_cooldownStart = 0;
    static bool     s_stallCooldown = false;
    static float    s_prevActP = 0, s_prevActT = 0;
    static int      s_warnScreen = 0;            // 0 none, 1 low-batt, 2 stall (draw-once dedup)
    static Sky::NextPass nextPass;

    if (g_testMode) {                        // manual motor test - tracking paused
        static uint32_t lastTestDraw = 0;
        uint32_t tms = millis();
        if (tms - lastTestDraw > 250) {
            lastTestDraw = tms;
            UI::drawTest(ServoHead::panAngle(), ServoHead::tiltAngle(),
                         ServoHead::readPan(), ServoHead::readTilt());
        }
        // Guard A: if you leave test mode holding a pose and walk away, cut the
        // torque after a while so a servo isn't held against gravity forever.
        static uint32_t lastSeenCmd = 0; static bool testRelaxed = false;
        if (g_lastTestCmdMs != lastSeenCmd) { lastSeenCmd = g_lastTestCmdMs; testRelaxed = false; }
        if (!testRelaxed && tms - g_lastTestCmdMs > TEST_IDLE_RELAX_MS) {
            ServoHead::relax(); testRelaxed = true;
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

        // Guard C: don't hold the servos against gravity on a draining battery
        // (the condition that amplified the original burnout). Checked ~10s.
        if (ms - lastPowerCheck > 10000) {
            lastPowerCheck = ms;
            int  lvl = M5.Power.getBatteryLevel();
            bool charging    = (M5.Power.isCharging() == m5::Power_Class::is_charging);
            bool discharging = (M5.Power.isCharging() == m5::Power_Class::is_discharging);
            // Latched with hysteresis: trip below the floor while discharging, and
            // only clear once plugged in AND back above floor+8%. Without the latch,
            // relaxing the head unloads the cell, % springs back over the floor, and
            // the guard re-energizes the servo it just protected -> oscillation.
            if (!g_lowBattery) {
                if (discharging && lvl >= 0 && lvl < LOW_BATT_PCT) g_lowBattery = true;
            } else if (charging && lvl >= LOW_BATT_PCT + 8) {
                g_lowBattery = false;
            }
        }

        if (g_lowBattery) {                    // battery low + unplugged -> go limp
            ServoHead::relax();
            g_status.tracking = false; g_status.name[0] = 0; g_status.hasNext = false;
            g_stalled = false;
            s_idleSince = 0;
            if (s_warnScreen != 1) { UI::status("Low battery", TFT_RED, "plug in to track"); s_warnScreen = 1; }
        } else if (t.valid && s_stallCooldown &&
                   (uint32_t)(ms - s_cooldownStart) < STALL_COOLDOWN_MS) {
            // Stall cooldown: a recent aim couldn't reach target (likely a jam) -
            // stay limp instead of grinding into the stop.
            ServoHead::relax();
            g_status.tracking = false;
            if (s_warnScreen != 2) { UI::status("Servo stall", TFT_RED, "check calibration"); s_warnScreen = 2; }
        } else if (t.valid) {                   // valid => reachable (within az/el limits)
            s_warnScreen = 0;
            s_stallCooldown = false;            // cooldown elapsed / inactive -> resume tracking
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

            // Guard B: if the head can't reach the commanded angle AND isn't
            // moving, it's jammed/stalled - cut torque before stall current
            // cooks the servo, then cool down before retrying.
            float actP = ServoHead::readPan(), actT = ServoHead::readTilt();
            // Per-axis: pair each axis's OWN gap with its OWN motion. A jammed tilt
            // fighting gravity must trip even while pan slews across the pass - a
            // shared OR-gap / AND-stuck let a moving pan mask a stalled tilt, the
            // exact burnout case this guard exists to catch.
            bool panStall  = fabsf(ServoHead::panAngle()  - actP) > STALL_GAP_DEG &&
                             fabsf(actP - s_prevActP) < STALL_MOVE_DEG;
            bool tiltStall = fabsf(ServoHead::tiltAngle() - actT) > STALL_GAP_DEG &&
                             fabsf(actT - s_prevActT) < STALL_MOVE_DEG;
            s_prevActP = actP; s_prevActT = actT;
            if (panStall || tiltStall) {
                if (s_stallSince == 0) s_stallSince = ms;
                else if (ms - s_stallSince > STALL_HOLD_MS) {
                    ServoHead::relax();
                    s_stallCooldown = true; s_cooldownStart = ms;  // rollover-safe elapsed test
                    s_stallSince = 0;
                    g_stalled = true;
                }
            } else { s_stallSince = 0; g_stalled = false; }
        } else {
            s_warnScreen = 0;
            g_stalled = false;                 // not tracking -> clear any stale stall flag
            // Idle: home the head ONCE, then de-energize the servos. Re-issuing
            // park() every second kept the tilt motor holding the head up
            // against gravity continuously - on a sagging battery that constant
            // current cooked the motor. Letting it go limp removes that load.
            if (s_idleSince == 0)    { s_idleSince = ms; s_idleRelaxed = false; ServoHead::park(); }
            else if (!s_idleRelaxed && ms - s_idleSince > 2500) { ServoHead::relax(); s_idleRelaxed = true; }
            g_status.tracking = false;
            g_status.name[0] = 0;
            // computeNextPass is heavy (SGP4 over the whole catalog), so THROTTLE it:
            // first run, when the reachable AOS arrives, or every 60 s. Never every
            // tick - even when nothing is reachable (that's what pegged the loop).
            bool aosArrived = nextPass.valid && now >= nextPass.aosUnix;
            if (nextPassMs == 0 ||
                (aosArrived && ms - nextPassMs > 5000UL) ||   // min 5 s apart: can't peg the loop
                ms - nextPassMs > 60000UL) {
                nextPass = Sky::computeNextPass(now);
                nextPassMs = ms;
            }
            // hasNext = a REACHABLE pass to count down to. nextName/nextInReach are
            // set regardless so the empty state can say "out of reach" vs "no passes".
            g_status.hasNext     = nextPass.valid;
            g_status.nextInReach = nextPass.inReach;
            strncpy(g_status.nextName, nextPass.name, sizeof(g_status.nextName) - 1);
            g_status.nextName[sizeof(g_status.nextName) - 1] = 0;
            if (nextPass.valid) g_status.nextAos = nextPass.aosUnix;
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

    // One-time servo ID tool (requested from the web). Borrows the bus and
    // reboots when done; never returns. Runs here so the servo rail is powered.
    if (ServoSetup::pending()) ServoSetup::run();

    if (!LittleFS.begin(true)) {           // format on first boot if needed
        UI::status("Filesystem error", TFT_RED);
        delay(2000);
    }

    settings.load();
    ServoHead::begin();                    // parks the head
    Compass::begin();                      // magnetometer (loads stored offsets)
    applyForwardBearing();                 // forward bearing from manual/auto heading (cached declination)
    ServoHead::setLimits(settings.panLimit, settings.minElevation, settings.maxElevation);

    delay(700);
    state = settings.hasWifi() ? ST_CONNECT : ST_PROVISION;
}

// 4 rapid taps in a row -> true (then resets). Drives the standby/wake gesture.
static bool quadTap() {
    static uint32_t lastMs = 0;
    static int streak = 0;
    uint32_t now = millis();
    streak = (now - lastMs <= QUAD_TAP_GAP_MS) ? streak + 1 : 1;
    lastMs = now;
    if (streak >= 4) { streak = 0; return true; }
    return false;
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

    // Magnetometer calibration / heading re-apply requested from the web UI.
    if (g_calibrateMagNeeded && state == ST_TRACK && !g_testMode && !g_sleepNow) {
        g_calibrateMagNeeded = false;
        runCompassCalibration();
    }
    if (g_applyHeadingNeeded && state == ST_TRACK && !g_testMode) {
        g_applyHeadingNeeded = false;
        refreshDeclination(Net::nowUtc());
        applyForwardBearing();
        s_forceRedraw = true;
    }

    // Schedule drives auto-sleep at the window edges (not while motor-testing).
    if (!g_testMode && (state == ST_TRACK || state == ST_SLEEP)) updateSleepSchedule(Net::nowUtc());

    // Touch: top-corner taps browse prev/next while tracking; a quadruple-tap
    // anywhere else toggles standby (and a quadruple-tap wakes it back up).
    if ((state == ST_TRACK || state == ST_SLEEP) && !g_testMode && M5.Touch.getCount()) {
        auto d = M5.Touch.getDetail();
        if (d.wasPressed()) {
            bool browsed = false;
            if (state == ST_TRACK && d.y < 44) {
                if (d.x < 70)       { Sky::selectPrev(); browsed = true; }
                else if (d.x > 250) { Sky::selectNext(); browsed = true; }
            }
            if (!browsed && quadTap()) g_sleepNow = !g_sleepNow;   // 4 taps: sleep <-> wake
        }
    }

    switch (state) {
        case ST_PROVISION:
            // Show the "Offline mode" option only when we can actually run on
            // cached data. run() returns true if the user chose offline.
            if (Provision::run(offlineCapable())) {
                if (doFetchOffline()) state = settings.hasFacing ? ST_TRACK : ST_CALIBRATE;
                else                  state = ST_CONNECT;     // nothing cached -> need WiFi
            } else {
                state = ST_CONNECT;
            }
            break;

        case ST_CONNECT:
            UI::status("Connecting to WiFi...", TFT_YELLOW);
            if (Net::connect(settings.ssid, settings.pass)) {
                state = ST_FETCH;
            } else {
                // KEEP the saved creds (so it reconnects back home) - just fall
                // back to setup, where Offline mode is offered if data is cached.
                UI::status("WiFi unavailable", TFT_RED, "choose setup or offline");
                delay(1500);
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
