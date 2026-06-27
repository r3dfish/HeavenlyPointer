// ============================================================================
//  ServoHead.h  -  Drives the pan/tilt servos to aim the face at an az/el.
// ============================================================================
#pragma once
#include <Arduino.h>
#include "config.h"

namespace ServoHead {
    void begin();

    // Which compass direction the face points when pan is centered.
    void setFacing(Facing f);

    // Exact forward azimuth (degrees true north) the face points when pan is
    // centered. Used by the magnetometer auto-heading path and precise manual
    // entry; setFacing() is just the N/E/S/W shorthand for this.
    void setForwardBearing(float trueBearingDeg);

    // The current forward bearing (degrees true north).
    float forwardBearing();

    // Runtime reach limits: azimuth +/- panLimitDeg from forward, elevation
    // between elMinDeg and elMaxDeg.
    void setLimits(float panLimitDeg, float elMinDeg, float elMaxDeg);

    // True if the head can point at this sky direction within the limits.
    // Used by Sky so it only selects satellites it can actually reach.
    bool reachable(float azDeg, float elDeg);

    // Aim at a sky direction. azDeg: 0=N..360, elDeg: 0=horizon..90=zenith.
    // Returns false if the target azimuth is outside the servo's reachable
    // arc (so the UI can warn "out of range").
    bool aimAt(float azDeg, float elDeg);

    // Move to the neutral / forward-horizontal "parked" pose.
    void park();

    // Cut servo torque so the head goes limp (no holding current). Used when
    // idle/asleep so the tilt motor isn't holding the head up against gravity.
    void relax();

    // Directly command the pan (yaw, deg: 0 = forward) and tilt (pitch, deg)
    // servos, bypassing the az/el + facing transform. For the web motor test -
    // isolates each physical motor. Clamped to the safe travel limits.
    void testMove(float yawDeg, float pitchDeg);

    // Re-enable servo torque (recovery after an overload/stall trip) and re-home.
    void recover();

    // Actual position the servos report back over the bus (degrees). If these
    // don't track the commanded angle, that motor isn't responding.
    float readPan();
    float readTilt();

    // No-op: StackChan-BSP runs its own motion-smoothing task. Kept so the
    // caller's loop structure is unchanged.
    void tick();

    // Last commanded servo angles (for the HUD).
    float panAngle();
    float tiltAngle();
    bool  inRange();
}
