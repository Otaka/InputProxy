#pragma once

#include <string>
#include <map>
#include <vector>
#include <functional>
#include <cstdint>
#include "../../shared/shared.h"

class EventLoop;

// Structure to hold axis information (min, max, default values)
struct AxisInfo {
    int minimum;        // Minimum value
    int maximum;        // Maximum value
    int defaultValue;   // Default/center value
    int eventType;      // EV_ABS, EV_REL, etc.
    bool isCentered;    // True if axis has a center position (not min or max)

    AxisInfo() : minimum(0), maximum(0), defaultValue(0), eventType(0), isCentered(false) {}
};

// Structure representing a real physical device
struct RealDevice {
    std::string deviceId;                           // "vendor:product:serial" or USB path
    std::string evdevPath;                          // "/dev/input/eventX"
    int fd;                                         // evdev file descriptor

    // Bidirectional axis/button mappings (built dynamically from evdev capabilities)
    AxisTable axes;

    // Axis information (min/max/default values) - key is axis code
    std::map<int, AxisInfo> axisInfo;               // axis code -> AxisInfo

    // Mapping from raw axis code to virtual split axis indices (for centered axes)
    std::map<int, std::pair<int, int>> centeredAxisMapping; // rawCode -> (positiveVirtualIndex, negativeVirtualIndex)

    // Track last sent values for virtual axes to avoid redundant updates
    std::map<int, int> lastAxisValues;              // virtualAxisIndex -> lastSentValue

    int nextVirtualAxisIndex;                       // Counter for assigning virtual axis indices

    // Device information
    uint16_t vendorId;
    uint16_t productId;
    std::string serial;
    std::string usbPath;                            // For devices with duplicate serials
    std::string deviceName;                         // Human-readable name

    RealDevice() : fd(-1), vendorId(0), productId(0), nextVirtualAxisIndex(10000) {}
};

// Callback types
using OnDeviceConnectCallback = std::function<void(RealDevice*)>;
using OnDeviceDisconnectCallback = std::function<void(RealDevice*)>;
using OnInputCallback = std::function<void(const std::string& deviceId, int axisIndex, int value)>;

/**
 * Manager for real physical USB input devices using evdev.
 * Integrates with EventLoop for non-blocking device scanning and input reading.
 */
class RealDeviceManager {
public:
    /**
     * Constructor
     * @param eventLoop Reference to the event loop for timer and FD registration
     * @param duplicateSerialIds List of "vendor:product:serial" IDs that should use USB path instead
     * @param onConnect Callback when a device is connected
     * @param onDisconnect Callback when a device is disconnected
     * @param onInput Callback when input is received from a device
     */
    RealDeviceManager(
        EventLoop& eventLoop,
        const std::vector<std::string>& duplicateSerialIds,
        OnDeviceConnectCallback onConnect,
        OnDeviceDisconnectCallback onDisconnect,
        OnInputCallback onInput
    );

    ~RealDeviceManager();

    /**
     * Send an event to a device (e.g., force feedback, LED, rumble)
     * @param deviceId The device identifier
     * @param axisIndex The axis/button/effect index
     * @param value The value to send
     * @return true if successful, false otherwise
     */
    bool sendEvent(const std::string& deviceId, int axisIndex, int value);

    /**
     * Get a device by its ID
     * @param deviceId The device identifier
     * @return Pointer to device or nullptr if not found
     */
    RealDevice* getDevice(const std::string& deviceId);

    /**
     * Get all connected devices
     * @return Map of deviceId -> RealDevice
     */
    const std::map<std::string, RealDevice>& getDevices() const { return deviceId2Device; }

private:
    // Scans /dev/input/event* for new or removed devices
    void scanDevices();

    // Process a single evdev device path
    void processDevicePath(const std::string& path);

    // Generate device ID from device info
    std::string generateDeviceId(uint16_t vendor, uint16_t product, const std::string& serial, const std::string& usbPath, const std::string& name, int axisCount);

    // Check if a device ID should use USB path fallback
    bool shouldUseFallbackId(const std::string& baseId) const;

    // Open and configure an evdev device
    bool openDevice(RealDevice& device);

    // Read device capabilities and populate axis mappings
    bool readDeviceCapabilities(RealDevice& device);

    // Close a device and clean up resources
    void closeDevice(RealDevice& device);

    // Handler for evdev file descriptor events
    void handleDeviceInput(int fd);

    // Handler for device removal
    void handleDeviceRemoval(int fd);

    // Find device by file descriptor
    RealDevice* findDeviceByFd(int fd);

public:
    EventLoop& eventLoop;
    std::map<std::string, RealDevice> deviceId2Device;
    std::vector<std::string> duplicateSerialIds;

    // Callbacks
    OnDeviceConnectCallback onDeviceConnect;
    OnDeviceDisconnectCallback onDeviceDisconnect;
    OnInputCallback onInput;

    // Track last scan to detect removed devices
    std::map<std::string, bool> lastScanDevices;
};
