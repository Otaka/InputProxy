// rpcinterface.h - Shared RPC interface between Mainboard (RPi4) and Pico
// This file defines the bidirectional RPC communication interface
#pragma once

#include <functional>
#include <tuple>
#include <string>
#include <cstdint>

// Device configuration structures
struct KbdConfig {
    // Empty for now
};

struct MouseConfig {
    // Empty for now
};

struct HidGamepadConfig {
    uint8_t hat;
    uint16_t axesMask;
    uint8_t buttons;
};

struct Xbox360GamepadConfig {
    // Empty for now
};

struct DeviceConfiguration {
    int deviceType;
    union {
        KbdConfig kbdConfig;
        MouseConfig mouseConfig;
        HidGamepadConfig hidGamepadConfig;
        Xbox360GamepadConfig xbox360GamepadConfig;
    } config;
};

// ---------------------------------------------
// Pico2Main Provider
// RPC methods that Pico implements, Main has client to it and just calls
// ---------------------------------------------
struct Pico2Main {
    // Synchronous methods
    std::function<int(int)> ping;
    std::function<void(std::string)> debugPrint;
    std::function<bool(std::string)> onBoot;  // Called by Pico on boot with deviceId

    // User-defined pointer for application context
    void* userPointer = nullptr;

    // Provider ID must be unique
    static constexpr uint32_t providerId = 100;

    // Method table for compile-time reflection
    static constexpr auto methods = std::make_tuple(
        &Pico2Main::ping,
        &Pico2Main::debugPrint,
        &Pico2Main::onBoot
    );
};

// ---------------------------------------------
// Main2Pico Provider
// RPC methods that Main boards implements, Pico has client to it and just calls
// ---------------------------------------------
struct Main2Pico {
    std::function<int(int)> ping;
    std::function<void(bool)> setLed;
    std::function<bool()> getLedStatus;
    std::function<bool()> rebootFlashMode;
    std::function<void()> reboot;  // Reboot the Pico
    std::function<void(int,int,int)> setAxis;  // device, axis, value
    std::function<void(uint8_t)> setMode;  // Set device mode (0=HID, 1=XInput), saves to flash and reboots
    std::function<uint8_t()> getMode;  // Get current device mode: 0=HID, 1=XInput
    std::function<bool(int, DeviceConfiguration)> plugDevice;  // Plug a device into a slot
    std::function<bool(int)> unplugDevice;  // Unplug a device from a slot

    // User-defined pointer for application context
    void* userPointer = nullptr;

    static constexpr uint32_t providerId = 200;

    static constexpr auto methods = std::make_tuple(
        &Main2Pico::ping,
        &Main2Pico::setLed,
        &Main2Pico::getLedStatus,
        &Main2Pico::rebootFlashMode,
        &Main2Pico::reboot,
        &Main2Pico::setAxis,
        &Main2Pico::setMode,
        &Main2Pico::getMode,
        &Main2Pico::plugDevice,
        &Main2Pico::unplugDevice
    );
};

// ---------------------------------------------
// Pc2Pico Provider
// RPC methods that Pico provides, Desktop calls
// ---------------------------------------------
struct Pc2Pico {
    std::function<int(int)> ping;
    
    static constexpr uint32_t providerId = 300;
    
    static constexpr auto methods = std::make_tuple(
        &Pc2Pico::ping
    );
};

// ---------------------------------------------
// Pico2Pc Provider
// RPC methods that Desktop provides, Pico calls
// ---------------------------------------------
struct Pico2Pc {
    std::function<int(int)> ping;
    std::function<void(std::string)> debugPrint;
    
    static constexpr uint32_t providerId = 301;
    
    static constexpr auto methods = std::make_tuple(
        &Pico2Pc::ping,
        &Pico2Pc::debugPrint
    );
};