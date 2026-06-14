// ============================================================================
//  Provision.h  -  WiFi credential capture via captive portal OR touch screen.
// ============================================================================
#pragma once

namespace Provision {
    // Runs the setup flow until WiFi credentials are obtained. Writes them
    // (and optional location overrides) straight into the global `settings`,
    // then returns true. Blocks until provisioned.
    bool run();
}
