#include "RealDeviceManager.h"
#include "EventLoop.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <linux/input.h>

// Helper to convert event type and code to string
// Uses standardized names that align with virtual device naming conventions
static std::string eventCodeToString(int type, int code) {
    // Map common event codes to standardized names
    // These names can be easily mapped to virtual device axes
    static const std::map<int, std::string> absNames = {
        // Main analog sticks - left stick
        {ABS_X, "Stick LX"},
        {ABS_Y, "Stick LY"},
        {ABS_Z, "Stick LZ"},

        // Main analog sticks - right stick
        {ABS_RX, "Stick RX"},
        {ABS_RY, "Stick RY"},
        {ABS_RZ, "Stick RZ"},

        // D-pad / Hat switch
        {ABS_HAT0X, "Hat X"},
        {ABS_HAT0Y, "Hat Y"},
        {ABS_HAT1X, "Hat2 X"},
        {ABS_HAT1Y, "Hat2 Y"},
        {ABS_HAT2X, "Hat3 X"},
        {ABS_HAT2Y, "Hat3 Y"},
        {ABS_HAT3X, "Hat4 X"},
        {ABS_HAT3Y, "Hat4 Y"},

        // Triggers and other controls
        {ABS_THROTTLE, "Throttle"},
        {ABS_RUDDER, "Rudder"},
        {ABS_WHEEL, "Wheel"},
        {ABS_GAS, "Gas"},
        {ABS_BRAKE, "Brake"}
    };

    static const std::map<int, std::string> relNames = {
        // Relative axes (mouse-like movement)
        {REL_X, "X Axis"},
        {REL_Y, "Y Axis"},
        {REL_Z, "Z Axis"},
        {REL_WHEEL, "Wheel"},
        {REL_HWHEEL, "H-Wheel"}
    };

    if (type == EV_ABS && absNames.count(code)) {
        return absNames.at(code);
    } else if (type == EV_REL && relNames.count(code)) {
        return relNames.at(code);
    } else if (type == EV_KEY) {
        // For buttons, use a simple "Button N" format
        return "Button " + std::to_string(code);
    }

    return "EV" + std::to_string(type) + "_" + std::to_string(code);
}

RealDeviceManager::RealDeviceManager(
    EventLoop& eventLoop,
    const std::vector<std::string>& duplicateSerialIds,
    OnDeviceConnectCallback onConnect,
    OnDeviceDisconnectCallback onDisconnect,
    OnInputCallback onInput)
    : eventLoop(eventLoop),
      duplicateSerialIds(duplicateSerialIds),
      onDeviceConnect(onConnect),
      onDeviceDisconnect(onDisconnect),
      onInput(onInput)
{
    std::cout << "[RealDeviceManager] Initializing..." << std::endl;

    // Schedule immediate scan on startup
    eventLoop.addOneShot(std::chrono::milliseconds(1000), [this]() {
        std::cout << "[RealDeviceManager] Initial device scan..." << std::endl;
        scanDevices();
    });

    // Schedule repeating scan every 5 seconds
    eventLoop.addRepeatable(std::chrono::milliseconds(5000), [this]() {
        //scanDevices();
    });

    std::cout << "[RealDeviceManager] Initialized. Scanning every 5 seconds." << std::endl;
}

RealDeviceManager::~RealDeviceManager() {
    std::cout << "[RealDeviceManager] Shutting down..." << std::endl;

    // Close all devices
    for (auto& pair : deviceId2Device) {
        closeDevice(pair.second);
    }
    deviceId2Device.clear();
}

void RealDeviceManager::scanDevices() {
    // Mark all devices as not seen in this scan
    std::map<std::string, bool> currentScanDevices;

    // Enumerate /dev/input/event* devices
    DIR* dir = opendir("/dev/input");
    if (!dir) {
        std::cerr << "[RealDeviceManager] Cannot open /dev/input directory" << std::endl;
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;

        // Only process event* files
        if (name.find("event") != 0) {
            continue;
        }

        std::string path = "/dev/input/" + name;
        processDevicePath(path);

        // Mark this path as seen
        currentScanDevices[path] = true;
    }
    closedir(dir);

    // Detect removed devices (in lastScanDevices but not in currentScanDevices)
    for (const auto& pair : lastScanDevices) {
        if (currentScanDevices.find(pair.first) == currentScanDevices.end()) {
            // Device was removed
            std::cout << "[RealDeviceManager] Device removed: " << pair.first << std::endl;

            // Find and close the device
            for (auto it = deviceId2Device.begin(); it != deviceId2Device.end();) {
                if (it->second.evdevPath == pair.first) {
                    RealDevice* device = &it->second;
                    if (onDeviceDisconnect) {
                        onDeviceDisconnect(device);
                    }
                    closeDevice(it->second);
                    it = deviceId2Device.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

    lastScanDevices = currentScanDevices;
}

void RealDeviceManager::processDevicePath(const std::string& path) {
    // Check if we already have this device path
    for (const auto& pair : deviceId2Device) {
        if (pair.second.evdevPath == path) {
            return; // Already tracking this device
        }
    }

    // Try to open the device temporarily to read its info
    int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        return; // Cannot open, skip
    }

    // Read device information
    struct input_id id;
    if (ioctl(fd, EVIOCGID, &id) < 0) {
        close(fd);
        return;
    }

    // Read device name
    char name[256] = "Unknown";
    ioctl(fd, EVIOCGNAME(sizeof(name)), name);

    // Read serial (if available via sysfs)
    std::string serial = "";
    // For simplicity, we'll use the device number as a fallback
    // In production, you'd parse /sys/class/input/eventX/device/../../serial
    serial = std::to_string(id.version); // Simplified

    // Read USB path (simplified - use device path for now)
    std::string usbPath = path;

    close(fd);

    // Create new device (without ID yet)
    RealDevice device;
    device.evdevPath = path;
    device.vendorId = id.vendor;
    device.productId = id.product;
    device.serial = serial;
    device.usbPath = usbPath;
    device.deviceName = name;

    // Try to open and configure the device (this reads capabilities)
    if (!openDevice(device)) {
        std::cerr << "[RealDeviceManager] Failed to open device: " << path << std::endl;
        return;
    }

    // Now generate device ID with axis count
    int axisCount = device.axes.getEntries().size();
    std::string deviceId = generateDeviceId(id.vendor, id.product, serial, usbPath, name, axisCount);

    if (deviceId.find("Motion") != std::string::npos) {
        closeDevice(device);
        return;
    }

    // Check if we already have this device ID
    if (deviceId2Device.find(deviceId) != deviceId2Device.end()) {
       // closeDevice(device);
       // return; // Already have this device
    }

    // Set the device ID
    device.deviceId = deviceId;

    // Add to our map
    deviceId2Device[deviceId] = device;

    std::cout << "[RealDeviceManager] Device connected: " << deviceId
              << " (" << device.deviceName << ") with " << axisCount << " inputs. Path: "<<device.evdevPath << std::endl;

    // Notify callback
    if (onDeviceConnect) {
        onDeviceConnect(&deviceId2Device[deviceId]);
    }
}

std::string RealDeviceManager::generateDeviceId(uint16_t vendor, uint16_t product,
                                                 const std::string& serial,
                                                 const std::string& usbPath,
                                                 const std::string& name,
                                                 int axisCount) {
    // Generate base ID with axis count
    std::ostringstream oss;
    oss << std::hex << vendor << ":" << product << ":" << serial << ":" << name << ":" << std::dec << axisCount;
    std::string baseId = oss.str();

    // Check if we should use fallback
    if (shouldUseFallbackId(baseId)) {
        // Use USB path with axis count instead
        return usbPath + ":" + std::to_string(axisCount);
    }

    return baseId;
}

bool RealDeviceManager::shouldUseFallbackId(const std::string& baseId) const {
    return std::find(duplicateSerialIds.begin(), duplicateSerialIds.end(), baseId)
           != duplicateSerialIds.end();
}

bool RealDeviceManager::openDevice(RealDevice& device) {
    // Open device with read-write for potential output events
    device.fd = open(device.evdevPath.c_str(), O_RDWR | O_NONBLOCK);
    if (device.fd < 0) {
        // Try read-only if read-write fails
        device.fd = open(device.evdevPath.c_str(), O_RDONLY | O_NONBLOCK);
        if (device.fd < 0) {
            std::cerr << "[RealDeviceManager] Cannot open device: " << device.evdevPath
                      << " - " << strerror(errno) << std::endl;
            return false;
        }
    }

    // Read device capabilities
    if (!readDeviceCapabilities(device)) {
        close(device.fd);
        device.fd = -1;
        return false;
    }

    // Register file descriptor with EventLoop
    eventLoop.addFileDescriptor(
        device.fd,
        [this](int fd) { handleDeviceInput(fd); },
        [this](int fd) { handleDeviceRemoval(fd); }
    );

    return true;
}

bool RealDeviceManager::readDeviceCapabilities(RealDevice& device) {
    // Read supported event types
    unsigned char evBits[(EV_MAX + 7) / 8] = {0};
    if (ioctl(device.fd, EVIOCGBIT(0, sizeof(evBits)), evBits) < 0) {
        return false;
    }

    // Helper to test if a bit is set
    auto testBit = [](int bit, const unsigned char* array) {
        return array[bit / 8] & (1 << (bit % 8));
    };

    // Process absolute axes (EV_ABS)
    if (testBit(EV_ABS, evBits)) {
        unsigned char absBits[(ABS_MAX + 7) / 8] = {0};
        ioctl(device.fd, EVIOCGBIT(EV_ABS, sizeof(absBits)), absBits);

        for (int code = 0; code <= ABS_MAX; code++) {
            if (testBit(code, absBits)) {
                std::string name = eventCodeToString(EV_ABS, code);
                device.axes.addEntry(name, code);

                // Read axis information (min, max, default)
                struct input_absinfo absInfo;
                if (ioctl(device.fd, EVIOCGABS(code), &absInfo) >= 0) {
                    AxisInfo info;
                    info.minimum = absInfo.minimum;
                    info.maximum = absInfo.maximum;
                    info.defaultValue = absInfo.value;
                    info.eventType = EV_ABS;

                    // Determine if axis is centered (default value is roughly in the middle)
                    int range = info.maximum - info.minimum;
                    int centerPoint = info.minimum + range / 2;
                    int tolerance = range / 10; // 10% tolerance

                    info.isCentered = (std::abs(info.defaultValue - centerPoint) <= tolerance);

                    device.axisInfo[code] = info;

                    // If axis is centered, create virtual + and - axes
                    if (info.isCentered) {
                        // Create positive virtual axis
                        int positiveIndex = device.nextVirtualAxisIndex++;
                        device.axes.addEntry(name + "+", positiveIndex);

                        // Create negative virtual axis
                        int negativeIndex = device.nextVirtualAxisIndex++;
                        device.axes.addEntry(name + "-", negativeIndex);

                        // Store mapping
                        device.centeredAxisMapping[code] = std::make_pair(positiveIndex, negativeIndex);

                        /*std::cout << "[RealDeviceManager] Axis " << name
                                  << " min=" << info.minimum
                                  << " max=" << info.maximum
                                  << " default=" << info.defaultValue
                                  << " centered=yes - split into " << positiveName << " and " << negativeName << std::endl;*/
                    } else {
                        /*std::cout << "[RealDeviceManager] Axis " << name
                                  << " min=" << info.minimum
                                  << " max=" << info.maximum
                                  << " default=" << info.defaultValue
                                  << " centered=no" << std::endl;*/
                    }
                }
            }
        }
    }

    // Process relative axes (EV_REL)
    if (testBit(EV_REL, evBits)) {
        unsigned char relBits[(REL_MAX + 7) / 8] = {0};
        ioctl(device.fd, EVIOCGBIT(EV_REL, sizeof(relBits)), relBits);

        for (int code = 0; code <= REL_MAX; code++) {
            if (testBit(code, relBits)) {
                std::string name = eventCodeToString(EV_REL, code);
                device.axes.addEntry(name, code);

                // Relative axes don't have min/max, use reasonable defaults
                AxisInfo info;
                info.minimum = -127;
                info.maximum = 127;
                info.defaultValue = 0;
                info.eventType = EV_REL;
                info.isCentered = true; // Relative axes are always centered

                device.axisInfo[code] = info;

                // Relative axes are always centered, create virtual + and - axes
                int positiveIndex = device.nextVirtualAxisIndex++;
                device.axes.addEntry(name + "+", positiveIndex);

                int negativeIndex = device.nextVirtualAxisIndex++;
                device.axes.addEntry(name + "-", negativeIndex);

                // Store mapping
                device.centeredAxisMapping[code] = std::make_pair(positiveIndex, negativeIndex);
            }
        }
    }

    // Process buttons/keys (EV_KEY)
    if (testBit(EV_KEY, evBits)) {
        unsigned char keyBits[(KEY_MAX + 7) / 8] = {0};
        ioctl(device.fd, EVIOCGBIT(EV_KEY, sizeof(keyBits)), keyBits);

        for (int code = 0; code <= KEY_MAX; code++) {
            if (testBit(code, keyBits)) {
                std::string name = eventCodeToString(EV_KEY, code);
                device.axes.addEntry(name, code);

                // Buttons have simple 0/1 values
                AxisInfo info;
                info.minimum = 0;
                info.maximum = 1;
                info.defaultValue = 0;
                info.eventType = EV_KEY;
                info.isCentered = false; // Buttons are not centered

                device.axisInfo[code] = info;
            }
        }
    }

    return true;
}

void RealDeviceManager::closeDevice(RealDevice& device) {
    if (device.fd >= 0) {
        eventLoop.removeFileDescriptor(device.fd);
        close(device.fd);
        device.fd = -1;
    }
}

void RealDeviceManager::handleDeviceInput(int fd) {
    RealDevice* device = findDeviceByFd(fd);
    if (!device) {
        return;
    }

    // Read input events
    struct input_event events[64];
    ssize_t bytesRead = read(fd, events, sizeof(events));

    if (bytesRead < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            std::cerr << "[RealDeviceManager] Read error from device " << device->deviceId
                      << ": " << strerror(errno) << std::endl;
        }
        return;
    }

    int numEvents = bytesRead / sizeof(struct input_event);

    // Process each event
    for (int i = 0; i < numEvents; i++) {
        const struct input_event& ev = events[i];

        // Filter out sync events and timestamps
        if (ev.type == EV_SYN) {
            continue;
        }

        int axisIndex = ev.code;
        int rawValue = ev.value;

        // Get axis information
        auto infoIt = device->axisInfo.find(axisIndex);
        if (infoIt == device->axisInfo.end()) {
            // No axis info, send raw value
            if (onInput) {
                onInput(device->deviceId, axisIndex, rawValue);
            }
            continue;
        }

        const AxisInfo& info = infoIt->second;

        // Scale and split axis based on whether it's centered
        if (info.isCentered) {
            // Centered axis - split into + and - variants
            auto mappingIt = device->centeredAxisMapping.find(axisIndex);
            if (mappingIt == device->centeredAxisMapping.end()) {
                // This shouldn't happen, but handle it gracefully
                std::cerr << "[RealDeviceManager] No virtual axis mapping found for centered axis " << axisIndex << std::endl;
                continue;
            }

            int positiveVirtualIndex = mappingIt->second.first;
            int negativeVirtualIndex = mappingIt->second.second;

            int positiveScaledValue = 0;
            int negativeScaledValue = 0;

            if (rawValue > info.defaultValue) {
                // Positive direction: scale from [defaultValue, maximum] to [0, 1000]
                int range = info.maximum - info.defaultValue;
                if (range > 0) {
                    int normalized = rawValue - info.defaultValue;
                    positiveScaledValue = (normalized * 1000) / range;
                    positiveScaledValue = std::min(1000, std::max(0, positiveScaledValue));
                }
            } else if (rawValue < info.defaultValue) {
                // Negative direction: scale from [minimum, defaultValue] to [1000, 0]
                int range = info.defaultValue - info.minimum;
                if (range > 0) {
                    int normalized = info.defaultValue - rawValue;
                    negativeScaledValue = (normalized * 1000) / range;
                    negativeScaledValue = std::min(1000, std::max(0, negativeScaledValue));
                }
            }
            // else: rawValue == defaultValue, both remain 0

            // Only send updates when values change
            if (onInput) {
                // Check and send positive axis update
                auto lastPosIt = device->lastAxisValues.find(positiveVirtualIndex);
                int lastPositiveValue = (lastPosIt != device->lastAxisValues.end()) ? lastPosIt->second : 0;
                if (positiveScaledValue != lastPositiveValue) {
                    onInput(device->deviceId, positiveVirtualIndex, positiveScaledValue);
                    device->lastAxisValues[positiveVirtualIndex] = positiveScaledValue;
                }

                // Check and send negative axis update
                auto lastNegIt = device->lastAxisValues.find(negativeVirtualIndex);
                int lastNegativeValue = (lastNegIt != device->lastAxisValues.end()) ? lastNegIt->second : 0;
                if (negativeScaledValue != lastNegativeValue) {
                    onInput(device->deviceId, negativeVirtualIndex, negativeScaledValue);
                    device->lastAxisValues[negativeVirtualIndex] = negativeScaledValue;
                }
            }
        } else {
            // Non-centered axis - scale from [minimum, maximum] to [0, 1000]
            int scaledValue = 0;
            int range = info.maximum - info.minimum;
            if (range > 0) {
                int normalized = rawValue - info.minimum;
                scaledValue = (normalized * 1000) / range;
                scaledValue = std::min(1000, std::max(0, scaledValue));
            }

            // Call input callback with scaled value
            if (onInput) {
                onInput(device->deviceId, axisIndex, scaledValue);
            }
        }
    }
}

void RealDeviceManager::handleDeviceRemoval(int fd) {
    RealDevice* device = findDeviceByFd(fd);
    if (!device) {
        return;
    }

    std::cout << "[RealDeviceManager] Device disconnected: " << device->deviceId << std::endl;

    // Notify callback
    if (onDeviceDisconnect) {
        onDeviceDisconnect(device);
    }

    // Remove from map
    std::string deviceId = device->deviceId;
    closeDevice(*device);
    deviceId2Device.erase(deviceId);
}

RealDevice* RealDeviceManager::findDeviceByFd(int fd) {
    for (auto& pair : deviceId2Device) {
        if (pair.second.fd == fd) {
            return &pair.second;
        }
    }
    return nullptr;
}

bool RealDeviceManager::sendEvent(const std::string& deviceId, int axisIndex, int value) {
    auto it = deviceId2Device.find(deviceId);
    if (it == deviceId2Device.end()) {
        std::cerr << "[RealDeviceManager] Device not found: " << deviceId << std::endl;
        return false;
    }

    RealDevice& device = it->second;
    if (device.fd < 0) {
        std::cerr << "[RealDeviceManager] Device not open: " << deviceId << std::endl;
        return false;
    }

    // Create output event (e.g., force feedback, LED)
    struct input_event ev;
    memset(&ev, 0, sizeof(ev));
    gettimeofday(&ev.time, nullptr);
    ev.type = EV_FF; // Force feedback type (can be parameterized)
    ev.code = axisIndex;
    ev.value = value;

    // Write event
    if (write(device.fd, &ev, sizeof(ev)) < 0) {
        std::cerr << "[RealDeviceManager] Failed to write event to " << deviceId
                  << ": " << strerror(errno) << std::endl;
        return false;
    }

    // Send sync event
    ev.type = EV_SYN;
    ev.code = SYN_REPORT;
    ev.value = 0;
    write(device.fd, &ev, sizeof(ev));

    return true;
}

RealDevice* RealDeviceManager::getDevice(const std::string& deviceId) {
    auto it = deviceId2Device.find(deviceId);
    if (it != deviceId2Device.end()) {
        return &it->second;
    }
    return nullptr;
}
