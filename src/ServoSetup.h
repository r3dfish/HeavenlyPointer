// ============================================================================
//  ServoSetup.h  -  One-time on-screen tool to set Feetech bus-servo IDs.
//
//  The BSP addresses the pan servo as ID 1 and the tilt servo as ID 2 and does
//  NOT program IDs - a replacement servo ships as ID 1, so it must be re-IDed.
//  This tool borrows the serial bus (the BSP owns it), scans, and writes the ID.
//  Because servos sharing an ID are indistinguishable on the bus, ID changes
//  require exactly ONE servo connected; the tool reboots when done.
// ============================================================================
#pragma once

namespace ServoSetup {
    bool pending();           // was the setup screen requested (NVS flag)?
    void requestAndReboot();  // set the flag and restart into the setup screen
    void run();               // interactive on-screen tool; never returns (reboots)
}
