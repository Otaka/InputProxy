// mainboard/src/MappingManager.cpp
#include "MappingManager.h"
#include "EmulatedDeviceManager.h"
#include "RealDeviceManager.h"
#include <iostream>

void MappingManager::loadFromConfig(const nlohmann::json& root, EmulatedDeviceManager* edm_) {
    using json = nlohmann::json;
    edm = edm_;

    // Build VIDs
    for (const auto& v : root.value("virtual_input_devices", json::array())) {
        std::string id   = v.value("id", "");
        std::string name = v.value("name", "");
        if (id.empty()) { std::cerr << "[mapping] VID entry missing id\n"; continue; }
        VirtualInputDevice vid;
        vid.id   = id;
        vid.name = name;
        vids.emplace(id, std::move(vid));
        vidMappings[id].vid = &vids.at(id);
    }

    // Build deviceAssignments
    for (const auto& rd : root.value("real_devices", json::array())) {
        std::string id         = rd.value("id", "");
        std::string assignedTo = rd.value("assignedTo", "");
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
        std::string type   = m.value("type", "");
        std::string vidId  = m.value("virtualInputDevice", "");
        std::string vodId  = m.value("virtualOutputDevice", "");

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
            sam.vidAxisIndex  = axis.value("from", -1);
            sam.vodId         = vodId;
            sam.vodDeviceIndex = -1;   // resolved later in onBoardRegistered
            sam.vodAxisIndex  = axis.value("to", -1);
            if (sam.vidAxisIndex < 0 || sam.vodAxisIndex < 0) {
                std::cerr << "[mapping] axis entry missing from/to, skipping\n";
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
            } else {
                sam.vodDeviceIndex = idx;
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
    if (it == realDeviceMappings.end() || !it->second.active) return;

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
