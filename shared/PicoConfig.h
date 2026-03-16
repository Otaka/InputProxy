// shared/PicoConfig.h
// Shared config structs for Pico device layout.
// No Pico SDK or TinyUSB includes — safe to use on mainboard and Pico.
#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "shared.h"   // for DeviceMode (HID_MODE / XINPUT_MODE)

// Separate from Pico-only DeviceType in AbstractVirtualDevice.h
// (which uses GAMEPAD/XINPUT_GAMEPAD naming for internal USB dispatch)
enum class PicoDeviceType {
    KEYBOARD,
    MOUSE,
    HID_GAMEPAD,
    XBOX360_GAMEPAD
};

struct PicoDeviceConfig {
    PicoDeviceType type;
    std::string    name;        // USB interface string; defaults assigned per type if empty
    // hidgamepad fields — required in JSON; C++ defaults for programmatic construction only:
    uint8_t        buttons  = 16;
    uint8_t        axesMask = 0;
    bool           hat      = false; // required field in JSON for hidgamepad
};

struct PicoConfig {
    DeviceMode                    mode         = HID_MODE;
    uint16_t                      vid          = 0x120A;
    uint16_t                      pid          = 0x0004;
    std::string                   manufacturer = "InputProxy";
    std::string                   product      = "InputProxy Device";
    std::string                   serial       = "000000";
    std::vector<PicoDeviceConfig> devices;
};

// Parse canonical JSON string into PicoConfig.
// Uses jsmn with a 128-token stack array (max ~102 tokens for 8 HID devices).
// Accepts decimal integers or "0x..." hex strings for vid/pid.
// Returns false and populates errorMsg on any error.
bool parsePicoConfig(const char* json, int len, PicoConfig& out, std::string& errorMsg);

// Serialize PicoConfig to canonical compact JSON.
// Fixed key order: mode, vid, pid, manufacturer, product, serial, devices.
// Per-device: type, name, [buttons, axesmask, hat for hidgamepad].
// vid/pid as decimal integers. hat as true/false.
// Must be idempotent: serialize(parse(serialize(x))) == serialize(x).
std::string serializePicoConfig(const PicoConfig& cfg);
