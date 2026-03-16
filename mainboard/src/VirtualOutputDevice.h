// mainboard/src/VirtualOutputDevice.h
// Mainboard-side handle for a single Pico-emulated device.
// Uses PicoDeviceType from shared/PicoConfig.h, NOT the Pico-internal DeviceType.
#pragma once
#include <string>
#include "PicoConfig.h"  // ../shared/ is in include_directories
#include "shared.h"      // for AxisTable

// Forward declaration
class EmulationBoard;

struct VirtualOutputDevice {
    std::string     id;          // human-readable id from mainboard config ("vkbd1", etc.)
    int             slotIndex;   // 0-based position in Pico's device list (used in M2P_SET_AXIS)
    PicoDeviceType  type;
    AxisTable       axisTable;   // built from forKeyboard()/forMouse()/forHidGamepad()/forXbox360()
    EmulationBoard* board;       // back-pointer to owning Pico board; nullptr until activated

    // Dispatch axis value to the Pico via RPC. No-op if board is nullptr or inactive.
    void setAxis(int axis, int value);
};
