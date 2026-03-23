// mainboard/src/EmulatedDeviceManager.cpp
#include "EmulatedDeviceManager.h"
#include "EmulationBoard.h"
#include <iostream>

void EmulatedDeviceManager::registerBoard(EmulationBoard* board,
                                           const std::vector<VirtualOutputDevice>& newDevices) {
    // Detect re-boot: on re-boot the same EmulationBoard object is found in
    // emulationBoards by serialString and reused — the same pointer is passed again.
    // If any existing device already points to this board, it's a re-boot: no-op.
    // (board->active is set to true by the caller before registerBoard is called.)
    for (const auto& d : devices) {
        if (d.board == board) return;
    }

    // First registration: append all devices and build idToIndex entries.
    for (auto d : newDevices) {
        d.board = board;
        int idx = (int)devices.size();
        idToIndex[d.id] = idx;
        devices.push_back(std::move(d));
    }
}

void EmulatedDeviceManager::deactivateBoard(EmulationBoard* board) {
    if (board) board->active = false;
}

void EmulatedDeviceManager::setAxis(int deviceIndex, int axis, int value) {
    if (deviceIndex < 0 || deviceIndex >= (int)devices.size()) return;
    auto& d = devices[deviceIndex];
    if (d.board == nullptr || !d.board->active) return;
    d.setAxis(axis, value);
}

void EmulatedDeviceManager::clear() {
    devices.clear();
    idToIndex.clear();
}

int EmulatedDeviceManager::resolveId(const std::string& id) const {
    auto it = idToIndex.find(id);
    return (it != idToIndex.end()) ? it->second : -1;
}
