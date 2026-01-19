// rpcinterface.h - Shared RPC interface between Mainboard (RPi4) and Pico
// This file defines the bidirectional RPC communication interface
#pragma once

#include <functional>
#include <tuple>
#include <string>

// ---------------------------------------------
// Pico2Main Provider
// RPC methods that Pico provides, Main calls
// ---------------------------------------------
struct Pico2Main {
    // Synchronous methods
    std::function<int(int)> ping;
    std::function<void(std::string)> debugPrint;  // Changed to int for simplicity (or use RpcByteArray for string)
    
    // Provider ID must be unique
    static constexpr uint32_t providerId = 100;
    
    // Method table for compile-time reflection
    static constexpr auto methods = std::make_tuple(
        &Pico2Main::ping,
        &Pico2Main::debugPrint
    );
};

// ---------------------------------------------
// Main2Pico Provider
// RPC methods that Main provides, Pico calls
// ---------------------------------------------
struct Main2Pico {
    std::function<int(int)> ping;
    std::function<void(bool)> setLed;
    std::function<bool()> getLedStatus;
    std::function<bool()> rebootFlashMode;
    std::function<void(int,int,int)> setAxis;// device, axis, value
    static constexpr uint32_t providerId = 200;
    
    static constexpr auto methods = std::make_tuple(
        &Main2Pico::ping,
        &Main2Pico::setLed,
        &Main2Pico::getLedStatus,
        &Main2Pico::rebootFlashMode,
        &Main2Pico::setAxis
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