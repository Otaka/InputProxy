#include "RealDeviceManager.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <linux/input.h>
#include <cerrno>
#include "corocgo/corocgo.h"

// Helper to convert event type and code to string
static std::string eventCodeToString(int type, int code) {
    static const std::map<int, std::string> absNames = {
        {ABS_X,      "Stick LX"}, {ABS_Y,      "Stick LY"}, {ABS_Z,      "Stick LZ"},
        {ABS_RX,     "Stick RX"}, {ABS_RY,     "Stick RY"}, {ABS_RZ,     "Stick RZ"},
        {ABS_HAT0X,  "Hat X"},    {ABS_HAT0Y,  "Hat Y"},
        {ABS_HAT1X,  "Hat2 X"},   {ABS_HAT1Y,  "Hat2 Y"},
        {ABS_HAT2X,  "Hat3 X"},   {ABS_HAT2Y,  "Hat3 Y"},
        {ABS_HAT3X,  "Hat4 X"},   {ABS_HAT3Y,  "Hat4 Y"},
        {ABS_THROTTLE, "Throttle"}, {ABS_RUDDER, "Rudder"},
        {ABS_WHEEL,  "Wheel"},    {ABS_GAS,    "Gas"},      {ABS_BRAKE,  "Brake"}
    };
    static const std::map<int, std::string> relNames = {
        {REL_X,      "X Axis"}, {REL_Y,     "Y Axis"}, {REL_Z,      "Z Axis"},
        {REL_WHEEL,  "Wheel"},  {REL_HWHEEL,"H-Wheel"}
    };

    if (type == EV_ABS && absNames.count(code)) return absNames.at(code);
    if (type == EV_REL && relNames.count(code)) return relNames.at(code);
    if (type == EV_KEY) return "Button " + std::to_string(code);
    return "EV" + std::to_string(type) + "_" + std::to_string(code);
}

// ---------------------------------------------------------------------------

RealDeviceManager::RealDeviceManager(const std::vector<std::string>& duplicateSerialIds)
    : duplicateSerialIds(duplicateSerialIds)
{
    std::cout << "[RealDeviceManager] Initialized." << std::endl;
}

RealDeviceManager::~RealDeviceManager() {
    std::cout << "[RealDeviceManager] Shutting down..." << std::endl;
    for (auto& [id, device] : deviceId2Device) {
        closeDevice(const_cast<RealDevice&>(device));
    }
    deviceId2Device.clear();
}

// ---------------------------------------------------------------------------
// scanDevices — returns devices that need a new reading coroutine
// ---------------------------------------------------------------------------

std::vector<std::string> RealDeviceManager::scanDevices() {
    std::vector<std::string> result;

    DIR* dir = opendir("/dev/input");
    if (!dir) {
        std::cerr << "[RealDeviceManager] Cannot open /dev/input directory" << std::endl;
        return result;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name.find("event") != 0) continue;

        std::string path = "/dev/input/" + name;

        // Linear scan — find existing device at this path
        RealDevice* existing = nullptr;
        for (auto& [id, dev] : deviceId2Device) {
            if (dev.evdevPath == path) { existing = &dev; break; }
        }

        // Include path if new or previously disconnected
        if (!existing || !existing->active) result.push_back(path);
    }
    closedir(dir);

    return result;
}

// ---------------------------------------------------------------------------

RealDevice* RealDeviceManager::registerDevice(const std::string& path) {
    // Check for an existing (inactive) entry at this path
    RealDevice* existing = nullptr;
    for (auto& [id, dev] : deviceId2Device) {
        if (dev.evdevPath == path) { existing = &dev; break; }
    }

    if (existing) {
        // Reactivate — reopen fd and re-read capabilities
        if (!openDevice(*existing)) return nullptr;
        existing->active = true;
        std::cout << "[RealDeviceManager] Device reactivated: "
                  << existing->deviceId << " (" << existing->deviceName << ")" << std::endl;
        return existing;
    }

    // New device — read metadata via a temporary fd
    int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) return nullptr;

    struct input_id id;
    if (ioctl(fd, EVIOCGID, &id) < 0) { close(fd); return nullptr; }

    char name[256] = "Unknown";
    ioctl(fd, EVIOCGNAME(sizeof(name)), name);
    close(fd);

    if (std::string(name).find("Motion") != std::string::npos) return nullptr;

    RealDevice device;
    device.evdevPath  = path;
    device.vendorId   = id.vendor;
    device.productId  = id.product;
    device.serial     = std::to_string(id.version);
    device.usbPath    = path;
    device.deviceName = name;

    if (!openDevice(device)) {
        std::cerr << "[RealDeviceManager] Failed to open device: " << path << std::endl;
        return nullptr;
    }

    int axisCount = static_cast<int>(device.axes.getEntries().size());
    device.deviceIdStr = generateDeviceKey(id.vendor, id.product, device.serial, path, name, axisCount);
    device.deviceId    = nextDeviceId++;

    unsigned int assignedId = device.deviceId;
    deviceId2Device[assignedId] = std::move(device);

    std::cout << "[RealDeviceManager] Device connected: id=" << assignedId
              << " (" << deviceId2Device[assignedId].deviceName << ")"
              << " with " << axisCount << " inputs. Path: " << path << std::endl;

    return &deviceId2Device[assignedId];
}

// ---------------------------------------------------------------------------

std::string RealDeviceManager::generateDeviceKey(uint16_t vendor, uint16_t product,
                                                  const std::string& serial,
                                                  const std::string& usbPath,
                                                  const std::string& name,
                                                  int axisCount) {
    std::ostringstream oss;
    oss << std::hex << vendor << ":" << product << ":" << serial
        << ":" << name << ":" << std::dec << axisCount;
    std::string baseId = oss.str();
    if (shouldUseFallbackId(baseId)) {
        return usbPath + ":" + std::to_string(axisCount);
    }
    return baseId;
}

bool RealDeviceManager::shouldUseFallbackId(const std::string& baseId) const {
    return std::find(duplicateSerialIds.begin(), duplicateSerialIds.end(), baseId)
           != duplicateSerialIds.end();
}

// ---------------------------------------------------------------------------

bool RealDeviceManager::openDevice(RealDevice& device) {
    device.fd = open(device.evdevPath.c_str(), O_RDWR | O_NONBLOCK);
    if (device.fd < 0)
        device.fd = open(device.evdevPath.c_str(), O_RDONLY | O_NONBLOCK);
    if (device.fd < 0) {
        std::cerr << "[RealDeviceManager] Cannot open device: " << device.evdevPath
                  << " - " << strerror(errno) << std::endl;
        return false;
    }

    if (!readDeviceCapabilities(device)) {
        close(device.fd);
        device.fd = -1;
        return false;
    }
    return true;
}

bool RealDeviceManager::readDeviceCapabilities(RealDevice& device) {
    unsigned char evBits[(EV_MAX + 7) / 8] = {0};
    if (ioctl(device.fd, EVIOCGBIT(0, sizeof(evBits)), evBits) < 0) return false;

    auto testBit = [](int bit, const unsigned char* array) -> bool {
        return (array[bit / 8] & (1 << (bit % 8))) != 0;
    };

    // Absolute axes
    if (testBit(EV_ABS, evBits)) {
        unsigned char absBits[(ABS_MAX + 7) / 8] = {0};
        ioctl(device.fd, EVIOCGBIT(EV_ABS, sizeof(absBits)), absBits);
        for (int code = 0; code <= ABS_MAX; code++) {
            if (!testBit(code, absBits)) continue;
            std::string axisName = eventCodeToString(EV_ABS, code);
            device.axes.addEntry(axisName, code);

            struct input_absinfo absInfo;
            if (ioctl(device.fd, EVIOCGABS(code), &absInfo) >= 0) {
                AxisInfo info;
                info.minimum      = absInfo.minimum;
                info.maximum      = absInfo.maximum;
                info.defaultValue = absInfo.value;
                info.eventType    = EV_ABS;
                int range       = info.maximum - info.minimum;
                int centerPoint = info.minimum + range / 2;
                int tolerance   = range / 10;
                info.isCentered = (std::abs(info.defaultValue - centerPoint) <= tolerance);
                device.axisInfo[code] = info;

                if (info.isCentered) {
                    int pos = device.nextVirtualAxisIndex++;
                    int neg = device.nextVirtualAxisIndex++;
                    device.axes.addEntry(axisName + "+", pos);
                    device.axes.addEntry(axisName + "-", neg);
                    device.centeredAxisMapping[code] = {pos, neg};
                }
            }
        }
    }

    // Relative axes
    if (testBit(EV_REL, evBits)) {
        unsigned char relBits[(REL_MAX + 7) / 8] = {0};
        ioctl(device.fd, EVIOCGBIT(EV_REL, sizeof(relBits)), relBits);
        for (int code = 0; code <= REL_MAX; code++) {
            if (!testBit(code, relBits)) continue;
            std::string axisName = eventCodeToString(EV_REL, code);
            device.axes.addEntry(axisName, code);

            AxisInfo info;
            info.minimum = -127; info.maximum = 127; info.defaultValue = 0;
            info.eventType = EV_REL; info.isCentered = true;
            device.axisInfo[code] = info;

            int pos = device.nextVirtualAxisIndex++;
            int neg = device.nextVirtualAxisIndex++;
            device.axes.addEntry(axisName + "+", pos);
            device.axes.addEntry(axisName + "-", neg);
            device.centeredAxisMapping[code] = {pos, neg};
        }
    }

    // Buttons / keys
    if (testBit(EV_KEY, evBits)) {
        unsigned char keyBits[(KEY_MAX + 7) / 8] = {0};
        ioctl(device.fd, EVIOCGBIT(EV_KEY, sizeof(keyBits)), keyBits);
        for (int code = 0; code <= KEY_MAX; code++) {
            if (!testBit(code, keyBits)) continue;
            std::string axisName = eventCodeToString(EV_KEY, code);
            device.axes.addEntry(axisName, code);

            AxisInfo info;
            info.minimum = 0; info.maximum = 1; info.defaultValue = 0;
            info.eventType = EV_KEY; info.isCentered = false;
            device.axisInfo[code] = info;
        }
    }

    return true;
}

void RealDeviceManager::closeDevice(RealDevice& device) {
    if (device.fd >= 0) {
        close(device.fd);
        device.fd = -1;
    }
}

// ---------------------------------------------------------------------------
// processDeviceInput — called by device reading coroutine after wait_file
// ---------------------------------------------------------------------------

bool RealDeviceManager::processDeviceInput(RealDevice* device, corocgo::Channel<AxisEvent>* channel) {
    struct input_event events[64];
    ssize_t bytesRead = read(device->fd, events, sizeof(events));

    if (bytesRead < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return true; // nothing yet
        std::cerr << "[RealDeviceManager] Read error on device " << device->deviceId
                  << ": " << strerror(errno) << std::endl;
        device->active = false;
        closeDevice(*device);
        return false;
    }

    if (bytesRead == 0) {
        std::cout << "[RealDeviceManager] Device disconnected: " << device->deviceId << std::endl;
        device->active = false;
        closeDevice(*device);
        return false;
    }

    int numEvents = static_cast<int>(bytesRead) / static_cast<int>(sizeof(struct input_event));

    for (int i = 0; i < numEvents; i++) {
        const struct input_event& ev = events[i];
        if (ev.type == EV_SYN) continue;

        int axisCode = ev.code;
        int rawValue = ev.value;

        auto infoIt = device->axisInfo.find(axisCode);
        if (infoIt == device->axisInfo.end()) {
            channel->send(AxisEvent{device->deviceId, axisCode, rawValue});
            continue;
        }

        const AxisInfo& info = infoIt->second;

        if (info.isCentered) {
            auto mappingIt = device->centeredAxisMapping.find(axisCode);
            if (mappingIt == device->centeredAxisMapping.end()) continue;

            int posIdx = mappingIt->second.first;
            int negIdx = mappingIt->second.second;
            int posVal = 0, negVal = 0;

            if (rawValue > info.defaultValue) {
                int range = info.maximum - info.defaultValue;
                if (range > 0) {
                    posVal = std::min(1000, std::max(0,
                        (rawValue - info.defaultValue) * 1000 / range));
                }
            } else if (rawValue < info.defaultValue) {
                int range = info.defaultValue - info.minimum;
                if (range > 0) {
                    negVal = std::min(1000, std::max(0,
                        (info.defaultValue - rawValue) * 1000 / range));
                }
            }

            auto lastPosIt = device->lastAxisValues.find(posIdx);
            int lastPos = (lastPosIt != device->lastAxisValues.end()) ? lastPosIt->second : 0;
            if (posVal != lastPos) {
                channel->send(AxisEvent{device->deviceId, posIdx, posVal});
                device->lastAxisValues[posIdx] = posVal;
            }

            auto lastNegIt = device->lastAxisValues.find(negIdx);
            int lastNeg = (lastNegIt != device->lastAxisValues.end()) ? lastNegIt->second : 0;
            if (negVal != lastNeg) {
                channel->send(AxisEvent{device->deviceId, negIdx, negVal});
                device->lastAxisValues[negIdx] = negVal;
            }
        } else {
            int scaledValue = 0;
            int range = info.maximum - info.minimum;
            if (range > 0) {
                scaledValue = std::min(1000, std::max(0,
                    (rawValue - info.minimum) * 1000 / range));
            }
            channel->send(AxisEvent{device->deviceId, axisCode, scaledValue});
        }
    }

    return true;
}

// ---------------------------------------------------------------------------

bool RealDeviceManager::sendEvent(unsigned int deviceId, int axisIndex, int value) {
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

    struct input_event ev;
    memset(&ev, 0, sizeof(ev));
    gettimeofday(&ev.time, nullptr);
    ev.type  = EV_FF;
    ev.code  = static_cast<uint16_t>(axisIndex);
    ev.value = value;
    if (write(device.fd, &ev, sizeof(ev)) < 0) {
        std::cerr << "[RealDeviceManager] Failed to write event to " << deviceId
                  << ": " << strerror(errno) << std::endl;
        return false;
    }

    ev.type = EV_SYN; ev.code = SYN_REPORT; ev.value = 0;
    write(device.fd, &ev, sizeof(ev));
    return true;
}

RealDevice* RealDeviceManager::getDevice(unsigned int deviceId) {
    auto it = deviceId2Device.find(deviceId);
    return (it != deviceId2Device.end()) ? &it->second : nullptr;
}
