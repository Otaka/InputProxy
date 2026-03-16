#pragma once

#include <vector>
#include "RealDeviceManager.h"
#include "EmulationBoard.h"
#include "EmulatedDeviceManager.h"

// Starts the HTTP API server coroutine on the given port.
// Must be called from within the coroutine scheduler context.
void startRestApi(int port, RealDeviceManager* deviceManager,
                  std::vector<EmulationBoard>* boards,
                  EmulatedDeviceManager* emulatedDeviceManager);
