#pragma once

#include <string>
#include <map>
#include <vector>
#include <functional>
#include <cstdint>
#include <sys/types.h>
#include <linux/input.h>
#include "../../shared/shared.h"
#include "../../shared/corocgo/corocgo.h"
#include "MainConfig.h"

// Structure to hold axis information (min, max, default values)
struct AxisInfo {
    int minimum;        // Minimum value
    int maximum;        // Maximum value
    int defaultValue;   // Default/center value
    int eventType;      // EV_ABS, EV_REL, etc.
    bool isCentered;    // True if axis has a center position (not min or max)

    AxisInfo() : minimum(0), maximum(0), defaultValue(0), eventType(0), isCentered(false) {}
};

// Event emitted by a real device after axis scaling/splitting
struct AxisEvent {
    std::string deviceIdStr;  // was: unsigned int deviceId
    int axisIndex;
    int value;
};

// Structure representing a real physical device
struct RealDevice {
    unsigned int deviceId;                          // Auto-incremented numeric ID (hot-path key)
    std::string deviceIdStr;                        // Stable string key from generateDeviceKey (for config)
    std::string evdevPath;                          // "/dev/input/eventX"
    int fd;                                         // evdev file descriptor
    bool active;                                    // false when disconnected

    // Bidirectional axis/button mappings (built dynamically from evdev capabilities)
    // originalAxes: raw names straight from evdev, never modified after readDeviceCapabilities
    // axes: current names — clone of originalAxes with any rename_axes overrides applied
    AxisTable originalAxes;
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

    RealDevice() : deviceId(0), fd(-1), active(true), vendorId(0), productId(0), nextVirtualAxisIndex(10000) {}
};

/**
 * Thin wrapper around all Linux input subsystem calls (open/close/ioctl/read/write).
 * Provides a single place for platform-specific I/O so RealDeviceManager stays
 * free of raw system calls.
 */
class LinuxInputManager {
public:
    // Return paths of all /dev/input/event* files currently present
    std::vector<std::string> scanEventPaths();

    // Open an evdev node; tries R/W first, falls back to R/O. Returns fd or -1.
    int openFd(const std::string& path);

    // Close an evdev file descriptor
    void closeFd(int fd);

    // Read device identity (vendor/product/version/bustype) via EVIOCGID
    bool readDeviceId(int fd, struct input_id& outId);

    // Read human-readable device name via EVIOCGNAME
    bool readDeviceName(int fd, char* buf, size_t size);

    // Read the top-level event-type capability bitmask
    bool readEventBits(int fd, unsigned char* bits, size_t size);

    // Read per-type capability bitmasks
    bool readAbsBits(int fd, unsigned char* bits, size_t size);
    bool readRelBits(int fd, unsigned char* bits, size_t size);
    bool readKeyBits(int fd, unsigned char* bits, size_t size);

    // Read absolute axis limits/current value via EVIOCGABS
    bool readAbsInfo(int fd, int code, struct input_absinfo& outInfo);

    // Read a batch of input_event structs; returns bytes read (may be 0 / negative)
    ssize_t readEvents(int fd, struct input_event* buf, size_t bufSize);

    // Write a single input_event to the device fd
    bool writeEvent(int fd, const struct input_event& ev);

    // Convert (type, code) to a human-readable axis/button name
    static std::string eventCodeToString(int type, int code);
};

/**
 * Manager for real physical USB input devices using evdev.
 * Uses coroutines for non-blocking device scanning and input reading.
 */
class RealDeviceManager {
public:
    /**
     * Constructor
     * @param duplicateSerialIds List of "vendor:product:serial" IDs that should use USB path instead
     */
    RealDeviceManager(const std::vector<std::string>& duplicateSerialIds);

    ~RealDeviceManager();

    /**
     * Open, read capabilities, and register (or reactivate) a device at the given path.
     * @return pointer to the RealDevice on success, nullptr on failure
     */
    RealDevice* registerDevice(const std::string& path);

    /**
     * Read one batch of input events from device fd, push AxisEvents to channel.
     * On disconnect: sets device.active = false, closes fd, returns false.
     * @return false if device is disconnected (caller should stop coroutine)
     */
    bool processDeviceInput(RealDevice* device, corocgo::Channel<AxisEvent>* channel);

    /**
     * Send an event to a device (e.g., force feedback, LED, rumble)
     */
    bool sendEvent(unsigned int deviceId, int axisIndex, int value);

    /**
     * Get a device by its numeric ID
     */
    RealDevice* getDevice(unsigned int deviceId);

    /**
     * Get all devices (including inactive)
     */
    const std::map<unsigned int, RealDevice>& getDevices() const { return deviceId2Device; }

    /**
     * Parse the real_devices section from config and extract axis rename overrides.
     * Immediately re-applies renames to all already-registered devices.
     */
    void load(const std::vector<ConfRealDevice>& devices);

    // Linux input abstraction — exposed so callers (e.g. main.cpp) can use it directly
    LinuxInputManager linuxInput;

public:
    std::map<unsigned int, RealDevice> deviceId2Device;   // numericId -> device
    std::vector<std::string> duplicateSerialIds;
    unsigned int nextDeviceId = 1;
    std::map<std::string, std::map<std::string,std::string>> axisRenames; // deviceIdStr -> (old -> new)

    // Generate stable string key from device info — used for config matching
    std::string generateDeviceKey(uint16_t vendor, uint16_t product, const std::string& serial,
                                  const std::string& usbPath, const std::string& name, int axisCount);

private:
    // Check if a device key should use USB path fallback
    bool shouldUseFallbackId(const std::string baseId) const;

    // Open and configure an evdev device (reads capabilities, sets fd)
    bool openDevice(RealDevice& device);

    // Read device capabilities and populate axis mappings
    bool readDeviceCapabilities(RealDevice& device);

    // Rebuild device.axes from device.originalAxes applying axisRenames for this device
    void applyAxisRenames(RealDevice& device);

    // Close a device fd
    void closeDevice(RealDevice& device);
};
