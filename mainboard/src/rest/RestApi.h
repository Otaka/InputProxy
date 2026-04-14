#pragma once

#include <string>
#include <vector>
#include <functional>
#include "../emulation/EmulationBoard.h"
#include "../emulation/EmulatedDeviceManager.h"
#include "../mapping/LayerManager.h"

class RealDeviceManager;

void startRestApi(int port,
                  RealDeviceManager* deviceManager,
                  std::vector<EmulationBoard>* boards,
                  EmulatedDeviceManager* emulatedDeviceManager,
                  LayerManager* layerManager,
                  std::function<std::vector<std::string>()> reloadConfigFn,
                  int* turboTimesPerSecond,
                  std::string* turboDeviceIdStr,
                  int* turboAxisIndex);
