#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include <vector>
#include <string>
#include <memory>
#include "AbstractVirtualDevice.h"
#include "TinyUsbKeyboardDevice.h"
#include "TinyUsbMouseDevice.h"
#include "TinyUsbGamepadDevice.h"

// Maximum number of device sockets
#define MAX_DEVICE_SOCKETS 8

// Device types
enum class DeviceType {
    KEYBOARD,
    MOUSE,
    GAMEPAD
};

// USB Device information for a socket
struct UsbDevice {
    std::string name;
    AbstractVirtualDevice* device;
    DeviceType deviceType;
    uint8_t axesCount;
    uint8_t interfaceNum;
    uint8_t endpointNum;
    uint8_t stringIndex;  // String descriptor index for interface name
    bool occupied;        // Whether this socket is occupied
};

class DeviceManager {
public:
    DeviceManager();
    ~DeviceManager();

    // Socket-based device management
    // Plug a device into a specific socket (0-7)
    // Returns true if successful, false if socket is occupied or index is invalid
    bool plugDevice(uint8_t socketIndex, AbstractVirtualDevice* device, const std::string& name, DeviceType deviceType, uint8_t axesCount = 0);
    
    // Unplug a device from a specific socket (0-7)
    // Returns true if successful, false if socket was empty or index is invalid
    bool unplugDevice(uint8_t socketIndex);
    
    // Check if a socket is occupied
    bool isSocketOccupied(uint8_t socketIndex) const;
    
    // Direct device access by socket index
    AbstractVirtualDevice* getDevice(uint8_t socketIndex);
    UsbDevice* getDeviceInfo(uint8_t socketIndex);
    
    // Get device by interface number (for USB callbacks)
    UsbDevice* getDeviceByInterface(uint8_t interfaceNum);
    
    // Simplified setAxis - access device directly by socket index
    void setAxis(uint8_t socketIndex, int axis, int value);
    
    // Initialize all devices and USB
    bool init();
    
    // Update all devices (called in main loop)
    void update();
    
    // USB descriptor generation
    const uint8_t* getConfigurationDescriptor(uint16_t* length);
    const uint8_t* getDeviceReportDescriptor(uint8_t interfaceNum, uint16_t* length);
    
    // Get occupied device count
    size_t getOccupiedCount() const;

    // Get interface string by string index (for USB string descriptor callback)
    const char* getInterfaceString(uint8_t stringIndex) const;

private:
    UsbDevice deviceSockets[MAX_DEVICE_SOCKETS];  // Fixed-size array of device sockets
    uint8_t nextInterfaceNum;
    uint8_t nextEndpointNum;
    uint8_t nextStringIndex;  // Next available string descriptor index

    // Buffer for dynamic configuration descriptor
    std::vector<uint8_t> configDescriptorBuffer;
    
    // Generate USB configuration descriptor
    void generateConfigurationDescriptor();
    
    // Helper to allocate next interface and endpoint numbers
    void allocateInterface(UsbDevice& info);
    
    // Cleanup a device
    void cleanupDevice(UsbDevice& info);
};

// Global accessor functions (extern "C" for TinyUSB callbacks)
#ifdef __cplusplus
extern "C" {
#endif

DeviceManager* getDeviceManager();
void setDeviceManager(DeviceManager* manager);
bool isUsbMounted(void);

#ifdef __cplusplus
}
#endif

#endif // DEVICE_MANAGER_H
