// shared/rpcinterface.h — Shared RPC interface between Mainboard (RPi4) and Pico
// Used with corocrpc: rpc.call(METHOD_ID, arg) / rpc.registerMethod(METHOD_ID, handler).
// Argument layout documented per method as: args: <type> <name>, ... | returns: <type>
#pragma once
#include <cstdint>

// ── Pico2Main method IDs ─────────────────────────────────────────────────
// Methods that Pico calls on Main (Main registers the handlers as server).

enum Pico2MainMethod : uint16_t {
    P2M_PING        = 1, /* args: int32 val                                   | returns: int32 val */
    P2M_DEBUG_PRINT = 2, /* args: string message                              | returns: void */
    P2M_ON_BOOT     = 3, /* args: string picoId, uint32 configCrc32           | returns: bool success */
};

// ── Main2Pico method IDs ─────────────────────────────────────────────────
// Methods that Main calls on Pico (Pico registers the handlers as server).

enum Main2PicoMethod : uint16_t {
    M2P_PING              = 1, /* args: int32 val                             | returns: int32 val */
    M2P_SET_LED           = 2, /* args: bool state                            | returns: void */
    M2P_GET_LED_STATUS    = 3, /* args: void                                  | returns: bool state */
    M2P_REBOOT_FLASH_MODE = 4, /* args: void                                  | returns: bool */
    M2P_REBOOT            = 5, /* args: void                                  | returns: void (Pico reboots) */
    M2P_SET_AXIS          = 6, /* args: int32 device, int32 axis, int32 value | returns: void */
    // 7 = M2P_SET_MODE — REMOVED (mode is now config-driven)
    // 8 = M2P_GET_MODE — REMOVED (mode is now config-driven)
    M2P_SET_CONFIGURATION = 9, /* args: string configJson
                                  returns: bool ok, string errorMsg
                                  Encoding: putBool(ok), putString(errorMsg)
                                  On success: Pico replies {true,""} then reboots.
                                  On failure: Pico replies {false, reason}, no reboot. */
};
