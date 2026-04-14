// mainboard/src/EmulatedDeviceManager.h
#pragma once
#include <vector>
#include <map>
#include <set>
#include <string>
#include "VirtualOutputDevice.h"

class EmulationBoard;

class EmulatedDeviceManager {
public:
    // Called when onBoot CRC matches.
    // First call for a board: appends its devices and builds idToIndex entries.
    // Subsequent calls (re-boot): replaces board pointer in-place at same indices.
    void registerBoard(EmulationBoard* board,
                       const std::vector<VirtualOutputDevice>& devices);

    // Sets board->active = false. Leaves devices in vector.
    // setAxis calls on these devices are silently dropped until re-activation.
    // Trigger: RPC timeout, UART disconnect (future implementation).
    void deactivateBoard(EmulationBoard* board);

    // Runtime axis dispatch. Silently drops if device out of range or board inactive.
    void setAxis(int deviceIndex, int axis, int value);

    // Silence a VOD: while silenced, setAxis calls are dropped.
    void setSilenced(const std::string& vodId, bool silenced);
    bool isSilenced(const std::string& vodId) const;

    // Config-time string-id to flat-index resolution. Returns -1 if not found.
    int resolveId(const std::string& id) const;

    // Clear all registered devices (call before reloading config).
    void clear();

    // For inspection (e.g., REST API)
    const std::vector<VirtualOutputDevice>& getDevices() const { return devices; }

private:
    std::vector<VirtualOutputDevice> devices;
    std::map<std::string, int>       idToIndex;
    std::set<std::string>            silencedVods;
};
