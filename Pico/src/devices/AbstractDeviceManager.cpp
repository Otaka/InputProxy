#include "AbstractDeviceManager.h"

// Global device manager instance
static AbstractDeviceManager* g_deviceManager = nullptr;

extern "C" {

AbstractDeviceManager* getDeviceManager() {
    return g_deviceManager;
}

void setDeviceManager(AbstractDeviceManager* manager) {
    g_deviceManager = manager;
}

}
