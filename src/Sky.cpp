// ============================================================================
//  Sky.cpp
// ============================================================================
#include "Sky.h"
#include "config.h"
#include "ServoHead.h"     // for reachable() - only select sats the head can point at
#include <LittleFS.h>
#include <Sgp4.h>
#include <math.h>
#include <ctype.h>

namespace Sky {

// In-memory TLE catalog (fixed buffers -> no heap churn while tracking).
struct TleEntry {
    char  name[28];
    char  l1[74];
    char  l2[74];
    float mm;        // mean motion (revs/day), parsed from TLE line 2
};
static TleEntry  s_cat[MAX_SATS];
static int       s_count = 0;

static double    s_lat = 0, s_lon = 0, s_altM = 0;
static int       s_current = -1;     // index of the satellite we're tracking
static float     s_minEl   = HORIZON_MIN_EL;  // runtime min tracking elevation
static String    s_filter;                    // lowercased name filter ("" = all)
static uint8_t   s_orbitClass = ORBIT_LEO;    // which orbit classes to track
static bool      s_manual  = false;           // manual next/prev hold active
static time_t    s_lastNow = 0;               // last time passed to update()

static Sgp4      s_sat;               // reusable propagator

// Earth gravitational parameter & radius for the velocity estimate.
static constexpr double MU_EARTH = 398600.4418;   // km^3/s^2
static constexpr double RE_EARTH = 6378.137;      // km

int count() { return s_count; }

void setSite(double latDeg, double lonDeg, double altM) {
    s_lat = latDeg; s_lon = lonDeg; s_altM = altM;
    s_sat.site(latDeg, lonDeg, altM);
}

void setMinElevation(float deg) { s_minEl = deg; }

void setFilter(const String& f) {
    s_filter = f;
    s_filter.toLowerCase();
    s_filter.trim();
    s_current = -1;   // force re-selection under the new filter
}

void setOrbitClass(uint8_t cls) {
    s_orbitClass = cls;
    s_current = -1;   // force re-selection
}

// Orbit-class tests on the parsed mean motion (revs/day).
static bool isGeo(const TleEntry& e) { return e.mm > 0.0f && e.mm < GEO_MEAN_MOTION_CUTOFF; }
static bool isLeo(const TleEntry& e) { return e.mm >= LEO_MEAN_MOTION_MIN; }

// True if this entry should be skipped under the current orbit-class filter.
static bool orbitFiltered(const TleEntry& e) {
    if (s_orbitClass == ORBIT_LEO)    return !isLeo(e);   // keep LEO only
    if (s_orbitClass == ORBIT_NONGEO) return  isGeo(e);   // drop geostationary
    return false;                                         // ORBIT_ALL
}

// Case-insensitive substring test - no heap allocation (runs per-sat each tick).
static bool ciContains(const char* hay, const char* needle) {
    if (!needle[0]) return true;
    for (; *hay; ++hay) {
        const char* h = hay; const char* n = needle;
        while (*h && *n && tolower((unsigned char)*h) == tolower((unsigned char)*n)) { ++h; ++n; }
        if (!*n) return true;
    }
    return false;
}
// True if a satellite name passes the active name filter (s_filter is lowercased).
static bool passesFilter(const char* name) {
    if (s_filter.length() == 0) return true;
    return ciContains(name, s_filter.c_str());
}

int load() {
    s_count = 0;
    File f = LittleFS.open(TLE_FILE_PATH, "r");
    if (!f) return 0;

    // The file is groups of 3 lines: name, TLE line 1, TLE line 2.
    while (f.available() && s_count < MAX_SATS) {
        String name = f.readStringUntil('\n');
        String l1   = f.readStringUntil('\n');
        String l2   = f.readStringUntil('\n');
        name.trim(); l1.trim(); l2.trim();
        // A valid pair starts with "1 " and "2 ".
        if (l1.length() < 60 || l2.length() < 60) continue;
        if (l1[0] != '1' || l2[0] != '2') continue;

        TleEntry& e = s_cat[s_count];
        strncpy(e.name, name.c_str(), sizeof(e.name) - 1); e.name[sizeof(e.name)-1]=0;
        strncpy(e.l1,   l1.c_str(),   sizeof(e.l1)   - 1); e.l1[sizeof(e.l1)-1]=0;
        strncpy(e.l2,   l2.c_str(),   sizeof(e.l2)   - 1); e.l2[sizeof(e.l2)-1]=0;
        // Mean motion lives in columns 53-63 of TLE line 2 (revs/day).
        e.mm = (l2.length() >= 63) ? l2.substring(52, 63).toFloat() : 0.0f;
        s_count++;
    }
    f.close();
    s_current = -1;
    return s_count;
}

// Propagate one catalog entry to `now` and fill a Target.
static Target computeTarget(int i, time_t now) {
    Target t;
    if (i < 0 || i >= s_count) return t;
    TleEntry& e = s_cat[i];
    s_sat.init(e.name, e.l1, e.l2);
    s_sat.findsat((unsigned long)now);

    strncpy(t.name, e.name, sizeof(t.name) - 1);
    t.az      = s_sat.satAz;
    t.el      = s_sat.satEl;
    t.rangeKm = s_sat.satDist;
    t.subLat  = s_sat.satLat;
    t.subLon  = s_sat.satLon;
    t.altKm   = s_sat.satAlt;
    t.vis     = s_sat.satVis;
    // Instantaneous orbital speed via the vis-viva equation, with the
    // semi-major axis derived from the TLE mean motion. This accounts for
    // orbital eccentricity (a plain circular estimate sqrt(GM/r) does not).
    // (SGP4 computes the exact velocity vector internally, but the library
    // keeps it private, so vis-viva is the most accurate value we can read.)
    {
        double r = RE_EARTH + max(1.0, t.altKm);              // geocentric distance, km
        double n = e.mm * 6.283185307179586 / 86400.0;        // mean motion, rad/s
        double a = (n > 1e-9) ? cbrt(MU_EARTH / (n * n)) : r; // semi-major axis, km
        double term = 2.0 / r - 1.0 / a;
        t.velKms = sqrt(MU_EARTH * (term > 0.0 ? term : 1.0 / r));
    }
    t.valid   = true;
    return t;
}

Target update(time_t now) {
    s_lastNow = now;
    Target best;  double bestEl = -1000.0;  int bestIdx = -1;
    Target cur;                                     // current target this tick

    for (int i = 0; i < s_count; i++) {
        // Only consider sats that pass the filters AND that the head can
        // actually point at (within the azimuth/elevation reach limits). A sat
        // outside the limits is skipped, so it's never selected; and if the
        // CURRENT target drifts out of reach, cur stays invalid below and we
        // automatically adopt the next-best reachable satellite.
        if (orbitFiltered(s_cat[i])) continue;
        if (!passesFilter(s_cat[i].name)) continue;
        Target t = computeTarget(i, now);
        if (!t.valid) continue;
        if (!ServoHead::reachable(t.az, t.el)) continue;
        if (i == s_current) cur = t;
        if (t.el > bestEl) { bestEl = t.el; best = t; bestIdx = i; }
    }

    // cur.valid here implies the current target is still reachable.
    // --- manual hold (next/prev): stay on the chosen sat while reachable -----
    if (s_manual) {
        if (cur.valid) return cur;
        s_manual = false;                           // out of reach -> back to auto
    }

    // --- auto: highest reachable sat, with anti-jitter hysteresis -----------
    if (cur.valid) {
        if (best.valid && bestIdx != s_current && best.el > cur.el + SWITCH_MARGIN_EL) {
            s_current = bestIdx;
            return best;
        }
        return cur;
    }

    // Current gone (set or out of reach) -> adopt the highest reachable one.
    s_current = best.valid ? bestIdx : -1;
    return best;   // .valid is false if nothing reachable is up
}

// Step the current target through the currently-visible satellites.
static void cycleTarget(int dir) {
    if (s_lastNow == 0) return;
    static int vis[MAX_SATS];
    int n = 0;
    for (int i = 0; i < s_count; i++) {
        if (orbitFiltered(s_cat[i])) continue;
        if (!passesFilter(s_cat[i].name)) continue;
        Target t = computeTarget(i, s_lastNow);
        if (t.valid && ServoHead::reachable(t.az, t.el)) vis[n++] = i;
    }
    if (n == 0) return;                             // nothing up -> no-op
    int pos = -1;
    for (int j = 0; j < n; j++) if (vis[j] == s_current) { pos = j; break; }
    if (pos < 0) pos = (dir > 0) ? -1 : 0;          // current not visible -> ends
    int np = ((pos + dir) % n + n) % n;             // wrap-around
    s_current = vis[np];
    s_manual  = true;
}

void selectNext() { cycleTarget(+1); }
void selectPrev() { cycleTarget(-1); }

// Find the first moment inside a pass window [jdStart, jdStop] when the sat is
// actually REACHABLE (within the az/el limits). Returns its Julian date, or -1
// if the head can never point at this pass. (~20 s sampling.)
static double firstReachableJd(double jdStart, double jdStop, double jdNow) {
    if (jdStart < jdNow) jdStart = jdNow;   // only count FUTURE moments - a pass already
                                            // in progress has its AOS in the past
    const double stepJd = 20.0 / 86400.0;   // 20 seconds
    for (double jd = jdStart; jd <= jdStop + 1e-9; jd += stepJd) {
        unsigned long ut = (unsigned long)((jd - 2440587.5) * 86400.0);
        s_sat.findsat(ut);
        if (ServoHead::reachable(s_sat.satAz, s_sat.satEl)) return jd;
    }
    return -1.0;
}

NextPass computeNextPass(time_t now) {
    NextPass best;
    const double jdNow = (double)now / 86400.0 + 2440587.5;  // unix -> Julian date
    double bestReachJd = 1e18;          // soonest pass the head can actually point at
    double bestAnyJd   = 1e18;          // soonest pass at all (for the empty-state hint only)
    char   anyName[28] = {0};
    for (int i = 0; i < s_count; i++) {
        if (orbitFiltered(s_cat[i])) continue;
        if (!passesFilter(s_cat[i].name)) continue;
        TleEntry& e = s_cat[i];
        s_sat.init(e.name, e.l1, e.l2);
        s_sat.initpredpoint((unsigned long)now, s_minEl);

        // Only this sat's NEXT pass - across the whole catalog that's enough to
        // find the soonest reachable one, and it keeps this (heavy) call cheap.
        // (nextpass returns TRUE on success.)
        double ws[1], we[1], wmax[1];
        int nw = 0;
        {
            passinfo pass;
            if (s_sat.nextpass(&pass, 20)) {
                ws[0] = pass.jdstart; we[0] = pass.jdstop; wmax[0] = pass.maxelevation; nw = 1;
            }
        }

        // Track the soonest pass overall (reachable or not) - used ONLY to tell
        // "no passes at all" from "passes exist but out of reach" in the empty state.
        if (nw > 0 && ws[0] < bestAnyJd) {
            bestAnyJd = ws[0];
            strncpy(anyName, e.name, sizeof(anyName) - 1); anyName[sizeof(anyName) - 1] = 0;
        }

        // Some passes rise above the horizon but stay outside the pan/elevation
        // reach, so they'd never be tracked - find the first reachable one.
        for (int w = 0; w < nw; w++) {
            double jdReach = firstReachableJd(ws[w], we[w], jdNow);
            if (jdReach < 0) continue;                     // window never in reach (in future)
            if (jdReach < bestReachJd) {
                bestReachJd = jdReach;
                best.valid = true; best.inReach = true;
                strncpy(best.name, e.name, sizeof(best.name) - 1);
                best.name[sizeof(best.name) - 1] = 0;
                best.maxEl   = wmax[w];
                best.aosUnix = (time_t)((jdReach - 2440587.5) * 86400.0);
            }
            break;                                         // got this sat's next reachable pass
        }
    }
    // We only ever count down to passes we can POINT at. If none are reachable
    // but passes do exist, record that (name only, no AOS -> no countdown) so the
    // UI can say "out of reach" instead of "no passes". valid stays false.
    if (!best.valid && bestAnyJd < 1e17) {
        best.inReach = false;
        strncpy(best.name, anyName, sizeof(best.name) - 1); best.name[sizeof(best.name) - 1] = 0;
    }
    return best;
}

const char* visText(int vis) {
    // Hopperpop SGP4 visibility codes.
    switch (vis) {
        case -2: return "below horizon";
        case -1: return "daylight";
        case  0: return "eclipsed";
        default: return "visible";   // positive -> illuminated & dark sky
    }
}

} // namespace Sky
