// ============================================================================
//  Settings.h  -  Persistent configuration stored in NVS (survives reboots)
// ============================================================================
#pragma once
#include <Arduino.h>
#include "config.h"

struct Settings {
    // --- WiFi credentials ---
    String   ssid;
    String   pass;

    // --- Observer location (filled by geolocation or manual entry) ---
    double   lat = 0.0;          // degrees N
    double   lon = 0.0;          // degrees E
    double   altM = 0.0;         // metres above sea level
    bool     hasLocation = false;
    int      tzOffsetMin = 0;    // UTC offset (minutes) for the DISPLAYED clock
                                 // only - tracking always uses UTC
    bool     tzManual = false;   // true if the offset was set by hand (don't
                                 // auto-refresh it from IP geolocation)

    // --- Desk orientation ---
    Facing   facing = FACE_NORTH;
    bool     hasFacing = false;

    // --- Heading / compass ---
    uint8_t  headingSource    = HEADING_MANUAL; // see HeadingSource (manual vs magnetometer)
    float    manualHeadingDeg = -1.0f;          // manual TRUE bearing the face points; <0 = derive from facing
    float    headingOffsetDeg = 0.0f;           // mounting trim added to the raw magnetometer heading
    float    declinationDeg   = 0.0f;           // magnetic declination (deg E); auto from WMM unless declinationManual
    bool     declinationManual = false;         // true = use declinationDeg as typed, don't auto-compute
    bool     magCalDone        = false;         // magnetometer has been calibrated at least once

    // --- Catalog ---
    String   tleGroup = DEFAULT_TLE_GROUP;
    uint32_t tleFetchedAt = 0;   // unix UTC when catalog was last downloaded

    // --- Tracking / reach limits (web-editable) ---
    double   minElevation = 5.0;   // deg; lower elevation reach limit (skip horizon-grazing passes)
    double   maxElevation = 90.0;  // deg; upper elevation reach limit
    double   panLimit     = 120.0; // deg; azimuth reach +/- from "forward"
    String   filter;               // optional satellite name substring filter ("" = all)
    uint8_t  orbitClass = ORBIT_LEO;  // which orbit classes to track (see OrbitClass)
    bool     ledBars = true;          // RGB bars: green=tracking, red=waiting

    // --- Sleep / quiet hours (all times are LOCAL minutes since midnight) ---
    bool     sleepSchedule = false;   // auto-sleep during the window below
    int      sleepStartMin = 23 * 60; // go to sleep at (default 23:00)
    int      sleepEndMin   = 7 * 60;  // wake at (default 07:00)

    bool hasWifi() const { return ssid.length() > 0; }

    void load();                 // read from NVS into this struct
    void save();                 // persist this struct to NVS
    void clearWifi();            // forget WiFi creds (forces re-provision)
    void factoryReset();         // wipe everything
};

extern Settings settings;        // single global instance
