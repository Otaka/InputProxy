// mainboard/src/MainboardConfig.h
#pragma once
#include <string>
#include <vector>
#include "VirtualOutputDevice.h"
#include "PicoConfig.h"
#include "json.hpp"

struct BoardEntry {
    std::string picoId;
    PicoConfig  config;
    std::vector<std::string> deviceIds;
};

// Open, parse and return the JSON root. Returns empty object on failure (logs to stderr).
nlohmann::json parseConfigFile(const std::string& path);

// Build BoardEntry list from a pre-parsed JSON root.
std::vector<BoardEntry> loadMainboardConfig(const nlohmann::json& root);

// Build a VirtualOutputDevice list for a board entry.
std::vector<VirtualOutputDevice> buildVirtualDevices(const BoardEntry& entry);
