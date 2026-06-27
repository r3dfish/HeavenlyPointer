// ============================================================================
//  Compass.cpp  -  Magnetometer heading via M5Unified (BMI270 + BMM150).
// ============================================================================
#include "Compass.h"
#include "config.h"
#include "Settings.h"
#include <M5Unified.h>
#include <math.h>

namespace Compass {

static bool     s_hasMag = false;
static bool     s_cal    = false;
static uint16_t s_sectorMask = 0;   // which of 12 thirty-degree heading sectors were swept

static inline float norm3(float x, float y, float z) { return sqrtf(x*x + y*y + z*z); }

void begin() {
    // M5StackChan.begin() -> M5.begin() already brought up the IMU. Load any
    // stored offsets and make sure runtime auto-calibration is OFF: with the
    // pan servo and RGB LEDs so close, continuous accumulation would slowly
    // corrupt the offsets. We calibrate explicitly instead.
    M5.Imu.loadOffsetFromNVS();
    M5.Imu.setCalibration(0, 0, 0);
    float mx, my, mz;
    s_hasMag = M5.Imu.getMag(&mx, &my, &mz) && !(mx == 0 && my == 0 && mz == 0);
}

bool available() { return s_hasMag; }

// Tilt-compensated heading from one accel+mag sample (Freescale AN4248).
// headRad is 0..2pi; tiltDeg is how far the head is from level.
static bool computeHeading(float ax, float ay, float az,
                           float mx, float my, float mz,
                           float* headRad, float* tiltDeg) {
    float an = norm3(ax, ay, az);
    if (an < 1e-3f) return false;
    ax /= an; ay /= an; az /= an;
    float phi   = atan2f(ay, az);                              // roll
    float theta = atan2f(-ax, ay * sinf(phi) + az * cosf(phi)); // pitch
    float mxh = mx * cosf(theta) + my * sinf(theta) * sinf(phi) + mz * sinf(theta) * cosf(phi);
    float myh = my * cosf(phi) - mz * sinf(phi);
    float h = atan2f(-myh, mxh);
    if (h < 0) h += 2.0f * (float)M_PI;
    *headRad = h;
    *tiltDeg = acosf(fminf(1.0f, fabsf(az))) * 180.0f / (float)M_PI;
    return true;
}

void startCalibration() {
    s_sectorMask = 0;
    s_cal = true;
    M5.Imu.setCalibration(0, 0, 120);   // magnetometer-only sphere fit
}

void updateCalibration() {
    if (!s_cal) return;
    M5.Imu.update();
    float ax, ay, az, mx, my, mz;
    if (M5.Imu.getAccel(&ax, &ay, &az) && M5.Imu.getMag(&mx, &my, &mz)) {
        float h, t;
        if (computeHeading(ax, ay, az, mx, my, mz, &h, &t)) {
            int sec = (int)(h * 180.0f / (float)M_PI / 30.0f) % 12;
            if (sec >= 0 && sec < 12) s_sectorMask |= (uint16_t)(1u << sec);
        }
    }
}

void finishCalibration() {
    if (!s_cal) return;
    M5.Imu.saveOffsetToNVS();
    M5.Imu.setCalibration(0, 0, 0);
    s_cal = false;
}

void cancelCalibration() {
    if (!s_cal) return;
    M5.Imu.loadOffsetFromNVS();   // restore last-good offsets (discard this run's accumulation)
    M5.Imu.setCalibration(0, 0, 0);
    s_cal = false;
}

bool calibrating() { return s_cal; }

float calProgress() {
    int n = 0;
    for (int i = 0; i < 12; i++) if (s_sectorMask & (uint16_t)(1u << i)) n++;
    return n / 12.0f;
}

bool readHeading(float* magHeadingDeg, float* tiltDeg, float* quality) {
    *magHeadingDeg = 0; *tiltDeg = 0; *quality = 0;   // always defined, even on early return
    if (!s_hasMag) return false;
    float sx = 0, sy = 0, tsum = 0;
    int ok = 0;
    for (int i = 0; i < 24; i++) {
        M5.Imu.update();
        float ax, ay, az, mx, my, mz;
        if (M5.Imu.getAccel(&ax, &ay, &az) && M5.Imu.getMag(&mx, &my, &mz)) {
            float h, t;
            if (computeHeading(ax, ay, az, mx, my, mz, &h, &t)) {
                sx += cosf(h); sy += sinf(h); tsum += t; ok++;
            }
        }
        delay(8);
    }
    if (ok < 6) return false;

    float R = norm3(sx, sy, 0.0f) / ok;          // resultant length 0..1 (consistency)
    float h = atan2f(sy, sx);
    if (h < 0) h += 2.0f * (float)M_PI;
    float headDeg = h * 180.0f / (float)M_PI + settings.headingOffsetDeg;
    while (headDeg < 0)    headDeg += 360.0f;
    while (headDeg >= 360) headDeg -= 360.0f;

    float tilt = tsum / ok;
    float q = R * 100.0f;
    if (tilt > COMPASS_MAX_TILT_DEG) q *= 0.3f;   // too tilted to trust

    *magHeadingDeg = headDeg;
    *tiltDeg = tilt;
    *quality = q;
    return true;
}

} // namespace Compass
