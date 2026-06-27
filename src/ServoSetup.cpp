// ============================================================================
//  ServoSetup.cpp  -  Interactive Feetech bus-servo ID tool (on-screen).
// ============================================================================
#include "ServoSetup.h"
#include <M5Unified.h>
#include <M5StackChan.h>
#include <Preferences.h>
#include "driver/uart.h"
#include <drivers/FTServo_Arduino/src/SCSCL.h>   // bundled by StackChan-BSP

namespace ServoSetup {
namespace {

// Must match the BSP's bus wiring (M5StackChan.cpp): UART1, 1 Mbps, pins 6/7.
constexpr uart_port_t BUS_UART = UART_NUM_1;
constexpr int BUS_BAUD = 1000000, BUS_TX = 6, BUS_RX = 7;
constexpr uint8_t REG_ID = SCSCL_ID;   // EEPROM register 5

SCSCL  s_bus;
String s_msg = "";
uint16_t s_msgColor = TFT_WHITE;

bool prefRun(bool* set = nullptr) {
    Preferences p;
    p.begin("servosetup", false);
    if (set) p.putBool("run", *set);
    bool v = p.getBool("run", false);
    p.end();
    return v;
}

// Ping IDs 1..20; fill out[] with those that answer. Returns count.
int scan(uint8_t* out, int maxOut) {
    int n = 0;
    for (uint8_t id = 1; id <= 20 && n < maxOut; id++) {
        if (s_bus.Ping(id) >= 0) out[n++] = id;
        delay(6);
    }
    return n;
}

// Simple filled-button + centred label; returns true if (tx,ty) is inside.
bool button(int x, int y, int w, int h, const char* label, uint16_t bg, int tx, int ty) {
    M5.Display.fillRoundRect(x, y, w, h, 5, bg);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextColor(TFT_WHITE, bg);
    M5.Display.setTextSize(2);
    M5.Display.drawString(label, x + w / 2, y + h / 2);
    return tx >= x && tx < x + w && ty >= y && ty < y + h;
}

void draw(const uint8_t* ids, int n) {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextDatum(top_center);
    M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
    M5.Display.setTextSize(2);
    M5.Display.drawString("Servo ID setup", 160, 6);

    M5.Display.setTextSize(1);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    String found = "On bus: ";
    if (n == 0) found += "(none)";
    else for (int i = 0; i < n; i++) { found += ids[i]; if (i < n - 1) found += ", "; }
    M5.Display.drawString(found, 160, 30);
    M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
    M5.Display.drawString("Target: pan = 1, tilt = 2", 160, 44);

    M5.Display.setTextColor(TFT_ORANGE, TFT_BLACK);
    M5.Display.drawString("ID change: 1 servo only  |  Center: both, aim first", 160, 62);

    if (s_msg.length()) {
        M5.Display.setTextColor(s_msgColor, TFT_BLACK);
        M5.Display.drawString(s_msg, 160, 78);
    }
}

void setId(const uint8_t* ids, int n, uint8_t target) {
    if (n == 0) { s_msg = "No servo found - check power/wiring"; s_msgColor = TFT_RED; return; }
    if (n > 1)  { s_msg = "Multiple servos - unplug all but one"; s_msgColor = TFT_RED; return; }
    uint8_t from = ids[0];
    if (from == target) { s_msg = String("Already ID ") + target; s_msgColor = TFT_GREEN; return; }
    s_bus.unLockEprom(from);
    int r = s_bus.writeByte(from, REG_ID, target);
    s_bus.LockEprom(target);
    delay(40);
    // Ack returns 1 on success / 0 on any failure (never -1); confirm the servo
    // actually moved to the new ID (and is gone from the old one).
    if (r == 1 && s_bus.Ping(target) >= 0 && s_bus.Ping(from) < 0) {
        s_msg = String("Set ") + from + " -> " + target + "  OK"; s_msgColor = TFT_GREEN;
    } else {
        s_msg = String("Set ") + from + " -> " + target + " FAILED"; s_msgColor = TFT_RED;
    }
}

// De-energize both servos so the head can be positioned by hand.
void relax() {
    s_bus.EnableTorque(1, 0);
    s_bus.EnableTorque(2, 0);
}

// Capture the current shaft positions of pan(1)+tilt(2) as the new "zero" and
// persist to the SAME NVS the BSP reads at boot (namespace "servo", i32 keys).
void setCenter() {
    uint8_t ids[12];
    int n = scan(ids, 12);
    bool h1 = false, h2 = false;
    for (int i = 0; i < n; i++) { if (ids[i] == 1) h1 = true; if (ids[i] == 2) h2 = true; }
    if (!(h1 && h2)) { s_msg = "Need pan(1) + tilt(2) connected"; s_msgColor = TFT_RED; return; }

    relax();
    for (int c = 3; c >= 1; c--) {        // give the user time to aim + hold the head
        s_msg = String("Hold head at center... ") + c; s_msgColor = TFT_YELLOW;
        draw(ids, n);
        delay(1000);
    }
    int p1 = s_bus.ReadPos(1), p2 = s_bus.ReadPos(2);
    if (p1 < 0 || p2 < 0)        { s_msg = "Position read failed - retry"; s_msgColor = TFT_RED; return; }
    if (p1 > 1000 || p2 > 1000)  { s_msg = "Out of range - re-seat horn";  s_msgColor = TFT_RED; return; }

    Preferences pr;
    pr.begin("servo", false);
    pr.putInt("zero_pos_1", p1);   // yaw  (matches BSP settingZeroPositionKey)
    pr.putInt("zero_pos_2", p2);   // tilt
    pr.end();
    s_msg = String("Center saved: ") + p1 + ", " + p2; s_msgColor = TFT_GREEN;
}

} // namespace

bool pending() { return prefRun(); }

void requestAndReboot() {
    bool on = true; prefRun(&on);
    delay(80);
    ESP.restart();
}

void run() {
    bool off = false; prefRun(&off);    // clear the flag first so a crash can't loop

    // CRITICAL: the BSP runs a motion_task that polls this same UART every 20ms
    // (auto torque-release). Quiesce it BEFORE we touch the driver. setAuto*()
    // take the same mutex update() holds, so once they return no bus transaction
    // is in flight and future ticks do no bus I/O - making the delete + reopen
    // safe and giving us exclusive use of the bus.
    M5StackChan.Motion.setAutoTorqueReleaseEnabled(false);
    M5StackChan.Motion.setAutoAngleSyncEnabled(false);
    delay(80);

    // Take the bus from the BSP (it already installed UART1 + powered the rail).
    uart_driver_delete(BUS_UART);
    if (!s_bus.begin(BUS_UART, BUS_BAUD, BUS_TX, BUS_RX)) {
        M5.Display.fillScreen(TFT_BLACK);
        M5.Display.setTextDatum(middle_center);
        M5.Display.setTextColor(TFT_RED, TFT_BLACK);
        M5.Display.setTextSize(2);
        M5.Display.drawString("Bus init failed", 160, 108);
        M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
        M5.Display.setTextSize(1);
        M5.Display.drawString("tap to reboot", 160, 150);
        for (;;) {
            M5.update();
            if (M5.Touch.getCount()) { auto d = M5.Touch.getDetail(); if (d.wasPressed()) ESP.restart(); }
            delay(20);
        }
    }
    delay(50);

    uint8_t ids[12];
    int n = scan(ids, 12);
    relax();                            // head limp so it can be hand-positioned

    for (;;) {
        draw(ids, n);
        // Buttons (drawn here so their hit-boxes match what's shown).
        int d = -1;
        button(12,  92, 140, 40, "ID -> 1",       TFT_NAVY,      d, d);
        button(168, 92, 140, 40, "ID -> 2",       TFT_NAVY,      d, d);
        button(12, 138, 140, 40, "Set center",    TFT_PURPLE,    d, d);
        button(168,138, 140, 40, "Rescan",        TFT_DARKGREY,  d, d);
        button(12, 184, 296, 40, "Done & reboot", TFT_DARKGREEN, d, d);

        // Wait for a tap.
        int tx = -1, ty = -1;
        while (tx < 0) {
            M5.update();
            if (M5.Touch.getCount()) {
                auto dt = M5.Touch.getDetail();
                if (dt.wasPressed()) { tx = dt.x; ty = dt.y; }
            }
            delay(10);
        }

        auto hit = [&](int x, int y, int w, int h) { return tx >= x && tx < x + w && ty >= y && ty < y + h; };
        if      (hit(12,  92, 140, 40)) { setId(ids, n, 1); n = scan(ids, 12); relax(); }
        else if (hit(168, 92, 140, 40)) { setId(ids, n, 2); n = scan(ids, 12); relax(); }
        else if (hit(12, 138, 140, 40)) { setCenter();      n = scan(ids, 12); }
        else if (hit(168,138, 140, 40)) { s_msg = "";       n = scan(ids, 12); relax(); }
        else if (hit(12, 184, 296, 40)) { ESP.restart(); }
    }
}

} // namespace ServoSetup
