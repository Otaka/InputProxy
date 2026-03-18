#pragma once

#include <vector>
#include "RealDeviceManager.h"
#include "EmulationBoard.h"
#include "EmulatedDeviceManager.h"
#include "LayerManager.h"

void startRestApi(int port,
                  RealDeviceManager* deviceManager,
                  std::vector<EmulationBoard>* boards,
                  EmulatedDeviceManager* emulatedDeviceManager,
                  LayerManager* layerManager);
