// mainboard/src/MappingManager.h
#pragma once
#include <string>
#include <map>
#include <vector>
#include <unordered_map>
#include "../../shared/shared.h"
#include "json.hpp"

// Forward declarations
class EmulatedDeviceManager;
struct RealDevice;

// ---------------------------------------------------------------------------
// Data types
// ---------------------------------------------------------------------------

struct VirtualInputDevice {
    std::string id;
    std::string name;
    AxisTable   axisTable;
};

struct RealDeviceToVidMapping {
    VirtualInputDevice*         vid;
    std::unordered_map<int,int> realToVidAxisIndex; // realAxisIdx → vidAxisIdx
    bool                        active = false;
};

struct SimpleAxisMapping {
    std::string vidAxisName;     // axis name from config "from"; looked up in VID axisTable
    int         vidAxisIndex;    // resolved in onRealDeviceConnected, -1 until then
    std::string vodId;           // retained for deferred resolution and diagnostics
    std::string vodAxisName;     // axis name from config "to"; looked up in VOD axisTable
    int         vodDeviceIndex;  // -1 until resolved via EmulatedDeviceManager::resolveId
    int         vodAxisIndex;    // resolved in onBoardRegistered, -1 until then
};

struct VidMappingEntry {
    VirtualInputDevice*            vid;
    std::vector<SimpleAxisMapping> simpleAxes;
    // future: layers, lua actions, accords, turbopressing, hotkeys...
};

// ---------------------------------------------------------------------------

class MappingManager {
public:
    MappingManager() : edm(nullptr) {}

    // Parse virtual_input_devices, real_devices, mapping. Stores edm for later resolution.
    // Does NOT resolve vodDeviceIndex (boards not yet registered at call time).
    void loadFromConfig(const nlohmann::json& root, EmulatedDeviceManager* edm);

    // Called after each EmulatedDeviceManager::registerBoard() (both code paths).
    // Attempts to resolve all vodDeviceIndex == -1 entries. Logs warning on failure.
    void onBoardRegistered();

    // Called when a real device connects or reconnects.
    // Merges device axes into assigned VID axisTable (union, conflict = first wins).
    // Builds or replaces RealDeviceToVidMapping for this device.
    void onRealDeviceConnected(const std::string& deviceIdStr, const RealDevice& device);

    // Called when a real device disconnects.
    // Sets realDeviceMappings[deviceIdStr].active = false.
    void onRealDeviceDisconnected(const std::string& deviceIdStr);

    // Hot path — called from the axis event coroutine.
    void axisEvent(const std::string& deviceIdStr, int axisIndex, int value);

private:
    EmulatedDeviceManager*                        edm;
    std::map<std::string, VirtualInputDevice>     vids;              // vid id → VID
    std::map<std::string, VidMappingEntry>        vidMappings;       // vid id → outbound rules
    std::map<std::string, RealDeviceToVidMapping> realDeviceMappings;// deviceIdStr → mapping
    std::map<std::string, std::string>            deviceAssignments; // deviceIdStr → vid id

    void processVidAxisEvent(VirtualInputDevice* vid, int vidAxisIndex, int value);
};
