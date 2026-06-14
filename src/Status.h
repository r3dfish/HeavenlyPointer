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
    char   nextName[28] = {0};
    time_t nextAos = 0;        // unix UTC of the next AOS
};

extern LiveStatus g_status;

// Requests raised by the web UI, consumed (and cleared) in trackTick():
extern bool g_tleRefetchNeeded;   // re-download the TLE catalog (group changed)
extern bool g_relocateNeeded;     // re-run IP geolocation

// Desired sleep state - toggled by the SLEEP/WAKE button and the schedule.
extern bool g_sleepNow;

// Motor-test mode: pauses tracking; servos driven directly by the web /test.
extern bool g_testMode;

// Result of the last IP-geolocation attempt (shown on the web page).
enum GeoStatus : int { GEO_NONE = 0, GEO_OK = 1, GEO_FAIL = 2 };
extern int    g_geoStatus;
extern String g_geoCity;
