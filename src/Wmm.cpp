// ============================================================================
//  Wmm.cpp  -  WMM2025 declination (NOAA reference algorithm, verified port).
// ============================================================================
#include "Wmm.h"
#include <Arduino.h>
#include <math.h>
#include "wmm_coeffs.h"     // WMM_COEFFS[90], WMM_EPOCH, WMM_MAX_DEGREE, WMM_VALID_*

namespace Wmm {

static const int MAXORD = WMM_MAX_DEGREE;       // 12
static const int SZ     = WMM_MAX_DEGREE + 1;   // 13

// Schmidt-normalized, time-independent Gauss coefficients + recursion helpers,
// built once from the embedded coefficient table.
static double C_[SZ][SZ], CD_[SZ][SZ], K_[SZ][SZ], FN_[SZ], FM_[SZ];
static bool   s_init = false;

static void initOnce() {
    if (s_init) return;
    double snorm[SZ * SZ];
    memset(C_, 0, sizeof C_); memset(CD_, 0, sizeof CD_);
    memset(K_, 0, sizeof K_); memset(snorm, 0, sizeof snorm);

    for (size_t i = 0; i < sizeof(WMM_COEFFS) / sizeof(WMM_COEFFS[0]); i++) {
        int n = WMM_COEFFS[i].n, m = WMM_COEFFS[i].m;
        double g = WMM_COEFFS[i].g, h = WMM_COEFFS[i].h, gd = WMM_COEFFS[i].gd, hd = WMM_COEFFS[i].hd;
        if (m > MAXORD) break;
        C_[m][n] = g; CD_[m][n] = gd;
        if (m != 0) { C_[n][m-1] = h; CD_[n][m-1] = hd; }
    }

    // Convert Schmidt-normalized coefficients to the unnormalized form the
    // recursion expects, and precompute the K recurrence terms.
    snorm[0] = 1.0; FM_[0] = 0.0;
    for (int n = 1; n <= MAXORD; n++) {
        snorm[n] = snorm[n-1] * (double)(2*n-1) / (double)n;
        int j = 2;
        for (int m = 0; m <= n; m++) {
            K_[m][n] = (double)(((n-1)*(n-1)) - (m*m)) / (double)((2*n-1)*(2*n-3));
            if (m > 0) {
                double flnmj = (double)((n-m+1)*j) / (double)(n+m);
                snorm[n + m*SZ] = snorm[n + (m-1)*SZ] * sqrt(flnmj);
                j = 1;
                C_[n][m-1]  *= snorm[n + m*SZ];
                CD_[n][m-1] *= snorm[n + m*SZ];
            }
            C_[m][n]  *= snorm[n + m*SZ];
            CD_[m][n] *= snorm[n + m*SZ];
        }
        FN_[n] = (double)(n + 1);
        FM_[n] = (double)n;
    }
    K_[1][1] = 0.0;
    s_init = true;
}

float declination(float latDeg, float lonDeg, float altKm, float decimalYear) {
    initOnce();

    // Clamp the date to the model's lifespan so secular extrapolation can't run wild.
    double yr = decimalYear;
    if (yr < WMM_VALID_FROM) yr = WMM_VALID_FROM;
    if (yr > WMM_VALID_TO)   yr = WMM_VALID_TO;

    double tc[SZ][SZ], dp[SZ][SZ];
    double sp[SZ], cp[SZ], pp[SZ], p[SZ*SZ];
    memset(tc, 0, sizeof tc); memset(dp, 0, sizeof dp);
    memset(sp, 0, sizeof sp); memset(cp, 0, sizeof cp);
    memset(pp, 0, sizeof pp); memset(p, 0, sizeof p);

    sp[0] = 0.0; cp[0] = 1.0; pp[0] = 1.0; p[0] = 1.0; dp[0][0] = 0.0;

    const double a = 6378.137, b = 6356.7523142, re = 6371.2;
    const double a2 = a*a, b2 = b*b, c2 = a2-b2, a4 = a2*a2, b4 = b2*b2, c4 = a4-b4;
    const double DEG = M_PI / 180.0;

    double dt = yr - WMM_EPOCH;
    double alt = altKm;

    double rlon = lonDeg*DEG, rlat = latDeg*DEG;
    double srlon = sin(rlon), srlat = sin(rlat), crlon = cos(rlon), crlat = cos(rlat);
    double srlat2 = srlat*srlat, crlat2 = crlat*crlat;
    sp[1] = srlon; cp[1] = crlon;

    // Geodetic -> geocentric spherical coordinates.
    double q  = sqrt(a2 - c2*srlat2);
    double q1 = alt*q;
    double q2t = (q1 + a2) / (q1 + b2); double q2 = q2t*q2t;
    double ct = srlat / sqrt(q2*crlat2 + srlat2);
    double st = sqrt(1.0 - ct*ct);
    double r2 = (alt*alt) + 2.0*q1 + (a4 - c4*srlat2)/(q*q);
    double r  = sqrt(r2);
    double d  = sqrt(a2*crlat2 + b2*srlat2);
    double ca = (alt + d)/r;
    double sa = c2*crlat*srlat/(r*d);

    for (int m = 2; m <= MAXORD; m++) {
        sp[m] = sp[1]*cp[m-1] + cp[1]*sp[m-1];
        cp[m] = cp[1]*cp[m-1] - sp[1]*sp[m-1];
    }

    double aor = re/r, ar = aor*aor;
    double br = 0, bt = 0, bp = 0, bpp = 0;

    for (int n = 1; n <= MAXORD; n++) {
        ar *= aor;
        for (int m = 0; m <= n; m++) {
            // Associated Legendre polynomials + derivatives via recursion.
            if (n == m) {
                p[n + m*SZ] = st * p[(n-1) + (m-1)*SZ];
                dp[m][n] = st*dp[m-1][n-1] + ct*p[(n-1) + (m-1)*SZ];
            } else if (n == 1 && m == 0) {
                p[n + m*SZ] = ct * p[(n-1) + m*SZ];
                dp[m][n] = ct*dp[m][n-1] - st*p[(n-1) + m*SZ];
            } else if (n > 1 && n != m) {
                if (m > n-2) { p[(n-2) + m*SZ] = 0.0; dp[m][n-2] = 0.0; }
                p[n + m*SZ] = ct*p[(n-1) + m*SZ] - K_[m][n]*p[(n-2) + m*SZ];
                dp[m][n] = ct*dp[m][n-1] - st*p[(n-1) + m*SZ] - K_[m][n]*dp[m][n-2];
            }

            tc[m][n] = C_[m][n] + dt*CD_[m][n];
            if (m != 0) tc[n][m-1] = C_[n][m-1] + dt*CD_[n][m-1];

            double par = ar * p[n + m*SZ];
            double temp1, temp2;
            if (m == 0) { temp1 = tc[m][n]*cp[m]; temp2 = tc[m][n]*sp[m]; }
            else { temp1 = tc[m][n]*cp[m] + tc[n][m-1]*sp[m]; temp2 = tc[m][n]*sp[m] - tc[n][m-1]*cp[m]; }

            bt -= ar * temp1 * dp[m][n];
            bp += FM_[m] * temp2 * par;
            br += FN_[n] * temp1 * par;

            // North/South geographic pole special case.
            if (st == 0.0 && m == 1) {
                if (n == 1) pp[n] = pp[n-1];
                else        pp[n] = ct*pp[n-1] - K_[m][n]*pp[n-2];
                double parp = ar * pp[n];
                bpp += FM_[m] * temp2 * parp;
            }
        }
    }

    if (st == 0.0) bp = bpp; else bp /= st;

    // Rotate spherical -> geodetic; declination only needs the horizontal pair.
    double bx = -bt*ca - br*sa;   // north
    double by = bp;               // east
    return (float)(atan2(by, bx) / DEG);
}

float decimalYear(time_t utc) {
    if (utc <= 0) return WMM_EPOCH;
    struct tm t; gmtime_r(&utc, &t);
    int year = t.tm_year + 1900;
    // Day-of-year fraction (tm_yday is 0-based). Ignore leap-year denominator
    // nuance - sub-day precision is irrelevant for declination.
    double frac = (t.tm_yday + (t.tm_hour + t.tm_min / 60.0) / 24.0) / 365.0;
    return (float)(year + frac);
}

} // namespace Wmm
