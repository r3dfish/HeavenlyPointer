// ============================================================================
//  Provision.cpp
//
//  Two ways to hand the tracker your WiFi credentials:
//    (A) Scan the on-screen QR with a phone -> join the "HeavenlyPointer"
//        network -> a captive form pops up -> type SSID + password there.
//    (B) Tap "Touch entry" -> pick the network on-screen -> type the
//        password with the on-screen keyboard.
// ============================================================================
#include "Provision.h"
#include "Settings.h"
#include "UI.h"
#include "config.h"
#include <M5Unified.h>      // for TFT_* color macros used in UI::status()
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

namespace Provision {
namespace {

WebServer  server(80);
DNSServer  dns;

bool   captured = false;
String capSsid, capPass;
double capLat = 0, capLon = 0;
bool   capHasLoc = false;
String s_ssidOptions;          // cached <option> list from a WiFi site survey

// The form is built dynamically so the SSID list comes from a live WiFi scan.
const char* FORM_HEAD = R"HTML(<!doctype html><html><head><meta name=viewport content="width=device-width,initial-scale=1">
<title>HeavenlyPointer setup</title>
<style>body{font-family:sans-serif;background:#101418;color:#eee;margin:0;padding:20px}
h2{color:#3cc} input,select{width:100%;padding:10px;margin:6px 0;border-radius:6px;border:1px solid #444;background:#1b2128;color:#fff;box-sizing:border-box}
button{width:100%;padding:12px;margin-top:12px;border:0;border-radius:6px;background:#2a7;color:#fff;font-size:16px}
small{color:#789}</style></head><body>
<h2>&#128225; HeavenlyPointer setup</h2>
<form action="/save" method="post">
<label>WiFi network</label>
<select name="ssidsel" onchange="document.getElementById('oth').style.display=this.value?'none':'block'">
)HTML";

const char* FORM_TAIL = R"HTML(
<label>Password</label><input name="pass" type="password">
<small>Optional &mdash; override auto-located position:</small>
<input name="lat" placeholder="Latitude (deg, +N)">
<input name="lon" placeholder="Longitude (deg, +E)">
<button type="submit">Connect</button></form></body></html>
)HTML";

// Escape the few HTML-significant chars that can appear in an SSID.
String htmlEscape(const String& s) {
    String o; o.reserve(s.length() + 8);
    for (size_t i = 0; i < s.length(); i++) {
        char c = s[i];
        if      (c == '&') o += "&amp;";
        else if (c == '<') o += "&lt;";
        else if (c == '>') o += "&gt;";
        else if (c == '"') o += "&quot;";
        else               o += c;
    }
    return o;
}

// One-time WiFi site survey -> cached <option> list (de-duplicated by SSID).
void scanNetworks() {
    s_ssidOptions = "";
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n && i < 24; i++) {
        String s = WiFi.SSID(i);
        if (s.isEmpty()) continue;
        String tag = ">" + htmlEscape(s) + "<";
        if (s_ssidOptions.indexOf(tag) >= 0) continue;       // already listed
        s_ssidOptions += "<option>" + htmlEscape(s) + "</option>";
    }
    // Keep the raw results in memory (we're in WIFI_AP_STA) so the on-screen
    // touch picker can reuse THIS scan instead of running a second one.
}

String buildForm() {
    String othStyle = s_ssidOptions.length() ? "display:none" : "display:block";
    String h; h.reserve(900 + s_ssidOptions.length());
    h += FORM_HEAD;
    h += s_ssidOptions;
    h += "<option value=''>Other / hidden network&hellip;</option></select>";
    h += "<input name=\"ssidother\" id=\"oth\" placeholder=\"Network name\" style=\"" + othStyle + "\">";
    h += "<a href=\"/rescan\" style=\"display:block;text-align:center;padding:11px;margin:8px 0;"
         "border-radius:6px;background:#243b44;color:#9eeaff;text-decoration:none;font-size:15px\">"
         "&#8635; Rescan networks</a>";
    h += FORM_TAIL;
    return h;
}

void handleRoot() { server.send(200, "text/html", buildForm()); }

// Send a 302 back to the portal root.
void redirectToPortal() {
    server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/");
    server.send(302, "text/plain", "");
}

// Bounce any other path to the portal root (captive-portal detection) without
// re-scanning WiFi on every OS probe.
void handleNotFound() { redirectToPortal(); }

// Explicit user "Rescan" link: fresh site survey, then reload the form.
void handleRescan() { scanNetworks(); redirectToPortal(); }

void handleSave() {
    capSsid = server.arg("ssidsel"); capSsid.trim();          // picked from the survey
    if (capSsid.isEmpty()) { capSsid = server.arg("ssidother"); capSsid.trim(); }  // or typed
    capPass = server.arg("pass");
    String la = server.arg("lat"), lo = server.arg("lon");
    if (la.length() && lo.length()) {
        capLat = la.toDouble(); capLon = lo.toDouble(); capHasLoc = true;
    }
    captured = capSsid.length() > 0;
    server.send(200, "text/html",
        "<html><body style='font-family:sans-serif;background:#101418;color:#3e7'>"
        "<h2>&#9989; Saved!</h2><p>Stack-chan is connecting. You can close this "
        "page.</p></body></html>");
}

} // anonymous namespace

bool run(bool offlineCapable) {
    captured = false; capHasLoc = false;
    bool offline = false;
    UI::resetProvisionCache();            // force a fresh paint, even on re-entry

    WiFi.mode(WIFI_AP_STA);
    WiFi.disconnect(false, true);         // drop any leftover STA connect (e.g. after a wrong
    WiFi.setAutoReconnect(false);         // password) so the scanner isn't starved
    WiFi.softAP(AP_SSID);                 // open network
    IPAddress ip = WiFi.softAPIP();       // typically 192.168.4.1

    UI::status("Scanning WiFi...", TFT_YELLOW);
    delay(150);                           // let the radio settle after the disconnect
    scanNetworks();                       // site survey for the setup form's list

    dns.start(53, "*", ip);               // captive-portal DNS catch-all
    server.on("/", handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/save", HTTP_GET, handleSave);
    server.on("/rescan", handleRescan);   // re-run the site survey on demand
    server.onNotFound(handleNotFound);    // 302 -> portal (captive popup)
    server.begin();

    // QR that joins the open setup AP. Scanning it makes the phone connect,
    // after which the captive form appears automatically.
    String joinQr  = String("WIFI:T:nopass;S:") + AP_SSID + ";;";
    String portalUrl = String("http://") + ip.toString();

    while (!captured && !offline) {
        dns.processNextRequest();
        server.handleClient();

        // drawProvision returns: 1 = Touch entry, 2 = Offline mode, else 0.
        int action = UI::drawProvision(joinQr.c_str(), portalUrl.c_str(), AP_SSID, offlineCapable);
        if (action == 1) {
            // Sub-flow: pick network -> password. The password screen's Back
            // button returns here so a wrong network can be re-picked.
            while (true) {
                String ssid;
                UI::pickSSID(ssid);
                if (ssid.isEmpty()) ssid = UI::keyboard("Enter WiFi name (SSID)", false);
                if (ssid.isEmpty()) break;            // backed out -> return to QR screen
                bool cancelled = false;
                String pass = UI::keyboard("Password", true, ssid.c_str(), &cancelled);
                if (cancelled) continue;              // wrong network -> re-pick
                capSsid = ssid; capPass = pass; captured = true;
                break;
            }
        } else if (action == 2) {
            offline = true;                           // run on cached data, no WiFi
        }
        delay(5);
    }

    // Tear down the AP & servers before we leave the portal.
    server.stop();
    dns.stop();
    WiFi.softAPdisconnect(true);

    if (offline) {                                    // keep saved creds untouched
        WiFi.mode(WIFI_OFF);                          // no radio needed offline -> saves power
        UI::status("Starting offline...", TFT_CYAN);
        return true;
    }
    WiFi.mode(WIFI_STA);

    // Persist captured credentials.
    settings.ssid = capSsid;
    settings.pass = capPass;
    if (capHasLoc) {
        settings.lat = capLat; settings.lon = capLon;
        settings.altM = 0; settings.hasLocation = true;
    }
    settings.save();

    UI::status("WiFi saved", TFT_GREEN);
    return false;
}

} // namespace Provision
