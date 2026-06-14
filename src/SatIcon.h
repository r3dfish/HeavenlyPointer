// ============================================================================
//  SatIcon.h  -  Vector (code-drawn) satellite icons by type. Zero storage:
//  each icon is rendered with M5GFX primitives, classified from the TLE name.
// ============================================================================
#pragma once
#include <M5Unified.h>   // for the LovyanGFX drawing type

namespace SatIcon {
    enum Type { COMSAT, STATION, STARLINK, NAV, WEATHER, TELESCOPE, ROCKET };

    // Pick an icon type from a satellite's catalog name (case-insensitive).
    Type classify(const char* name);

    // Short human label for the type (e.g. "station", "starlink").
    const char* typeName(Type t);

    // Draw the icon centered at (cx,cy), fitting an s-by-s pixel box.
    void draw(LovyanGFX& gfx, Type t, int cx, int cy, int s);
}
