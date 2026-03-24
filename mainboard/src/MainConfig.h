// mainboard/src/MainConfig.h
#pragma once
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include "VirtualOutputDevice.h"
#include "PicoConfig.h"

// ── Enums ────────────────────────────────────────────────────────────────────

enum class ConfActionType { EmitAxis, OutputSequence, Sleep };
enum class ConfRuleType   { Simple, Hotkey };

// ── Conf structs (pure data, no runtime state) ───────────────────────────────

// Matches emulation_boards[].devices[]
struct ConfVod {
    std::string    id;
    PicoDeviceType type        = PicoDeviceType::HID_GAMEPAD;
    std::string    name;
    uint8_t        buttons     = 0;
    uint8_t        axesMask    = 0;   // JSON key: "axesmask"
    bool           hat         = false;
};

// Matches emulation_boards[]
struct ConfEmulationBoard {
    std::string          id;
    uint16_t             vid          = 0x120A;
    uint16_t             pid          = 0x0004;
    std::string          manufacturer = "InputProxy";
    std::string          product      = "InputProxy Device";
    std::string          serial       = "000001";
    std::vector<ConfVod> devices;
};

// Matches virtual_input_devices[]
struct ConfVid {
    std::string id;
    std::string name;
};

// Matches real_devices[]
struct ConfRealDevice {
    std::string                        id;
    std::string                        assignedTo;
    std::map<std::string, std::string> renameAxes;  // JSON key: "rename_axes"; old -> new
};

// Axis from/to pair inside a "simple" rule
struct ConfAxisEntry {
    std::string from;
    std::string to;
};

// A single action in press_action / release_action
struct ConfAction {
    ConfActionType type     = ConfActionType::EmitAxis;
    std::string    vod;
    std::string    axis;
    std::string    sequence;
    int            timeMs   = 0;
};

// Matches layers[].rules[]
struct ConfRule {
    ConfRuleType               type      = ConfRuleType::Simple;
    std::string                vid;
    std::string                vod;           // used by Simple
    std::vector<ConfAxisEntry> axes;          // used by Simple
    std::string                hotkey;        // used by Hotkey
    bool                       propagate = false;
    std::vector<ConfAction>    pressActions;
    std::vector<ConfAction>    releaseActions;
};

// Matches layers[]
struct ConfLayer {
    std::string           id;
    std::string           name;
    bool                  active = true;
    std::vector<ConfRule> rules;
};

// Top-level config document
struct ConfRoot {
    std::vector<ConfEmulationBoard> emulationBoards;
    std::vector<ConfVid>            vids;
    std::vector<ConfRealDevice>     realDevices;
    std::vector<ConfLayer>          layers;
};

// ── Runtime artifact (kept here — used by main.cpp and EmulatedDeviceManager) ─

struct BoardEntry {
    std::string              picoId;
    PicoConfig               config;
    std::vector<std::string> deviceIds;
};

// ── Global config instance ────────────────────────────────────────────────────

extern ConfRoot gConfig;

// ── API ───────────────────────────────────────────────────────────────────────

// Parse file at path into gConfig. On failure: logs to stderr, leaves gConfig
// unchanged, returns false.
bool loadConfig(const std::string& path);

// Serialize gConfig to JSON and write to path. Returns false on failure.
// Not wired up yet — placeholder for future REST save endpoint.
bool saveConfig(const std::string& path);

// Build runtime BoardEntry list from a ConfEmulationBoard list.
std::vector<BoardEntry> buildBoardEntries(const std::vector<ConfEmulationBoard>& boards);

// Build VirtualOutputDevice list for a board entry.
std::vector<VirtualOutputDevice> buildVirtualDevices(const BoardEntry& entry);
