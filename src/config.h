// ============================================================================
//  config.h  -  Central tunables & hardware calibration for HeavenlyPointer
//
//  EVERYTHING you may need to adjust for YOUR Stack-chan lives here.
//  The servo + mounting constants in particular MUST be verified on hardware
//  the first time you flash (see the README's "Calibration" section).
// ============================================================================
#pragma once

// Firmware version (shown on the boot splash). Bump this per release.
#define FW_VERSION "v1.1.0"

// ---------------------------------------------------------------------------
//  Head / servo geometry  (StackChan-BSP serial-bus servos)
//
//  The Stack-chan's pan/tilt servos are Feetech serial-bus servos managed by
//  the StackChan-BSP `Motion` controller - NOT PWM hobby servos. Angles are
//  commanded in 1/10-degree units (e.g. 900 = 90.0 deg) and the BSP runs its
//  own smoothing engine, so there's no wiring/pin to set here.
//
//  BSP physical limits (from M5StackChan servo_init):
//      yaw (pan)  : -1280 .. 1280   (+/-128 deg)
//      pitch(tilt):     0 ..  900   (0 .. 90 deg)
//  Tenths below are what we actually command (kept inside those + the product's
//  safe envelope). Verify directions on hardware and flip *_DIR if reversed.
// ---------------------------------------------------------------------------

// --- Yaw / azimuth ---
// 0 = face pointing straight "forward" (the direction chosen at calibration).
// Keep this comfortably below the servo's electronic limit (+/-128 deg): the
// head reaches a mechanical stop before that, and driving into it STALLS the
// pan servo and trips its overload protection (head goes limp until power-
// cycled). If the head strains/buzzes at the extremes, lower this.
constexpr int YAW_LIMIT_TENTHS = 1200;   // app pan limit, +/-120 deg (near the
                                         // +/-128 electronic limit - verify the
                                         // head clears the mechanical stop here)
constexpr int YAW_DIR          = +1;     // set -1 if the head pans the wrong way

// --- Pitch / elevation ---
// The head's pitch angle tracks the satellite's elevation. PITCH_HORIZON_TENTHS
// is the pitch value that aims the face at the horizon (elevation 0); pitch then
// rises 1:1 with elevation. Calibrate PITCH_HORIZON_TENTHS on hardware using the
// live tilt readout on the tracking screen.
constexpr int PITCH_HORIZON_TENTHS = 0;    // pitch (tenths) when looking at el = 0
constexpr int PITCH_DIR            = +1;   // set -1 if tilt is inverted
constexpr int PITCH_MIN_TENTHS     = 50;   // safe floor  ( 5 deg) - product limit
constexpr int PITCH_MAX_TENTHS     = 850;  // safe ceiling (85 deg) - product limit

// BSP motion smoothing speed (0..1000). Higher = snappier head movement.
constexpr int SERVO_SPEED = 250;

// --- Servo-protection guards (defense-in-depth against a burnout) -----------
// A held servo fighting gravity is what cooked the original tilt motor; these
// cut torque before sustained load can damage a servo.
#define TEST_IDLE_RELAX_MS   30000U   // motor-test: relax if no command for this long
#define LOW_BATT_PCT         20       // relax + warn below this battery % while on battery
#define STALL_GAP_DEG        25.0f    // commanded-vs-actual gap that may mean a jam
#define STALL_MOVE_DEG       2.5f     // actual moving less than this between ticks = "stuck"
#define STALL_HOLD_MS        5000U    // gap+stuck must persist this long to trip a stall
#define STALL_COOLDOWN_MS    20000U   // stay limp this long after a stall trip, then retry

// ---------------------------------------------------------------------------
//  Tracking behaviour
// ---------------------------------------------------------------------------
// Only consider a satellite "trackable" above this elevation (degrees).
// Raise to e.g. 5-10 to ignore satellites sitting low on the horizon (where
// buildings/trees usually block the view anyway).
constexpr float HORIZON_MIN_EL   = 0.0f;
// Drop the current target the MOMENT it sinks below the horizon - the head
// never points at a satellite that's below the horizon. Keep >= HORIZON_MIN_EL.
constexpr float HORIZON_EXIT_EL  = 0.0f;
// Only switch to a different satellite if it beats the current target's
// elevation by at least this margin (prevents nervous target hopping).
constexpr float SWITCH_MARGIN_EL = 8.0f;
// How often the tracking solution is recomputed (ms).
constexpr uint32_t TRACK_INTERVAL_MS = 1000;
// Max satellites loaded from a TLE catalog (caps RAM + CPU per tick).
constexpr int MAX_SATS = 220;

// Orbit classification by mean motion (revolutions/day from TLE line 2):
//   GEO/geosync  ~1.0   (skipped by "skip GEO" and "LEO only")
//   MEO/HEO      ~1.7-2 (GPS, Galileo, Molniya)
//   LEO          >=~11.25  (period <= ~128 min, altitude < ~2000 km)
// Below this is treated as geostationary/geosynchronous (period > ~16 h).
constexpr float GEO_MEAN_MOTION_CUTOFF = 1.5f;
// At/above this is Low Earth Orbit - the fast passes that sweep across the sky.
constexpr float LEO_MEAN_MOTION_MIN    = 11.25f;

// Which orbit classes to track (web-selectable). LEO-only is the default, since
// only fast LEO passes really move across the sky for the head to follow.
enum OrbitClass : uint8_t { ORBIT_ALL = 0, ORBIT_NONGEO = 1, ORBIT_LEO = 2 };

// Normal screen backlight (0-255). Sleep mode drops it to 0.
constexpr uint8_t SCREEN_BRIGHTNESS = 180;

// ---------------------------------------------------------------------------
//  Display units
// ---------------------------------------------------------------------------
// Range & altitude on the HUD are shown in miles. Set to 1.0f (and swap the
// "mi" labels in UI.cpp) if you'd rather show kilometres.
constexpr float KM_TO_MI = 0.621371f;

// ---------------------------------------------------------------------------
//  Data sources
// ---------------------------------------------------------------------------
// Celestrak GP element group. Good options:
//   "stations" -> ISS + crewed/space-station objects (few; reliable + famous)
//   "visual"   -> ~150 brightest naked-eye satellites (great default)
//   "amateur"  -> amateur (ham) radio satellites - many carry FM/SSB repeaters
//   "active"   -> thousands of active sats (heavy; needs lots of storage)
#define DEFAULT_TLE_GROUP "visual"

// Refresh the TLE catalog when it is older than this many hours.
constexpr uint32_t TLE_MAX_AGE_HOURS = 12;

// NTP servers for UTC time (SGP4 needs accurate UTC).
#define NTP_SERVER_1 "pool.ntp.org"
#define NTP_SERVER_2 "time.google.com"

// ---------------------------------------------------------------------------
//  WiFi provisioning Access-Point (shown when no creds are stored)
// ---------------------------------------------------------------------------
#define AP_SSID "HeavenlyPointer"
#define AP_PASS ""              // open AP so the join-QR needs no password

// ---------------------------------------------------------------------------
//  Filesystem paths
// ---------------------------------------------------------------------------
#define TLE_FILE_PATH "/tles.txt"

// ---------------------------------------------------------------------------
//  Compass facing options for the desk-orientation step
// ---------------------------------------------------------------------------
enum Facing : uint8_t { FACE_NORTH = 0, FACE_EAST = 1, FACE_SOUTH = 2, FACE_WEST = 3 };
// Bearing (degrees) the face points to for each Facing choice.
inline float facingBearing(Facing f) { return (uint8_t)f * 90.0f; }
inline const char* facingName(Facing f) {
    switch (f) { case FACE_NORTH: return "NORTH"; case FACE_EAST: return "EAST";
                 case FACE_SOUTH: return "SOUTH"; default: return "WEST"; }
}

// How the head's "forward" bearing is determined.
//   MANUAL = the N/E/S/W tap (or a typed bearing) you set by hand.
//   AUTO   = read it from the on-board magnetometer (true-north corrected).
enum HeadingSource : uint8_t { HEADING_MANUAL = 0, HEADING_AUTO = 1 };

// Magnetometer reads are only trusted when the head is level within this much
// (degrees). Beyond it, AUTO heading is flagged low-quality and we hold manual.
// Max gap between taps for the 4-tap standby/wake gesture (ms).
#define QUAD_TAP_GAP_MS        700U

#define COMPASS_MAX_TILT_DEG   35.0f
// Calibration is considered good once the user has swept the head through this
// many of 12 thirty-degree heading sectors during the figure-8.
#define COMPASS_CAL_SECTORS    10
