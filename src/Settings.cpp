// ============================================================================
//  Settings.cpp
// ============================================================================
#include "Settings.h"
#include <Preferences.h>

Settings settings;

static const char* NS = "heavpoint";   // NVS namespace (rebrand; old "heavtrack" data is orphaned)

void Settings::load() {
    Preferences p;
    p.begin(NS, /*readOnly=*/true);
    ssid         = p.getString("ssid", "");
    pass         = p.getString("pass", "");
    lat          = p.getDouble("lat", 0.0);
    lon          = p.getDouble("lon", 0.0);
    altM         = p.getDouble("alt", 0.0);
    hasLocation  = p.getBool("hasloc", false);
    tzOffsetMin  = p.getInt("tzoff", 0);
    tzManual     = p.getBool("tzman", false);
    facing       = (Facing)p.getUChar("facing", FACE_NORTH);
    hasFacing    = p.getBool("hasface", false);
    tleGroup     = p.getString("tlegrp", DEFAULT_TLE_GROUP);
    tleFetchedAt = p.getUInt("tleat", 0);
    minElevation = p.getDouble("minel", 5.0);
    maxElevation = p.getDouble("maxel", 90.0);
    panLimit     = p.getDouble("panlim", 120.0);
    filter       = p.getString("filter", "");
    orbitClass   = p.getUChar("orbit", ORBIT_LEO);
    ledBars      = p.getBool("leds", true);
    sleepSchedule = p.getBool("slsch", false);
    sleepStartMin = p.getInt("slstart", 23 * 60);
    sleepEndMin   = p.getInt("slend", 7 * 60);
    p.end();
}

void Settings::save() {
    Preferences p;
    p.begin(NS, /*readOnly=*/false);
    p.putString("ssid", ssid);
    p.putString("pass", pass);
    p.putDouble("lat", lat);
    p.putDouble("lon", lon);
    p.putDouble("alt", altM);
    p.putBool("hasloc", hasLocation);
    p.putInt("tzoff", tzOffsetMin);
    p.putBool("tzman", tzManual);
    p.putUChar("facing", (uint8_t)facing);
    p.putBool("hasface", hasFacing);
    p.putString("tlegrp", tleGroup);
    p.putUInt("tleat", tleFetchedAt);
    p.putDouble("minel", minElevation);
    p.putDouble("maxel", maxElevation);
    p.putDouble("panlim", panLimit);
    p.putString("filter", filter);
    p.putUChar("orbit", orbitClass);
    p.putBool("leds", ledBars);
    p.putBool("slsch", sleepSchedule);
    p.putInt("slstart", sleepStartMin);
    p.putInt("slend", sleepEndMin);
    p.end();
}

void Settings::clearWifi() {
    Preferences p;
    p.begin(NS, false);
    p.remove("ssid");
    p.remove("pass");
    p.end();
    ssid = "";
    pass = "";
}

void Settings::factoryReset() {
    Preferences p;
    p.begin(NS, false);
    p.clear();
    p.end();
    *this = Settings();   // reset in-memory copy to defaults
}
