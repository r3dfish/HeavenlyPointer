// ============================================================================
//  Provision.h  -  WiFi credential capture via captive portal OR touch screen.
// ============================================================================
#pragma once

namespace Provision {
    // Runs the setup flow. If offlineCapable is true, an "Offline mode" button
    // is offered. Returns true if the user chose OFFLINE (run on cached data,
    // no WiFi); returns false after WiFi credentials are captured + saved to
    // the global `settings`. Blocks until one of those happens.
    bool run(bool offlineCapable);
}
