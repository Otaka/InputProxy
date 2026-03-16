// mainboard/src/VirtualOutputDevice.cpp
#include "VirtualOutputDevice.h"
#include "EmulationBoard.h"

void VirtualOutputDevice::setAxis(int axis, int value) {
    if (board == nullptr || !board->active) return;
    board->setAxis(slotIndex, axis, value);
}
