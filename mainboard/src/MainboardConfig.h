// mainboard/src/MainboardConfig.h
// Loads emulation_boards from the mainboard config.json using jsmn.
#pragma once
#include <string>
#include <vector>
#include "VirtualOutputDevice.h"
#include "PicoConfig.h"  // ../shared/ is in include_directories

struct BoardEntry {
    std::string picoId;
    PicoConfig  config;
    std::vector<std::string> deviceIds; // human-readable IDs in same order as config.devices
};

// Read config.json and return all emulation_boards entries.
// Returns empty vector on file-not-found or parse failure (logs to stderr).
std::vector<BoardEntry> loadMainboardConfig(const std::string& path);

// Build a VirtualOutputDevice list for a board entry.
// Called after loadMainboardConfig when registering into EmulatedDeviceManager.
std::vector<VirtualOutputDevice> buildVirtualDevices(const BoardEntry& entry);
