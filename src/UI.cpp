// ============================================================================
//  UI.cpp  -  Screens & touch widgets (M5GFX / M5Unified)
// ============================================================================
#include "UI.h"
#include "SatIcon.h"
#include "Settings.h"
#include <M5Unified.h>
#include <WiFi.h>
#include <math.h>
#include <time.h>

namespace UI {

// ---- a flicker-free off-screen buffer for the tracking HUD ----------------
static M5Canvas* hud = nullptr;
static LovyanGFX& hudG() { return *(LovyanGFX*)hud; }
static void hudFlush()   { if (hud) hud->pushSprite(0, 0); }

// ---- touch helper: true once, on the frame a press begins -----------------
static bool getTap(int& x, int& y) {
    M5.update();
    if (M5.Touch.getCount()) {
        auto d = M5.Touch.getDetail();
        if (d.wasPressed()) { x = d.x; y = d.y; return true; }
    }
    return false;
}
static void waitTap(int& x, int& y) { while (!getTap(x, y)) delay(8); }

// ---- generic button --------------------------------------------------------
static void button(int x, int y, int w, int h, const char* label,
                   uint16_t fill, uint16_t text = TFT_WHITE) {
    M5.Display.fillRoundRect(x, y, w, h, 6, fill);
    M5.Display.drawRoundRect(x, y, w, h, 6, TFT_DARKGREY);
    M5.Display.setTextColor(text, fill);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextSize(2);
    M5.Display.drawString(label, x + w / 2, y + h / 2);
}
static bool hit(int px, int py, int x, int y, int w, int h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}

void begin() {
    M5.Display.setRotation(1);          // landscape 320x240
    M5.Display.setBrightness(180);
    hud = new M5Canvas(&M5.Display);
    hud->setColorDepth(16);
    if (!hud->createSprite(320, 240)) { delete hud; hud = nullptr; }
}

// ---------------------------------------------------------------------------
//  Simple messages
// ---------------------------------------------------------------------------
void splash(const char* title, const char* line1, const char* line2) {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
    M5.Display.setTextSize(3);
    M5.Display.drawString(title, 160, 80);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setTextSize(2);
    M5.Display.drawString(line1, 160, 140);
    M5.Display.drawString(line2, 160, 170);
    // firmware version, dim in the bottom-right corner
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
    M5.Display.setTextDatum(top_right);
    M5.Display.drawString(FW_VERSION, 314, 228);
}

void status(const char* msg, uint16_t color, const char* line2) {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextColor(color, TFT_BLACK);
    M5.Display.setTextSize(2);
    bool two = line2 && line2[0];
    M5.Display.drawString(msg, 160, two ? 104 : 120);
    if (two) M5.Display.drawString(line2, 160, 136);
}

// ---------------------------------------------------------------------------
//  Provisioning screen (QR + button)
// ---------------------------------------------------------------------------
// Force-repaint latch for the provisioning screen. Provision::run() calls
// resetProvisionCache() on (re)entry, so a stale `painted` static from a prior
// session can't leave the QR + buttons UNpainted (with live hit-boxes) sitting
// on top of the "Scanning WiFi..." status screen on a second provisioning.
static bool s_provRepaint = true;
void resetProvisionCache() { s_provRepaint = true; }

int drawProvision(const char* joinQr, const char* portalUrl, const char* apSsid, bool showOffline) {
    static bool painted = false;
    static String lastQr;
    static bool lastShowOffline = false;
    // Repaint on (re)entry, a QR change, OR a change in whether the Offline button
    // is shown - the hit-boxes below depend on showOffline, so a stale paint would
    // mismatch the drawn buttons.
    if (!painted || lastQr != joinQr || lastShowOffline != showOffline || s_provRepaint) {
        M5.Display.fillScreen(TFT_BLACK);
        // The setup network name, prominent across the top.
        M5.Display.setTextDatum(top_center);
        M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
        M5.Display.setTextSize(1);
        M5.Display.drawString("Join this WiFi network:", 160, 4);
        M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
        M5.Display.setTextSize(2);
        M5.Display.drawString(apSsid, 160, 16);
        // QR on the left, instructions on the right.
        M5.Display.qrcode(joinQr, 8, 44, 128);
        M5.Display.setTextDatum(top_left);
        M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
        M5.Display.setTextSize(1);
        M5.Display.drawString("Scan QR to join,", 148, 50);
        M5.Display.drawString("or join it in your", 148, 62);
        M5.Display.drawString("phone's WiFi list.", 148, 74);
        M5.Display.drawString("A form opens -", 148, 92);
        M5.Display.drawString("pick your network", 148, 104);
        M5.Display.drawString("from the list.", 148, 116);
        M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
        M5.Display.drawString(portalUrl, 148, 134);
        if (showOffline) {
            button(148, 150, 164, 38, "Touch entry",  TFT_NAVY);
            button(148, 194, 164, 38, "Offline mode", TFT_DARKGREEN);
        } else {
            button(148, 182, 164, 48, "Touch entry", TFT_NAVY);
        }
        painted = true;
        lastQr = joinQr;
        lastShowOffline = showOffline;
        s_provRepaint = false;
    }
    int x, y;
    if (getTap(x, y)) {
        if (showOffline) {
            if (hit(x, y, 148, 150, 164, 38)) { painted = false; return 1; }  // touch entry
            if (hit(x, y, 148, 194, 164, 38)) { painted = false; return 2; }  // offline
        } else if (hit(x, y, 148, 182, 164, 48)) { painted = false; return 1; }
    }
    return 0;
}

// ---------------------------------------------------------------------------
//  WiFi network picker (scan + tap)
// ---------------------------------------------------------------------------
bool pickSSID(String& out) {
    bool forceScan = false;
    while (true) {
        // Reuse the boot-time site survey if it's still cached; only run a fresh
        // scan on first entry without one, or when the user taps Rescan.
        int n = forceScan ? -1 : WiFi.scanComplete();
        if (n < 0) { status("Scanning WiFi...", TFT_YELLOW); n = WiFi.scanNetworks(); }
        if (n < 0) n = 0;
        forceScan = false;

        M5.Display.fillScreen(TFT_BLACK);
        M5.Display.setTextDatum(top_left);
        M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
        M5.Display.setTextSize(2);
        M5.Display.drawString("Pick network:", 8, 6);

        const int maxRows = 6;
        int shown = n < maxRows ? n : maxRows;
        for (int i = 0; i < shown; i++) {
            int ry = 34 + i * 28;
            M5.Display.fillRoundRect(6, ry, 308, 24, 4, TFT_NAVY);
            M5.Display.setTextColor(TFT_WHITE, TFT_NAVY);
            M5.Display.setTextSize(1);
            M5.Display.setTextDatum(middle_left);
            String label = WiFi.SSID(i);
            if (label.length() > 34) label = label.substring(0, 34);
            M5.Display.drawString(label, 12, ry + 12);
            M5.Display.setTextDatum(middle_right);
            M5.Display.drawString(String(WiFi.RSSI(i)) + "dBm", 308, ry + 12);
        }
        button(6,   206, 150, 30, "Manual", TFT_DARKGREY);
        button(164, 206, 150, 30, "Rescan", TFT_DARKGREY);

        // wait for a tap
        int x, y;
        waitTap(x, y);
        for (int i = 0; i < shown; i++) {
            int ry = 34 + i * 28;
            if (hit(x, y, 6, ry, 308, 24)) { out = WiFi.SSID(i); WiFi.scanDelete(); return true; }
        }
        if (hit(x, y, 6, 206, 150, 30))   { out = ""; WiFi.scanDelete(); return true; } // manual
        if (hit(x, y, 164, 206, 150, 30)) forceScan = true;   // Rescan -> fresh scan next loop
        // a stray tap just redraws the (cached) list - no needless rescan
    }
}

// ---------------------------------------------------------------------------
//  On-screen QWERTY keyboard
// ---------------------------------------------------------------------------
static const int KB_TOP = 64;
static const int ROW_H  = 34;
static const int KEY_W  = 30;
static const char* KB_LOW[4] = {"1234567890","qwertyuiop","asdfghjkl","zxcvbnm"};
static const char* KB_UPP[4] = {"1234567890","QWERTYUIOP","ASDFGHJKL","ZXCVBNM"};
static const char* KB_SYM[4] = {"1234567890","@#$%&*-+()","/=_:;,.!?","~\"'<>[]{}"};

static const char* const* kbRows(int mode) {
    return mode == 1 ? KB_UPP : (mode == 2 ? KB_SYM : KB_LOW);
}

// Right-of-field buttons: show/hide (eye, on password fields) and Back (when
// cancellable). Positions depend on which are present so draw + hit-test agree.
static const int KB_BTN_Y = 18, KB_BTN_W = 58, KB_BTN_H = 30;
static int kbFieldW(bool eye, bool back) { return 308 - ((eye?1:0)+(back?1:0)) * (KB_BTN_W + 6); }
static int kbEyeX (bool eye, bool back)  { return eye ? (6 + kbFieldW(eye, back) + 6) : -1; }
static int kbBackX(bool eye, bool back)  { if (!back) return -1; int x = 6 + kbFieldW(eye, back) + 6; if (eye) x += KB_BTN_W + 6; return x; }

static void drawKeyboard(const char* title, const String& buf, bool password, bool reveal,
                         int mode, const char* rightLabel, bool cancel) {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextDatum(top_left);
    M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
    M5.Display.setTextSize(1);
    M5.Display.drawString(title, 6, 4);

    // Optional right-aligned label (e.g. the network the password is for).
    if (rightLabel && rightLabel[0]) {
        String nm = rightLabel;
        if (nm.length() > 30) nm = nm.substring(0, 30);
        M5.Display.setTextDatum(top_right);
        M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
        M5.Display.drawString(nm, 314, 4);
        M5.Display.setTextDatum(top_left);
    }

    // text field + right-side buttons (show/hide, Back), sized to fit
    int fieldW = kbFieldW(password, cancel);
    M5.Display.drawRoundRect(6, 18, fieldW, 30, 4, TFT_DARKGREY);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setTextSize(2);
    M5.Display.setTextDatum(middle_left);
    String shown = buf;
    if (password && !reveal) { shown = ""; for (size_t i = 0; i < buf.length(); i++) shown += '*'; }
    size_t maxShown = fieldW / 13; if (maxShown < 8) maxShown = 8;
    if (shown.length() > maxShown) shown = shown.substring(shown.length() - maxShown);
    M5.Display.drawString(shown + "_", 12, 33);
    if (password) button(kbEyeX(password, cancel),  KB_BTN_Y, KB_BTN_W, KB_BTN_H, reveal ? "hide" : "show", TFT_NAVY);
    if (cancel)   button(kbBackX(password, cancel), KB_BTN_Y, KB_BTN_W, KB_BTN_H, "back", TFT_MAROON);

    // character rows
    const char* const* rows = kbRows(mode);
    M5.Display.setTextDatum(middle_center);
    for (int r = 0; r < 4; r++) {
        int len = strlen(rows[r]);
        int startX = (320 - len * KEY_W) / 2;
        int ry = KB_TOP + r * ROW_H;
        for (int i = 0; i < len; i++) {
            int kx = startX + i * KEY_W;
            M5.Display.drawRoundRect(kx + 1, ry + 1, KEY_W - 2, ROW_H - 3, 3, TFT_DARKGREY);
            char s[2] = { rows[r][i], 0 };
            M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
            M5.Display.drawString(s, kx + KEY_W / 2, ry + ROW_H / 2);
        }
    }
    // function row
    int fy = KB_TOP + 4 * ROW_H;
    button(0,   fy, 50, 240 - fy, mode == 1 ? "abc" : "ABC", TFT_DARKGREY);
    button(50,  fy, 50, 240 - fy, mode == 2 ? "abc" : "#+=", TFT_DARKGREY);
    button(100, fy, 120,240 - fy, "space", TFT_NAVY);
    button(220, fy, 50, 240 - fy, "DEL", TFT_MAROON);
    button(270, fy, 50, 240 - fy, "OK",  TFT_DARKGREEN);
}

String keyboard(const char* title, bool password, const char* rightLabel, bool* cancelled) {
    String buf;
    int mode = 0;     // 0 lower, 1 upper, 2 symbols
    bool reveal = false;
    bool back = (cancelled != nullptr);     // show a Back button when the caller can handle it
    drawKeyboard(title, buf, password, reveal, mode, rightLabel, back);
    while (true) {
        int x, y; waitTap(x, y);
        // Show/hide toggle (password fields only) - reveal the typed text.
        if (password && hit(x, y, kbEyeX(password, back), KB_BTN_Y, KB_BTN_W, KB_BTN_H)) {
            reveal = !reveal;
            drawKeyboard(title, buf, password, reveal, mode, rightLabel, back);
            continue;
        }
        // Back/cancel - abandon entry and tell the caller (e.g. wrong network).
        if (back && hit(x, y, kbBackX(password, back), KB_BTN_Y, KB_BTN_W, KB_BTN_H)) {
            if (cancelled) *cancelled = true;
            return "";
        }
        int fy = KB_TOP + 4 * ROW_H;
        if (y >= fy) {                                    // function row
            if      (x < 50)  mode = (mode == 1) ? 0 : 1; // shift
            else if (x < 100) mode = (mode == 2) ? 0 : 2; // symbols
            else if (x < 220) buf += ' ';
            else if (x < 270) { if (buf.length()) buf.remove(buf.length() - 1); }
            else              return buf;                 // OK
        } else if (y >= KB_TOP && y < fy) {               // character rows
            int r = (y - KB_TOP) / ROW_H;
            if (r >= 0 && r < 4) {
                const char* row = kbRows(mode)[r];
                int len = strlen(row);
                int startX = (320 - len * KEY_W) / 2;
                if (x >= startX && x < startX + len * KEY_W) {
                    int i = (x - startX) / KEY_W;
                    buf += row[i];
                    if (mode == 1) mode = 0;              // auto-unshift
                }
            }
        }
        drawKeyboard(title, buf, password, reveal, mode, rightLabel, back);
    }
}

// ---------------------------------------------------------------------------
//  Facing / compass calibration
// ---------------------------------------------------------------------------
Facing askFacing() {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
    M5.Display.setTextSize(2);
    M5.Display.drawString("Center me & aim", 160, 22);
    M5.Display.drawString("face forward.", 160, 44);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setTextSize(1);
    M5.Display.drawString("Which way does my face point?", 160, 70);

    // compass cross of buttons
    button(120, 86,  80, 40, "NORTH", TFT_NAVY);
    button(216, 140, 90, 40, "EAST",  TFT_NAVY);
    button(120, 194, 80, 40, "SOUTH", TFT_NAVY);
    button(14,  140, 90, 40, "WEST",  TFT_NAVY);

    while (true) {
        int x, y; waitTap(x, y);
        if (hit(x, y, 120, 86,  80, 40)) return FACE_NORTH;
        if (hit(x, y, 216, 140, 90, 40)) return FACE_EAST;
        if (hit(x, y, 120, 194, 80, 40)) return FACE_SOUTH;
        if (hit(x, y, 14,  140, 90, 40)) return FACE_WEST;
    }
}

void drawCompassCal(float progress) {
    if (progress < 0) progress = 0;
    if (progress > 1) progress = 1;
    static uint32_t last = 0;
    uint32_t nowMs = millis();
    if (nowMs - last < 120) return;     // throttle redraws (called in a tight loop)
    last = nowMs;

    bool done = progress >= (float)COMPASS_CAL_SECTORS / 12.0f;
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
    M5.Display.setTextSize(2);
    M5.Display.drawString("Calibrating", 160, 38);
    M5.Display.drawString("compass", 160, 62);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setTextSize(1);
    M5.Display.drawString("Slowly rotate me through", 160, 102);
    M5.Display.drawString("a full turn, then tilt around", 160, 116);

    int x = 40, y = 150, w = 240, h = 22;
    M5.Display.drawRoundRect(x, y, w, h, 4, TFT_DARKGREY);
    int fw = (int)((w - 4) * progress);
    M5.Display.fillRoundRect(x + 2, y + 2, fw, h - 4, 3, done ? TFT_GREEN : TFT_CYAN);

    char b[8]; snprintf(b, sizeof b, "%d%%", (int)(progress * 100));
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setTextSize(2);
    M5.Display.drawString(b, 160, 192);
    M5.Display.setTextColor(done ? TFT_GREEN : TFT_DARKGREY, TFT_BLACK);
    M5.Display.setTextSize(1);
    M5.Display.drawString(done ? "good - tap to finish" : "tap to finish", 160, 222);
}

// ---------------------------------------------------------------------------
//  Tracking HUD
// ---------------------------------------------------------------------------
static void drawPolar(LovyanGFX& g, int cx, int cy, int R, double az, double el) {
    g.drawCircle(cx, cy, R, TFT_DARKGREY);
    g.drawCircle(cx, cy, R * 2 / 3, TFT_DARKGREY);   // 30 deg el ring
    g.drawCircle(cx, cy, R / 3, TFT_DARKGREY);       // 60 deg el ring
    g.setTextColor(TFT_DARKGREY, TFT_BLACK);
    g.setTextDatum(middle_center);
    g.setTextSize(1);
    g.drawString("N", cx, cy - R - 6);
    g.drawString("S", cx, cy + R + 6);
    g.drawString("E", cx + R + 6, cy);
    g.drawString("W", cx - R - 6, cy);
    if (el < 0) return;
    double rr = (90.0 - el) / 90.0 * R;
    double a  = az * M_PI / 180.0;        // 0=N, clockwise
    int px = cx + (int)round(rr * sin(a));
    int py = cy - (int)round(rr * cos(a));
    g.drawLine(cx, cy, px, py, TFT_GREEN);
    g.fillCircle(px, py, 4, TFT_GREEN);
}

// Format the clock in LOCAL time (UTC + settings.tzOffsetMin), with the offset
// shown so it's clear which zone it is. Tracking still uses UTC internally.
static void fmtClock(time_t utc, char* out) {
    int    om    = settings.tzOffsetMin;
    time_t local = utc + (time_t)om * 60;
    struct tm tmv; gmtime_r(&local, &tmv);
    int  ao = om < 0 ? -om : om;
    char sg = om < 0 ? '-' : '+';
    if (ao % 60)
        sprintf(out, "%02d:%02d:%02d UTC%c%d:%02d", tmv.tm_hour, tmv.tm_min,
                tmv.tm_sec, sg, ao / 60, ao % 60);
    else
        sprintf(out, "%02d:%02d:%02d UTC%c%d", tmv.tm_hour, tmv.tm_min,
                tmv.tm_sec, sg, ao / 60);
}

// Local date (YYYY-MM-DD) for the footer, using the same tz offset as the clock.
static void fmtDate(time_t utc, char* out) {
    time_t local = utc + (time_t)settings.tzOffsetMin * 60;
    struct tm tmv; gmtime_r(&local, &tmv);
    sprintf(out, "%04d-%02d-%02d", tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday);
}

// Battery % centered at (160,y): red when low, green when charging. Skips if unknown.
static void drawBatteryCentered(LovyanGFX& g, int y, uint16_t bg) {
    int lvl = M5.Power.getBatteryLevel();
    if (lvl < 0) return;
    bool charging = (M5.Power.isCharging() == m5::Power_Class::is_charging);
    char b[16]; sprintf(b, "batt %d%%%s", lvl, charging ? " +" : "");
    g.setTextSize(1);
    g.setTextDatum(top_center);
    // Low + not actively charging (discharging OR unknown) -> red, matching the
    // conservative direction the relax guard uses; only real charging shows green.
    g.setTextColor((lvl < LOW_BATT_PCT && !charging) ? TFT_RED
                   : (charging ? TFT_GREENYELLOW : TFT_LIGHTGREY), bg);
    g.drawString(b, 160, y);
}

void drawTracking(const Target& t, float pan, float tilt,
                  bool inRange, time_t utc, int satCount) {
    LovyanGFX& g = hud ? hudG() : *(LovyanGFX*)&M5.Display;
    g.fillScreen(TFT_BLACK);

    // name bar, colored by visibility
    uint16_t bar = t.vis > 0 ? TFT_DARKGREEN : (t.vis == 0 ? TFT_NAVY : TFT_MAROON);
    g.fillRect(0, 0, 320, 28, bar);
    g.setTextColor(TFT_WHITE, bar);
    g.setTextDatum(middle_center);
    g.setTextSize(2);
    g.drawString(t.name, 160, 14);

    // browse buttons over the name-bar corners: prev (left), next (right)
    g.fillRoundRect(0, 0, 30, 28, 4, TFT_BLACK);
    g.fillTriangle(20, 5, 20, 23, 7, 14, TFT_WHITE);      // left chevron
    g.fillRoundRect(290, 0, 30, 28, 4, TFT_BLACK);
    g.fillTriangle(300, 5, 300, 23, 313, 14, TFT_WHITE);  // right chevron

    // telemetry column (left)
    char line[40];
    g.setTextDatum(top_left);
    g.setTextSize(2);
    int ty = 36;
    auto row = [&](const char* k, const String& v, uint16_t c) {
        g.setTextColor(TFT_DARKGREY, TFT_BLACK); g.drawString(k, 6, ty);
        g.setTextColor(c, TFT_BLACK);            g.drawString(v, 70, ty);
        ty += 24;
    };
    row("AZ",  String(t.az, 1) + (char)247, TFT_WHITE);
    row("EL",  String(t.el, 1) + (char)247, t.el > 0 ? TFT_GREEN : TFT_RED);
    row("RNG", String((int)(t.rangeKm * KM_TO_MI)) + " mi", TFT_WHITE);
    row("ALT", String((int)(t.altKm  * KM_TO_MI)) + " mi", TFT_WHITE);
    row("VEL", String((int)(t.velKms * 3600.0 * KM_TO_MI)) + " mph", TFT_WHITE);

    // satellite identity icon (top-right) + type label
    SatIcon::Type it = SatIcon::classify(t.name);
    SatIcon::draw(g, it, 262, 64, 56);
    g.setTextDatum(top_center);
    g.setTextSize(1);
    g.setTextColor(TFT_DARKGREY, TFT_BLACK);
    g.drawString(SatIcon::typeName(it), 262, 96);

    // polar sky plot (lower-right)
    drawPolar(g, 262, 156, 30, t.az, t.el);

    // sub-point + visibility
    g.setTextSize(1);
    g.setTextColor(TFT_CYAN, TFT_BLACK);
    g.setTextDatum(top_left);
    sprintf(line, "sub-point %.2f, %.2f", t.subLat, t.subLon);
    g.drawString(line, 6, 168);
    g.setTextColor(t.vis > 0 ? TFT_GREENYELLOW : TFT_ORANGE, TFT_BLACK);
    g.drawString(Sky::visText(t.vis), 6, 182);

    // footer
    g.fillRect(0, 198, 320, 42, 0x10A2);
    g.setTextColor(TFT_WHITE, 0x10A2);
    g.setTextDatum(top_left);
    char clk[24]; fmtClock(utc, clk);
    sprintf(line, "pan %3d  tilt %2d", (int)pan, (int)tilt);
    g.drawString(line, 6, 204);
    g.drawString(clk, 6, 222);

    // battery (above) + date, centered between the clock and the IP
    drawBatteryCentered(g, 204, 0x10A2);
    if (utc > 0) {
        char dt[12]; fmtDate(utc, dt);
        g.setTextColor(TFT_WHITE, 0x10A2);
        g.setTextDatum(top_center);
        g.drawString(dt, 160, 222);
    }

    // right column: sat count, then the IP address in the bottom-right corner
    g.setTextDatum(top_right);
    sprintf(line, "sats:%d", satCount);
    g.drawString(line, 314, 204);
    String ip = WiFi.localIP().toString();
    if (ip == "0.0.0.0") ip = "offline";
    g.setTextColor(TFT_GREENYELLOW, 0x10A2);
    g.drawString(ip, 314, 222);

    // out-of-range alert (centered on the first footer line)
    if (!inRange) {
        g.setTextColor(TFT_RED, 0x10A2);
        g.setTextDatum(top_center);
        g.drawString("OUT OF RANGE", 160, 204);
    }
    hudFlush();
}

void drawTest(float cmdPan, float cmdTilt, float actPan, float actTilt) {
    LovyanGFX& g = hud ? hudG() : *(LovyanGFX*)&M5.Display;
    g.fillScreen(TFT_BLACK);
    g.setTextDatum(top_center);
    g.setTextColor(TFT_ORANGE, TFT_BLACK);
    g.setTextSize(2);
    g.drawString("MOTOR TEST", 160, 16);
    g.setTextColor(TFT_DARKGREY, TFT_BLACK);
    g.setTextSize(1);
    g.drawString("commanded  /  actual", 160, 44);

    char line[32];
    g.setTextSize(2);
    // A read pinned at the servo's limit extreme means the bus got no answer.
    bool panOff  = actPan  < -100.0f;
    bool tiltOff = actTilt < -100.0f;
    g.setTextColor(panOff || fabsf(cmdPan - actPan) > 8 ? TFT_RED : TFT_WHITE, TFT_BLACK);
    if (panOff) sprintf(line, "PAN  %d / no resp", (int)cmdPan);
    else        sprintf(line, "PAN  %d / %d", (int)cmdPan, (int)actPan);
    g.drawString(line, 160, 70);
    g.setTextColor(tiltOff || fabsf(cmdTilt - actTilt) > 8 ? TFT_RED : TFT_WHITE, TFT_BLACK);
    if (tiltOff) sprintf(line, "TILT %d / no resp", (int)cmdTilt);
    else         sprintf(line, "TILT %d / %d", (int)cmdTilt, (int)actTilt);
    g.drawString(line, 160, 108);

    g.setTextColor(TFT_DARKGREY, TFT_BLACK);
    g.setTextSize(1);
    g.drawString("red = motor not following the command", 160, 150);
    g.drawString("tracking paused - drive from the web page", 160, 184);
    String ip = WiFi.localIP().toString();
    if (ip == "0.0.0.0") ip = "offline";
    g.setTextDatum(top_right);
    g.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
    g.drawString(ip, 314, 224);
    hudFlush();
}

void drawSearching(time_t utc, int satCount, const Sky::NextPass& next) {
    LovyanGFX& g = hud ? hudG() : *(LovyanGFX*)&M5.Display;
    g.fillScreen(TFT_BLACK);
    g.setTextDatum(top_center);
    g.setTextColor(TFT_YELLOW, TFT_BLACK);
    g.setTextSize(2);
    g.drawString("Scanning the sky", 160, 28);

    if (next.valid) {                       // a reachable pass -> count down to it
        g.setTextColor(TFT_DARKGREY, TFT_BLACK);
        g.setTextSize(1);
        g.drawString("next pass", 160, 66);
        g.setTextColor(TFT_CYAN, TFT_BLACK);
        g.setTextSize(2);
        g.drawString(next.name, 160, 86);

        long secs = (long)(next.aosUnix - utc); if (secs < 0) secs = 0;
        char cd[16];
        if (secs >= 3600) sprintf(cd, "%ld:%02ld:%02ld", secs / 3600, (secs / 60) % 60, secs % 60);
        else              sprintf(cd, "%02ld:%02ld", secs / 60, secs % 60);
        g.setTextColor(TFT_WHITE, TFT_BLACK);
        g.setTextSize(4);
        g.drawString(cd, 160, 128);

        char ml[28]; sprintf(ml, "max elevation %d%c", (int)next.maxEl, (char)247);
        g.setTextColor(TFT_DARKGREY, TFT_BLACK);
        g.setTextSize(1);
        g.drawString(ml, 160, 180);
    } else if (!next.inReach && next.name[0]) {  // passes exist, but none pointable
        g.setTextColor(TFT_ORANGE, TFT_BLACK);
        g.setTextSize(1);
        g.drawString("next pass is out of reach:", 160, 80);
        g.setTextColor(TFT_CYAN, TFT_BLACK);
        g.setTextSize(2);
        g.drawString(next.name, 160, 100);
        g.setTextColor(TFT_DARKGREY, TFT_BLACK);
        g.setTextSize(1);
        g.drawString("widen reach limits / check facing", 160, 132);
    } else {
        g.setTextColor(TFT_DARKGREY, TFT_BLACK);
        g.setTextSize(1);
        g.drawString("no upcoming passes in view", 160, 110);
    }

    // footer - row 1 (y208): sat count (left) + battery (centre).
    g.setTextSize(1);
    g.setTextColor(TFT_DARKGREY, TFT_BLACK);
    g.setTextDatum(top_left);
    char sl[16]; sprintf(sl, "%d sats", satCount);
    g.drawString(sl, 6, 208);
    drawBatteryCentered(g, 208, TFT_BLACK);
    // row 2 (y224): clock (left) + date (centre) + IP (right) - clock alone is
    // short enough that it no longer collides with the centred date.
    char clk[24]; fmtClock(utc, clk);
    g.setTextColor(TFT_DARKGREY, TFT_BLACK);
    g.setTextDatum(top_left);
    g.drawString(clk, 6, 224);
    if (utc > 0) {
        char dt[12]; fmtDate(utc, dt);
        g.setTextDatum(top_center);
        g.drawString(dt, 160, 224);
    }
    String ip = WiFi.localIP().toString();
    if (ip == "0.0.0.0") ip = "offline";
    g.setTextDatum(top_right);
    g.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
    g.drawString(ip, 314, 224);
    hudFlush();
}

} // namespace UI
