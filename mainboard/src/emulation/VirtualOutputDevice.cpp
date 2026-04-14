// mainboard/src/VirtualOutputDevice.cpp
#include "VirtualOutputDevice.h"
#include "EmulationBoard.h"

// Mouse motion axes (6-13 split, 14 combined XY) carry relative deltas.
// A value=0 on these is a release artifact from the mapping manager's
// press/release state machine — it has no meaning for a relative axis
// and must not reach the Pico.
static bool isMouseMotionZero(PicoDeviceType type, int axis, int value) {
    return type == PicoDeviceType::MOUSE && axis >= 6 && value == 0;
}

void VirtualOutputDevice::setAxis(int axis, int value) {
    if (board == nullptr || !board->active) return;
    if (isMouseMotionZero(type, axis, value)) return;
    board->setAxis(slotIndex, axis, value);
}
