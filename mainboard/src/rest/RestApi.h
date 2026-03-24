#pragma once

#include <vector>
#include <functional>
#include "EmulationBoard.h"
#include "EmulatedDeviceManager.h"
#include "LayerManager.h"

class RealDeviceManager;

void startRestApi(int port,
                  RealDeviceManager* deviceManager,
                  std::vector<EmulationBoard>* boards,
                  EmulatedDeviceManager* emulatedDeviceManager,
                  LayerManager* layerManager,
                  std::function<void()> reloadConfigFn);
