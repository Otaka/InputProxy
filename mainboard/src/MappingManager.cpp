// mainboard/src/MappingManager.cpp
#include "MappingManager.h"
#include "EmulatedDeviceManager.h"
#include "RealDeviceManager.h"
#include <iostream>

// Returns the string value of key in obj, or "" if missing.
// Logs an error if the key exists but is not a string.
static std::string jsonStr(const nlohmann::json& obj, const char* key) {
    auto it = obj.find(key);
    if (it == obj.end()) return "";
    if (!it->is_string()) {
        std::cerr << "[mapping] field '" << key << "' must be a string, got: "
                  << it->dump() << "\n";
        return "";
    }
    return it->get<std::string>();
}

void MappingManager::loadFromConfig(const nlohmann::json& root, EmulatedDeviceManager* edm_) {
    using json = nlohmann::json;
    edm = edm_;

    // Build VIDs
    for (const auto& v : root.value("virtual_input_devices", json::array())) {
        std::string id   = jsonStr(v, "id");
        std::string name = jsonStr(v, "name");
        if (id.empty()) { std::cerr << "[mapping] VID entry missing id\n"; continue; }
        VirtualInputDevice vid;
        vid.id   = id;
        vid.name = name;
        vids.emplace(id, std::move(vid));
        vidMappings[id].vid = &vids.at(id);
    }

    // Build deviceAssignments
    for (const auto& rd : root.value("real_devices", json::array())) {
        std::string id         = jsonStr(rd, "id");
        std::string assignedTo = jsonStr(rd, "assignedTo");
        if (id.empty() || assignedTo.empty()) {
            std::cerr << "[mapping] real_device entry missing id or assignedTo\n";
            continue;
        }
        if (vids.find(assignedTo) == vids.end()) {
            std::cerr << "[mapping] real_device '" << id << "' assigned to unknown VID '" << assignedTo << "'\n";
            continue;
        }
        deviceAssignments[id] = assignedTo;
    }

    // Build simple axis mappings (VOD resolution deferred)
    for (const auto& m : root.value("mapping", json::array())) {
        std::string type   = jsonStr(m, "type");
        std::string vidId  = jsonStr(m, "virtualInputDevice");
        std::string vodId  = jsonStr(m, "virtualOutputDevice");

        if (type != "simple") {
            std::cerr << "[mapping] unsupported mapping type '" << type << "', skipping\n";
            continue;
        }
        if (vids.find(vidId) == vids.end()) {
            std::cerr << "[mapping] mapping references unknown VID '" << vidId << "'\n";
            continue;
        }

        VidMappingEntry& entry = vidMappings[vidId];
        for (const auto& axis : m.value("axes", json::array())) {
            SimpleAxisMapping sam;
            sam.vidAxisName   = jsonStr(axis, "from");
            sam.vidAxisIndex  = -1;    // resolved in onRealDeviceConnected
            sam.vodId         = vodId;
            sam.vodAxisName   = jsonStr(axis, "to");
            sam.vodDeviceIndex = -1;   // resolved in onBoardRegistered
            sam.vodAxisIndex  = -1;    // resolved in onBoardRegistered
            if (sam.vidAxisName.empty() || sam.vodAxisName.empty()) {
                std::cerr << "[mapping] axis entry missing or invalid from/to, skipping\n";
                continue;
            }
            entry.simpleAxes.push_back(std::move(sam));
        }
    }

    std::cout << "[mapping] loaded " << vids.size() << " VID(s), "
              << deviceAssignments.size() << " device assignment(s)\n";
}

void MappingManager::onBoardRegistered() {
    for (auto& [vidId, entry] : vidMappings) {
        for (auto& sam : entry.simpleAxes) {
            if (sam.vodDeviceIndex != -1) continue;
            int idx = edm->resolveId(sam.vodId);
            if (idx == -1) {
                std::cerr << "[mapping] VID '" << vidId
                          << "': cannot resolve VOD '" << sam.vodId << "'\n";
                continue;
            }
            sam.vodDeviceIndex = idx;
            sam.vodAxisIndex = edm->getDevices()[idx].axisTable.getIndex(sam.vodAxisName);
            if (sam.vodAxisIndex == -1) {
                std::cerr << "[mapping] VID '" << vidId
                          << "': cannot resolve VOD axis '" << sam.vodAxisName
                          << "' on '" << sam.vodId << "'\n";
            }
        }
    }
}

void MappingManager::onRealDeviceConnected(const std::string& deviceIdStr, const RealDevice& device) {
    auto assignIt = deviceAssignments.find(deviceIdStr);
    if (assignIt == deviceAssignments.end()) {
        // Device not listed in config — not assigned to any VID, ignore
        return;
    }
    const std::string& vidId = assignIt->second;
    VirtualInputDevice& vid = vids.at(vidId);

    // Merge device axes into VID axisTable (union, conflict = first wins)
    for (const auto& entry : device.axes.getEntries()) {
        if (!vid.axisTable.hasName(entry.name)) {
            vid.axisTable.addEntry(entry.name, entry.index);
        }
        // If name exists with same index: no-op. If different index: skip (first wins).
    }

    // Build axis index translation: real device axis index → VID axis index
    RealDeviceToVidMapping mapping;
    mapping.vid    = &vid;
    mapping.active = true;
    for (const auto& entry : device.axes.getEntries()) {
        int vidAxisIndex = vid.axisTable.getIndex(entry.name);
        if (vidAxisIndex != -1) {
            mapping.realToVidAxisIndex[entry.index] = vidAxisIndex;
        }
    }

    realDeviceMappings[deviceIdStr] = std::move(mapping);

    // Resolve any unresolved vidAxisIndex entries now that axisTable is updated
    for (auto& sam : vidMappings[vidId].simpleAxes) {
        if (sam.vidAxisIndex == -1 && !sam.vidAxisName.empty()) {
            sam.vidAxisIndex = vid.axisTable.getIndex(sam.vidAxisName);
            if (sam.vidAxisIndex == -1) {
                std::cerr << "[mapping] VID '" << vidId
                          << "': cannot resolve VID axis '" << sam.vidAxisName << "'\n";
            }
        }
    }

    std::cout << "[mapping] device '" << deviceIdStr
              << "' connected → VID '" << vidId << "'\n";
}

void MappingManager::onRealDeviceDisconnected(const std::string& deviceIdStr) {
    auto it = realDeviceMappings.find(deviceIdStr);
    if (it != realDeviceMappings.end()) {
        it->second.active = false;
        std::cout << "[mapping] device '" << deviceIdStr << "' disconnected\n";
    }
}

void MappingManager::axisEvent(const std::string& deviceIdStr, int axisIndex, int value) {
    auto it = realDeviceMappings.find(deviceIdStr);
    if (it == realDeviceMappings.end() 
        || !it->second.active)
        return;

    RealDeviceToVidMapping& mapping = it->second;
    auto axisIt = mapping.realToVidAxisIndex.find(axisIndex);
    if (axisIt == mapping.realToVidAxisIndex.end()) return;

    processVidAxisEvent(mapping.vid, axisIt->second, value);
}

void MappingManager::processVidAxisEvent(VirtualInputDevice* vid, int vidAxisIndex, int value) {
    auto it = vidMappings.find(vid->id);
    if (it == vidMappings.end()) return;

    for (auto& sam : it->second.simpleAxes) {
        if (sam.vidAxisIndex == vidAxisIndex && sam.vodDeviceIndex != -1) {
            edm->setAxis(sam.vodDeviceIndex, sam.vodAxisIndex, value);
        }
    }
}
