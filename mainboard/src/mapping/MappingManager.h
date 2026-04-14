// mainboard/src/MappingManager.h
#pragma once
#include <string>
#include <map>
#include <set>
#include <memory>
#include <unordered_map>
#include "../../shared/shared.h"
#include "../MainConfig.h"
#include "VidStateMap.h"
#include "LayerManager.h"
#include "TurboRule.h"

class EmulatedDeviceManager;
struct RealDevice;

struct VirtualInputDevice {
    std::string id;
    std::string name;
    AxisTable   axisTable;
};

struct RealDeviceToVidMapping {
    VirtualInputDevice*         vid;
    std::unordered_map<int,int> realToVidAxisIndex;
    bool                        active = false;
};

struct TurboKey {
    std::string vidId;
    int         axisIndex;
    bool operator<(const TurboKey& o) const {
        return vidId < o.vidId || (vidId == o.vidId && axisIndex < o.axisIndex);
    }
};

class MappingManager {
public:
    MappingManager() : edm(nullptr) {}

    void load(const ConfRoot& config, EmulatedDeviceManager* edm);
    void clear();
    void onBoardRegistered();
    void onRealDeviceConnected(const std::string& deviceIdStr, const RealDevice& device);
    void onRealDeviceDisconnected(const std::string& deviceIdStr);
    void axisEvent(const std::string& deviceIdStr, int axisIndex, int value);

    LayerManager& getLayerManager() { return layerManager; }

private:
    EmulatedDeviceManager*                        edm;
    std::map<std::string, VirtualInputDevice>     vids;
    std::map<std::string, RealDeviceToVidMapping> realDeviceMappings;
    std::map<std::string, std::string>            deviceAssignments;
    VidStateMap                                   vidState;
    LayerManager                                  layerManager;

    // Turbo state
    std::map<TurboKey, std::shared_ptr<bool>>     activeTurbos;

    // USB disconnect tracking: board serial IDs currently disconnected
    std::set<std::string>                         usbDisconnectedBoards;

    void resolveVidAxes();
    void resolveVodAxes();
    void dispatchVidAxisEvent(const std::string& vidId, int vidAxisIndex, int value);
    void executeActions(std::vector<std::unique_ptr<Action>>& actions, int value);
    void evaluateVodStates();
    void startTurbo(const TurboRule& rule);
    void stopTurbo(const std::string& vidId, int axisIndex);
    void stopAllTurbosForLayer(Layer* layer);
};
