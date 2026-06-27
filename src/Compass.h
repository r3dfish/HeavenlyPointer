// ============================================================================
//  Compass.h  -  Tilt-compensated heading from the CoreS3 BMM150 magnetometer.
//
//  Returns the MAGNETIC heading of the face (0=N..360); the caller adds the
//  WMM declination to get a TRUE bearing. Hard/soft-iron calibration uses
//  M5Unified's built-in sphere fit (persisted to NVS). Because the pan servo
//  and the 12 RGB LEDs sit centimetres from the sensor, read heading only with
//  the servos relaxed and the LEDs off.
// ============================================================================
#pragma once
#include <Arduino.h>

namespace Compass {
    void begin();
    bool available();          // true if a magnetometer is present & responding

    // --- Calibration (non-blocking; drive from a loop while the user rotates) ---
    void  startCalibration();  // begin the sphere-fit accumulation
    void  updateCalibration(); // call often during the figure-8
    void  finishCalibration(); // stop + persist offsets to NVS
    void  cancelCalibration(); // abort: stop + restore the previously-saved offsets (no persist)
    bool  calibrating();
    float calProgress();       // 0..1 heuristic of heading-sector coverage

    // Averaged, tilt-compensated MAGNETIC heading of the face (deg, 0..360),
    // with the mounting trim (settings.headingOffsetDeg) applied.
    //   tiltDeg : how far the head is from level
    //   quality : 0..100 (sample consistency, penalised when tilted)
    // Returns false if there's no magnetometer or the read is unusable.
    bool readHeading(float* magHeadingDeg, float* tiltDeg, float* quality);
}
