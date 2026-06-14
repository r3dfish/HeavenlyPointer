// ============================================================================
//  UI.h  -  All on-screen drawing & touch widgets for the 2" CoreS3 display.
// ============================================================================
#pragma once
#include <Arduino.h>
#include "config.h"
#include "Sky.h"

namespace UI {
    void begin();

    // Simple full-screen status / splash message.
    void splash(const char* title, const char* line1 = "", const char* line2 = "");
    void status(const char* msg, uint16_t color = 0xFFFF, const char* line2 = "");

    // ----- Provisioning screen ----------------------------------------------
    // Draws the join-QR + instructions and a "Touch keyboard" button.
    // Returns true if that button was tapped this frame.
    bool drawProvision(const char* joinQr, const char* portalUrl, const char* apSsid);

    // Blocking on-device WiFi entry. Returns true and fills ssid/pass if the
    // user completed entry; false if they backed out.
    bool pickSSID(String& ssidOut);                       // scan + tap to pick
    String keyboard(const char* title, bool password);    // on-screen QWERTY

    // ----- Calibration screen -----------------------------------------------
    // Blocks until the user taps a compass direction. Returns the choice.
    Facing askFacing();

    // ----- Tracking HUD -----------------------------------------------------
    // Non-blocking: paints the current tracking state. Call each tick.
    void drawTracking(const Target& t, float pan, float tilt,
                      bool inRange, time_t utc, int satCount);
    void drawSearching(time_t utc, int satCount, const Sky::NextPass& next);

    // Motor-test screen: commanded vs. actual (bus read-back) servo angles.
    void drawTest(float cmdPan, float cmdTilt, float actPan, float actTilt);
}
