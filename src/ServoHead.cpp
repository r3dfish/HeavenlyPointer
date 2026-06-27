// ============================================================================
//  ServoHead.cpp  -  Aims the Stack-chan head via the StackChan-BSP Motion API.
//
//  The Stack-chan servos are Feetech SERIAL-BUS servos, driven by
//  M5StackChan.Motion (which powers the servo rail through the IO expander and
//  runs its own smoothing task). We just compute yaw/pitch targets (in 1/10
//  degree) from the satellite's az/el and hand them to Motion.move().
//
//  Mapping:
//    relAz = wrap180(azimuth - forwardBearing)            # +/- from "forward"
//    yaw   = YAW_DIR   * relAz*10        (clamped to +/-YAW_LIMIT_TENTHS)
//    pitch = PITCH_HORIZON_TENTHS + PITCH_DIR * el*10      (clamped to safe band)
// ============================================================================
#include "ServoHead.h"
#include <M5StackChan.h>
#include <math.h>

namespace ServoHead {

static float s_forwardBearing = 0.0f;   // degrees; set from Facing
static int   s_lastYawT  = 0;           // last commanded yaw   (1/10 deg)
static int   s_lastPitchT = 0;          // last commanded pitch (1/10 deg)
static bool  s_inRange   = true;
static bool  s_torqueOn  = true;        // are the servos currently energized?

// Runtime reach limits (web-editable; defaults from config.h).
static float s_panLimitDeg = YAW_LIMIT_TENTHS / 10.0f;   // +/- azimuth from forward
static float s_elMinDeg    = 5.0f;
static float s_elMaxDeg    = 90.0f;

static float wrap180(float a) {
    while (a > 180.0f)  a -= 360.0f;
    while (a < -180.0f) a += 360.0f;
    return a;
}
static int clampi(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
// Re-energize the servos before commanding a move (no-op if already on).
static void ensureTorque() {
    if (!s_torqueOn) { M5StackChan.Motion.setTorqueEnabled(true); s_torqueOn = true; }
}

void begin() {
    // M5StackChan.begin() (called in setup) already powered the servo rail and
    // initialised the bus servos. Just center the head to a known pose.
    park();
    M5StackChan.Motion.move(s_lastYawT, s_lastPitchT, SERVO_SPEED);
}

void setForwardBearing(float trueBearingDeg) {
    while (trueBearingDeg < 0.0f)    trueBearingDeg += 360.0f;
    while (trueBearingDeg >= 360.0f) trueBearingDeg -= 360.0f;
    s_forwardBearing = trueBearingDeg;
}
float forwardBearing() { return s_forwardBearing; }

void setFacing(Facing f) { setForwardBearing(facingBearing(f)); }

void setLimits(float panLimitDeg, float elMinDeg, float elMaxDeg) {
    s_panLimitDeg = panLimitDeg;
    s_elMinDeg    = elMinDeg;
    s_elMaxDeg    = elMaxDeg;
}

bool reachable(float azDeg, float elDeg) {
    float relAz = wrap180(azDeg - s_forwardBearing);
    return fabsf(relAz) <= s_panLimitDeg && elDeg >= s_elMinDeg && elDeg <= s_elMaxDeg;
}

bool aimAt(float azDeg, float elDeg) {
    ensureTorque();
    float relAz = wrap180(azDeg - s_forwardBearing);
    s_inRange = reachable(azDeg, elDeg);            // within az + el limits?

    // Clamp to the runtime pan limit and the hard servo-safe pitch band.
    int panLimT = (int)lroundf(s_panLimitDeg * 10.0f);
    int yawT  = clampi(YAW_DIR * (int)lroundf(relAz * 10.0f), -panLimT, panLimT);
    int pitchT = clampi(PITCH_HORIZON_TENTHS + PITCH_DIR * (int)lroundf(elDeg * 10.0f),
                        PITCH_MIN_TENTHS, PITCH_MAX_TENTHS);

    s_lastYawT = yawT;
    s_lastPitchT = pitchT;
    M5StackChan.Motion.move(yawT, pitchT, SERVO_SPEED);
    return s_inRange;
}

void park() {
    ensureTorque();
    s_lastYawT   = 0;                                          // face forward
    s_lastPitchT = clampi(PITCH_HORIZON_TENTHS, PITCH_MIN_TENTHS, PITCH_MAX_TENTHS);
    s_inRange    = true;
    M5StackChan.Motion.move(s_lastYawT, s_lastPitchT, SERVO_SPEED);
}

// Cut servo torque so the head goes limp (rests at its mechanical position).
// Used when idle/asleep so the tilt motor isn't holding the head up against
// gravity continuously - that constant load overheats it, especially on a
// sagging battery. Re-energized automatically by the next aimAt()/park().
void relax() {
    if (s_torqueOn) { M5StackChan.Motion.setTorqueEnabled(false); s_torqueOn = false; }
}

void testMove(float yawDeg, float pitchDeg) {
    ensureTorque();
    int yawT   = clampi((int)lroundf(yawDeg   * 10.0f), -YAW_LIMIT_TENTHS, YAW_LIMIT_TENTHS);
    int pitchT = clampi((int)lroundf(pitchDeg * 10.0f),  PITCH_MIN_TENTHS, PITCH_MAX_TENTHS);
    s_lastYawT = yawT;
    s_lastPitchT = pitchT;
    s_inRange  = true;
    M5StackChan.Motion.move(yawT, pitchT, SERVO_SPEED);
}

void recover() {
    M5StackChan.Motion.setTorqueEnabled(true);   // re-enable torque after a stall trip
    s_torqueOn = true;
    park();
}

float readPan()  { return M5StackChan.Motion.getCurrentYawAngle()   / 10.0f; }
float readTilt() { return M5StackChan.Motion.getCurrentPitchAngle() / 10.0f; }

// The BSP runs its own motion task, so there's nothing to do per-tick.
void tick() {}

float panAngle()  { return s_lastYawT  / 10.0f; }   // degrees, for the HUD
float tiltAngle() { return s_lastPitchT / 10.0f; }
bool  inRange()   { return s_inRange; }

} // namespace ServoHead
