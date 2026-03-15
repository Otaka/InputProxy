// rpcinterface.h - Shared RPC interface between Mainboard (RPi4) and Pico
// Used with corocrpc: rpc.call(METHOD_ID, arg) / rpc.registerMethod(METHOD_ID, handler).
// Argument layout documented per method as: args: <type> <name>, ... | returns: <type>
#pragma once
#include <cstdint>

// ── Device configuration structures ──────────────────────────────────────

struct HidGamepadConfig {
    uint8_t  hat;
    uint16_t axesMask;
    uint8_t  buttons;
};

struct DeviceConfiguration {
    int deviceType;  // 0=Keyboard, 1=Mouse, 2=HID Gamepad, 3=Xbox360 Gamepad
    union {
        HidGamepadConfig hidGamepadConfig;  // valid when deviceType == 2
    } config;
};

// ── Pico2Main method IDs ─────────────────────────────────────────────────
// Methods that Pico calls on Main (Main registers the handlers as server).

enum Pico2MainMethod : uint16_t {
    P2M_PING        = 1,  /* args: int32 val                             | returns: int32 val */
    P2M_DEBUG_PRINT = 2,  /* args: string message                        | returns: void */
    P2M_ON_BOOT     = 3,  /* args: string serialString, int32 deviceMode | returns: bool success */
};

// ── Main2Pico method IDs ─────────────────────────────────────────────────
// Methods that Main calls on Pico (Pico registers the handlers as server).

enum Main2PicoMethod : uint16_t {
    M2P_PING              = 1,   /* args: int32 val                                           | returns: int32 val */
    M2P_SET_LED           = 2,   /* args: bool state                                          | returns: void */
    M2P_GET_LED_STATUS    = 3,   /* args: void                                                | returns: bool state */
    M2P_REBOOT_FLASH_MODE = 4,   /* args: void                                                | returns: bool (always true; triggers USB boot) */
    M2P_REBOOT            = 5,   /* args: void                                                | returns: void (no reply; Pico reboots) */
    M2P_SET_AXIS          = 6,   /* args: int32 device, int32 axis, int32 value               | returns: void */
    M2P_SET_MODE          = 7,   /* args: int32 mode (0=HID, 1=XInput)                       | returns: void (saves to flash, reboots) */
    M2P_GET_MODE          = 8,   /* args: void                                                | returns: int32 mode */
    M2P_PLUG_DEVICE       = 9,   /* args: int32 slotIndex, int32 deviceType, int32 hat, int32 axesMask, int32 buttons | returns: bool success */
    M2P_UNPLUG_DEVICE     = 10,  /* args: int32 slotIndex                                     | returns: bool success */
};
