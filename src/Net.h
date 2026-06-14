// ============================================================================
//  Net.h  -  Network services: WiFi, NTP time, IP geolocation, TLE download
// ============================================================================
#pragma once
#include <Arduino.h>

namespace Net {
    // Join a WiFi network. Blocks up to timeoutMs. Returns true on success.
    bool connect(const String& ssid, const String& pass, uint32_t timeoutMs = 20000);
    bool isConnected();

    // Sync clock from NTP (UTC) and push it into the on-board RTC.
    // Returns true once the system clock looks valid (year >= 2024).
    bool syncTime(uint32_t timeoutMs = 15000);

    // Current UTC unix time (seconds). 0 if the clock isn't set yet.
    time_t nowUtc();

    // Best-effort geolocation from public IP. Fills lat/lon (deg), alt (m),
    // the local UTC offset in seconds (DST-correct at request time), and the
    // detected city. Returns true on success. (Uses http://ip-api.com - no key.)
    bool geolocate(double& lat, double& lon, double& altM, int& offsetSec, String& city);

    // Download a Celestrak GP/TLE catalog for `group` and store it at
    // TLE_FILE_PATH on LittleFS. Returns the number of satellites written,
    // or -1 on failure.
    int  fetchTLEs(const String& group);
}
