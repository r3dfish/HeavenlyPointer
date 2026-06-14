// ============================================================================
//  Sky.h  -  Loads TLEs, propagates orbits (SGP4) and picks a target.
//
//  Coordinate output is topocentric look-angles (azimuth/elevation) for the
//  observer site, which is exactly what the servo head needs to point at.
// ============================================================================
#pragma once
#include <Arduino.h>

struct Target {
    bool   valid   = false;
    char   name[28] = {0};
    double az      = 0;   // azimuth   [deg], 0=N 90=E 180=S 270=W
    double el      = 0;   // elevation [deg], 0=horizon 90=zenith
    double rangeKm = 0;   // slant range observer->satellite
    double subLat  = 0;   // sub-satellite point latitude  [deg]
    double subLon  = 0;   // sub-satellite point longitude [deg]
    double altKm   = 0;   // height above Earth's surface
    double velKms  = 0;   // estimated orbital speed
    int    vis     = 0;   // raw SGP4 visibility code
};

namespace Sky {
    // Read the TLE catalog from flash into memory. Returns satellite count.
    int  load();
    int  count();

    // Observer location for look-angle computation.
    void setSite(double latDeg, double lonDeg, double altM);

    // Only track satellites above this elevation (degrees). Runtime-editable.
    void setMinElevation(float deg);

    // Restrict tracking to satellites whose name contains this substring
    // (case-insensitive). Empty string = track everything.
    void setFilter(const String& f);

    // Restrict tracking by orbit class: ORBIT_ALL, ORBIT_NONGEO (skip
    // geostationary), or ORBIT_LEO (low Earth orbit only). Default ORBIT_LEO.
    void setOrbitClass(uint8_t cls);

    // Manually step to the next/previous currently-visible satellite (above the
    // min elevation, passing the filters). Engages a manual hold that reverts to
    // auto-tracking once the held satellite sets.
    void selectNext();
    void selectPrev();

    // Recompute the best target for time `nowUtc` (unix seconds, UTC).
    // Applies sticky-target hysteresis so the head doesn't jump around.
    Target update(time_t nowUtc);

    // Human-readable visibility text for a SGP4 vis code.
    const char* visText(int vis);

    // Earliest upcoming pass across the filtered catalog (the next satellite
    // that will rise above the min elevation). HEAVY - call sparingly (e.g. once
    // while parked) and count down to aosUnix locally between calls.
    struct NextPass {
        bool   valid   = false;
        char   name[28] = {0};
        time_t aosUnix = 0;   // unix UTC when it rises above the min elevation
        double maxEl   = 0;   // peak elevation of that pass (deg)
    };
    NextPass computeNextPass(time_t nowUtc);
}
