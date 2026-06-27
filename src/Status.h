// ============================================================================
//  Status.h  -  Shared live state, written by trackTick() and read by the
//  web server. Both run on the main loop thread, so no locking is needed.
// ============================================================================
#pragma once
#include <Arduino.h>

struct LiveStatus {
    bool   tracking = false;   // is a satellite currently being followed?
    char   name[28] = {0};
    double az = 0, el = 0;
    double rangeKm = 0, altKm = 0, velKms = 0;
    double subLat = 0, subLon = 0;
    int    vis = 0;
    float  pan = 0, tilt = 0;  // commanded servo angles (deg)
    bool   inRange = true;
    int    satCount = 0;
    time_t utc = 0;

    // Next-pass countdown (populated while parked / searching)
    bool   hasNext = false;
    bool   nextInReach = true; // false = soonest pass is outside the reach arc
    char   nextName[28] = {0};
    time_t nextAos = 0;        // unix UTC of the next AOS
};

extern LiveStatus g_status;

// Requests raised by the web UI, consumed (and cleared) in trackTick():
extern bool g_tleRefetchNeeded;   // re-download the TLE catalog (group changed)
extern bool g_relocateNeeded;     // re-run IP geolocation
extern bool g_calibrateMagNeeded; // run the magnetometer calibration routine
extern bool g_applyHeadingNeeded; // re-apply the forward bearing (heading settings changed)

// Live heading state (for the web page + diagnostics overlay).
struct HeadingState {
    bool  autoActive = false;   // AUTO source AND a good magnetometer fix is in use
    bool  magPresent = false;   // a magnetometer was detected
    float forwardTrue = 0;      // forward bearing currently applied (deg true north)
    float magHeading = 0;       // last magnetometer MAGNETIC heading (deg)
    float declination = 0;      // declination applied (deg E)
    float tilt = 0;             // head tilt from level (deg) at last read
    float quality = 0;          // 0..100 last read quality
};
extern HeadingState g_heading;

// Desired sleep state - toggled by the SLEEP/WAKE button and the schedule.
extern bool g_sleepNow;

// Motor-test mode: pauses tracking; servos driven directly by the web /test.
extern bool g_testMode;
extern uint32_t g_lastTestCmdMs;  // millis() of the last /test command (for auto-relax)

// Servo-protection state (for the web page).
extern bool g_lowBattery;   // on battery and below LOW_BATT_PCT -> servos relaxed
extern bool g_stalled;      // last aim couldn't reach target (jam) -> servos relaxed

// Result of the last IP-geolocation attempt (shown on the web page).
enum GeoStatus : int { GEO_NONE = 0, GEO_OK = 1, GEO_FAIL = 2 };
extern int    g_geoStatus;
extern String g_geoCity;
