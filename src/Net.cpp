// ============================================================================
//  Net.cpp
// ============================================================================
#include "Net.h"
#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <M5Unified.h>
#include <time.h>

namespace Net {

bool isConnected() { return WiFi.status() == WL_CONNECTED; }

bool connect(const String& ssid, const String& pass, uint32_t timeoutMs) {
    if (ssid.isEmpty()) return false;
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
        delay(200);
    }
    bool ok = (WiFi.status() == WL_CONNECTED);
    if (ok) {
        // Good link: re-enable self-heal so a transient AP drop reconnects on its
        // own (the disable below is scoped to the failure/provisioning teardown,
        // and is sticky, so it must be undone once we actually connect).
        WiFi.setAutoReconnect(true);
    } else {
        // Tear the failed attempt down: otherwise the STA keeps auto-retrying
        // the bad credentials in the background, which starves WiFi.scanNetworks()
        // when we fall back to provisioning (scans come up empty until a reboot).
        WiFi.disconnect(false, true);     // keep radio on, erase the stored AP config
        WiFi.setAutoReconnect(false);
    }
    return ok;
}

time_t nowUtc() {
    time_t t = time(nullptr);
    return (t < 1700000000) ? 0 : t;   // ~2023-11; below this the clock is unset
}

bool syncTime(uint32_t timeoutMs) {
    configTime(0, 0, NTP_SERVER_1, NTP_SERVER_2);   // 0 offset -> UTC
    uint32_t start = millis();
    while (millis() - start < timeoutMs) {
        if (time(nullptr) > 1700000000) {
            // Push the synced UTC time into the CoreS3 RTC (BM8563) so the
            // clock keeps reasonable time across brief power blips.
            struct tm tmv;
            time_t t = time(nullptr);
            gmtime_r(&t, &tmv);
            m5::rtc_datetime_t dt;
            dt.date.year  = tmv.tm_year + 1900;
            dt.date.month = tmv.tm_mon + 1;
            dt.date.date  = tmv.tm_mday;
            dt.time.hours = tmv.tm_hour;
            dt.time.minutes = tmv.tm_min;
            dt.time.seconds = tmv.tm_sec;
            M5.Rtc.setDateTime(dt);
            return true;
        }
        delay(250);
    }
    return false;
}

bool geolocate(double& lat, double& lon, double& altM, int& offsetSec, String& city) {
    offsetSec = 0; city = "";
    if (!isConnected()) return false;
    HTTPClient http;
    // Plain HTTP, no key. "offset" is the local UTC offset in seconds (with DST).
    if (!http.begin("http://ip-api.com/json/?fields=status,lat,lon,offset,city"))
        return false;
    int code = http.GET();
    if (code != HTTP_CODE_OK) { http.end(); return false; }
    String body = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, body)) return false;
    if (String(doc["status"] | "fail") != "success") return false;   // ip-api uses "success"
    lat       = doc["lat"] | 0.0;
    lon       = doc["lon"] | 0.0;
    altM      = 0.0;       // IP geolocation has no altitude; sea level is fine
    offsetSec = doc["offset"] | 0;
    city      = String(doc["city"] | "");
    return (lat != 0.0 || lon != 0.0);
}

int fetchTLEs(const String& group) {
    if (!isConnected()) return -1;

    String url = "https://celestrak.org/NORAD/elements/gp.php?GROUP=" +
                 group + "&FORMAT=tle";

    WiFiClientSecure client;
    client.setInsecure();             // skip cert pinning on a constrained MCU
    HTTPClient http;
    http.setTimeout(20000);
    if (!http.begin(client, url)) return -1;
    http.addHeader("User-Agent", "HeavenlyPointer/1.0");

    int code = http.GET();
    if (code != HTTP_CODE_OK) { http.end(); return -1; }

    // Stream the TLE text straight to flash so we don't buffer the whole
    // catalog in RAM. Cap at MAX_SATS (3 lines per satellite).
    File f = LittleFS.open(TLE_FILE_PATH, "w");
    if (!f) { http.end(); return -1; }

    WiFiClient* stream = http.getStreamPtr();
    uint8_t   buf[256];
    int       lineCount = 0;
    int       sats = 0;
    String    line;
    uint32_t  lastData = millis();
    while (sats < MAX_SATS) {
        int avail = stream->available();
        if (avail > 0) {
            int n = stream->read(buf, avail < (int)sizeof(buf) ? avail : (int)sizeof(buf));
            lastData = millis();
            for (int i = 0; i < n && sats < MAX_SATS; i++) {
                char c = (char)buf[i];
                if (c == '\r') continue;
                if (c == '\n') {
                    line.trim();
                    if (line.length()) {
                        f.println(line);
                        if (++lineCount % 3 == 0) sats++;
                    }
                    line = "";
                } else {
                    line += c;
                }
            }
        } else {
            if (!stream->connected()) break;          // server done & drained
            if (millis() - lastData > 8000) break;    // stall guard
            delay(2);
        }
    }
    // flush any trailing line (no newline at EOF)
    line.trim();
    if (line.length()) { f.println(line); if (++lineCount % 3 == 0) sats++; }
    f.close();
    http.end();
    return sats;
}

} // namespace Net
