// ============================================================================
//  WebControl.h  -  HTTP server for browser control of the tracker.
//
//  Serves a phone-friendly page (live telemetry + satellite configuration) and
//  a small JSON API. Runs on the main loop thread alongside tracking, so config
//  edits apply without locking; only the blocking TLE re-download is deferred
//  to trackTick() via g_tleRefetchNeeded.
// ============================================================================
#pragma once

namespace WebControl {
    void begin();    // start the server once WiFi is connected (idempotent)
    void handle();   // pump the server each loop iteration (no-op until begun)
    bool started();
}
