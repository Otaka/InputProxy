#pragma once

#include "RealDeviceManager.h"

// Starts the HTTP API server coroutine on the given port.
// Must be called from within the coroutine scheduler context.
void startRestApi(int port, RealDeviceManager* deviceManager);
