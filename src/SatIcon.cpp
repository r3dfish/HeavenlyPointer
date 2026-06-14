// ============================================================================
//  SatIcon.cpp
// ============================================================================
#include "SatIcon.h"
#include <string.h>
#include <ctype.h>
#include <math.h>

namespace SatIcon {
namespace {

// RGB565 palette (hard-coded so we don't depend on a color helper).
constexpr uint16_t C_PANEL = 0x2B39;   // solar-array blue
constexpr uint16_t C_GRID  = 0x11F2;   // darker blue cell lines
constexpr uint16_t C_BODY  = 0xD6BB;   // light-grey bus
constexpr uint16_t C_BODYD = 0x7BF1;   // grey outline
constexpr uint16_t C_GOLD  = 0xD568;   // telescope foil
constexpr uint16_t C_GOLDD = 0x93C5;
constexpr uint16_t C_METAL = 0xB5B7;   // dish / antenna
constexpr uint16_t C_NOZ   = 0x7AC7;   // rocket nozzle
constexpr uint16_t C_DARK  = 0x18E5;   // aperture
constexpr uint16_t C_HI    = 0xFFFF;   // highlight

// A solar panel with cell grid lines.
void panel(LovyanGFX& g, int x, int y, int w, int h) {
    if (w < 2 || h < 2) return;
    g.fillRect(x, y, w, h, C_PANEL);
    g.drawRect(x, y, w, h, C_GRID);
    if (w >= h) {
        int cells = w / 5; if (cells < 2) cells = 2;
        for (int i = 1; i < cells; i++) g.drawFastVLine(x + w * i / cells, y, h, C_GRID);
        g.drawFastHLine(x, y + h / 2, w, C_GRID);
    } else {
        int cells = h / 5; if (cells < 2) cells = 2;
        for (int i = 1; i < cells; i++) g.drawFastHLine(x, y + h * i / cells, w, C_GRID);
        g.drawFastVLine(x + w / 2, y, h, C_GRID);
    }
}

// Case-insensitive substring test (needle given in UPPER case).
bool has(const char* hay, const char* needle) {
    size_t nl = strlen(needle);
    for (; *hay; ++hay) {
        size_t i = 0;
        while (i < nl && hay[i] && toupper((unsigned char)hay[i]) == (unsigned char)needle[i]) i++;
        if (i == nl) return true;
    }
    return false;
}

} // anonymous namespace

Type classify(const char* name) {
    if (has(name, "STARLINK")) return STARLINK;
    if (has(name, "ISS") || has(name, "ZARYA") || has(name, "TIANHE") ||
        has(name, "TIANGONG") || has(name, "CSS (")) return STATION;
    if (has(name, "HUBBLE") || has(name, "HST")) return TELESCOPE;
    if (has(name, "NOAA") || has(name, "GOES") || has(name, "METEOR") ||
        has(name, "METOP") || has(name, "HIMAWARI") || has(name, "FENGYUN") ||
        has(name, "DMSP")) return WEATHER;
    if (has(name, "GPS") || has(name, "NAVSTAR") || has(name, "GLONASS") ||
        has(name, "GALILEO") || has(name, "BEIDOU") || has(name, "GSAT")) return NAV;
    if (has(name, "R/B") || has(name, "DEB")) return ROCKET;
    return COMSAT;
}

const char* typeName(Type t) {
    switch (t) {
        case STATION:   return "station";
        case STARLINK:  return "starlink";
        case NAV:       return "nav sat";
        case WEATHER:   return "weather";
        case TELESCOPE: return "telescope";
        case ROCKET:    return "rocket body";
        default:        return "satellite";
    }
}

void draw(LovyanGFX& g, Type t, int cx, int cy, int s) {
    auto k = [&](float f) { return (int)lroundf(s * f); };

    switch (t) {
        case STATION: {                                   // truss + modules + 4 wings
            int half = k(0.30f);
            g.fillRect(cx - half, cy - 2, half * 2, 4, C_BODYD);
            g.fillRoundRect(cx - k(0.10f), cy - k(0.07f), k(0.20f), k(0.14f), 3, C_BODY);
            g.drawRoundRect(cx - k(0.10f), cy - k(0.07f), k(0.20f), k(0.14f), 3, C_BODYD);
            g.fillRoundRect(cx - k(0.03f), cy - k(0.21f), k(0.06f), k(0.16f), 2, C_BODY);
            int pw = k(0.20f), ph = k(0.14f);
            panel(g, cx - half - pw, cy - ph - 3, pw, ph);
            panel(g, cx - half - pw, cy + 3,      pw, ph);
            panel(g, cx + half,      cy - ph - 3, pw, ph);
            panel(g, cx + half,      cy + 3,      pw, ph);
            break;
        }
        case STARLINK: {                                  // one flat panel + bus
            int pw = k(0.82f), ph = k(0.30f);
            panel(g, cx - pw / 2, cy - ph - 2, pw, ph);
            int bw = k(0.34f), bh = k(0.18f);
            g.fillRoundRect(cx - bw / 2, cy + 2, bw, bh, 2, C_BODY);
            g.drawRoundRect(cx - bw / 2, cy + 2, bw, bh, 2, C_BODYD);
            break;
        }
        case NAV: {                                       // bus + wings + down antenna
            int bw = k(0.26f), bh = k(0.34f), pw = k(0.28f), ph = k(0.26f), gap = k(0.04f);
            g.fillRoundRect(cx - bw / 2, cy - bh / 2, bw, bh, 2, C_BODY);
            g.drawRoundRect(cx - bw / 2, cy - bh / 2, bw, bh, 2, C_BODYD);
            panel(g, cx - bw / 2 - gap - pw, cy - ph / 2, pw, ph);
            panel(g, cx + bw / 2 + gap,      cy - ph / 2, pw, ph);
            int ay = cy + bh / 2;
            g.fillTriangle(cx - k(0.12f), ay + k(0.16f), cx + k(0.12f), ay + k(0.16f), cx, ay, C_METAL);
            break;
        }
        case WEATHER: {                                   // bus + one wing + scanner
            int bw = k(0.24f), bh = k(0.34f);
            g.fillRoundRect(cx - bw / 2, cy - bh / 2, bw, bh, 2, C_BODY);
            g.drawRoundRect(cx - bw / 2, cy - bh / 2, bw, bh, 2, C_BODYD);
            panel(g, cx + bw / 2 + k(0.04f), cy - k(0.10f), k(0.40f), k(0.20f));
            g.fillCircle(cx, cy + bh / 2 + k(0.06f), k(0.07f), C_METAL);
            g.drawCircle(cx, cy + bh / 2 + k(0.06f), k(0.07f), C_BODYD);
            break;
        }
        case TELESCOPE: {                                 // gold tube + aperture + wings
            int bw = k(0.50f), bh = k(0.28f);
            g.fillRoundRect(cx - bw / 2, cy - bh / 2, bw, bh, bh / 2, C_GOLD);
            g.drawRoundRect(cx - bw / 2, cy - bh / 2, bw, bh, bh / 2, C_GOLDD);
            g.fillCircle(cx + bw / 2 - 2, cy, bh / 2 - 1, C_DARK);
            g.drawCircle(cx + bw / 2 - 2, cy, bh / 2 - 1, C_HI);
            panel(g, cx - k(0.18f), cy - bh / 2 - k(0.16f), k(0.36f), k(0.12f));
            panel(g, cx - k(0.18f), cy + bh / 2 + k(0.04f), k(0.36f), k(0.12f));
            break;
        }
        case ROCKET: {                                    // tumbling cylinder + nozzle
            int bw = k(0.22f), bh = k(0.52f);
            g.fillRoundRect(cx - bw / 2, cy - bh / 2, bw, bh, bw / 2, C_METAL);
            g.drawRoundRect(cx - bw / 2, cy - bh / 2, bw, bh, bw / 2, C_BODYD);
            g.drawFastHLine(cx - bw / 2, cy - k(0.06f), bw, C_BODYD);
            g.drawFastHLine(cx - bw / 2, cy + k(0.06f), bw, C_BODYD);
            g.fillTriangle(cx - bw / 2, cy + bh / 2, cx + bw / 2, cy + bh / 2,
                           cx, cy + bh / 2 + k(0.14f), C_NOZ);
            break;
        }
        default: {                                        // COMSAT - bus + 2 wings + dish
            int bw = k(0.24f), bh = k(0.42f), pw = k(0.30f), ph = k(0.30f), gap = k(0.04f);
            g.fillRoundRect(cx - bw / 2, cy - bh / 2, bw, bh, 2, C_BODY);
            g.drawRoundRect(cx - bw / 2, cy - bh / 2, bw, bh, 2, C_BODYD);
            panel(g, cx - bw / 2 - gap - pw, cy - ph / 2, pw, ph);
            panel(g, cx + bw / 2 + gap,      cy - ph / 2, pw, ph);
            g.drawLine(cx, cy - bh / 2, cx, cy - bh / 2 - k(0.12f), C_HI);
            g.fillCircle(cx, cy - bh / 2 - k(0.15f), k(0.05f), C_HI);
            break;
        }
    }
}

} // namespace SatIcon
