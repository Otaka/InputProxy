// mainboard/src/MappingManager.h
#pragma once
#include <string>
#include <map>
#include <unordered_map>
#include "../../shared/shared.h"
#include "json.hpp"
#include "VidStateMap.h"
#include "LayerManager.h"

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

class MappingManager {
public:
    MappingManager() : edm(nullptr) {}

    void loadFromConfig(const nlohmann::json& root, EmulatedDeviceManager* edm);
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

    void resolveVidAxes();
    void resolveVodAxes();
    void dispatchVidAxisEvent(const std::string& vidId, int vidAxisIndex, int value);
    void executeActions(std::vector<std::unique_ptr<Action>>& actions, int value);
};
