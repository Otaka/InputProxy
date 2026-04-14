// mainboard/src/MainConfig.h
#pragma once
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <cstdint>
#include "emulation/VirtualOutputDevice.h"
#include "PicoConfig.h"

// ── Enums ────────────────────────────────────────────────────────────────────

enum class ConfActionType      { EmitAxis, OutputSequence, Sleep };
enum class ConfRuleType        { Simple, Hotkey, Block, VodState, Turbo };
enum class ConfVodState        { Active, Silenced, Disconnected };
enum class ConfTurboCondition  { WhileAxisActive, Always };
enum class ConfActivationMode  { Toggle, WhileActive, WhileNotActive };

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

// Axis/value pair for block rules
struct ConfBlockAxis {
    std::string axis;
    int         value = 0;
};

// Activation trigger for a layer
struct ConfActivation {
    ConfActivationMode mode   = ConfActivationMode::Toggle;
    std::string        vid;
    std::string        hotkey;
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
    // Block
    std::vector<ConfBlockAxis> blockAxes;
    // VodState
    ConfVodState               vodState  = ConfVodState::Active;
    // Turbo
    std::string                turboAxis;
    int                        turboOnMs         = 100;
    int                        turboOffMs        = 100;
    int                        turboInitialDelay = 0;
    int                        turboMaxValue     = 1000;
    int                        turboMinValue     = 0;
    ConfTurboCondition         turboCondition    = ConfTurboCondition::WhileAxisActive;
};

// Matches layers[]
struct ConfLayer {
    std::string                   id;
    std::string                   name;
    bool                          active = true;
    std::vector<ConfRule>         rules;
    std::optional<ConfActivation> activation;
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
// unchanged, returns false. All validation errors are appended to `errors`.
bool loadConfig(const std::string& path, std::vector<std::string>& errors);

// Serialize gConfig to JSON and write to path. Returns false on failure.
// Not wired up yet — placeholder for future REST save endpoint.
bool saveConfig(const std::string& path);

// Build runtime BoardEntry list from a ConfEmulationBoard list.
std::vector<BoardEntry> buildBoardEntries(const std::vector<ConfEmulationBoard>& boards);

// Build VirtualOutputDevice list for a board entry.
std::vector<VirtualOutputDevice> buildVirtualDevices(const BoardEntry& entry);
