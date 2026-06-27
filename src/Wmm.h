// ============================================================================
//  Wmm.h  -  World Magnetic Model 2025: magnetic declination on-device.
//
//  Converts the magnetometer's MAGNETIC heading to TRUE (geographic) heading,
//  which is what satellite azimuths are referenced to. Fully offline - the
//  WMM2025 spherical-harmonic coefficients are embedded (see wmm_coeffs.h).
//  Valid 2025.0 - 2030.0; ports the NOAA reference algorithm (verified against
//  NOAA's published test values to < 0.005 deg).
// ============================================================================
#pragma once
#include <time.h>

namespace Wmm {
    // Magnetic declination in degrees (EAST positive) for a location and date.
    //   latDeg/lonDeg : geodetic, degrees (N+/E+)
    //   altKm         : height above the WGS84 ellipsoid, km (0 is fine on a desk)
    //   decimalYear   : e.g. 2026.45  (clamped to the model's 2025-2030 lifespan)
    float declination(float latDeg, float lonDeg, float altKm, float decimalYear);

    // Decimal year (e.g. 2026.45) from a UTC time_t. Returns the model epoch if utc<=0.
    float decimalYear(time_t utc);
}
