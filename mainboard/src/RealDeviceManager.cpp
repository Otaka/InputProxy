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
#include <cerrno>
#include "corocgo/corocgo.h"


// ---------------------------------------------------------------------------
// LinuxInputManager
// ---------------------------------------------------------------------------

std::vector<std::string> LinuxInputManager::scanEventPaths() {
    std::vector<std::string> result;
    DIR* dir = opendir("/dev/input");
    if (!dir) {
        std::cerr << "[LinuxInputManager] Cannot open /dev/input" << std::endl;
        return result;
    }
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name.find("event") == 0)
            result.push_back("/dev/input/" + name);
    }
    closedir(dir);
    return result;
}

int LinuxInputManager::openFd(const std::string& path) {
    int fd = open(path.c_str(), O_RDWR | O_NONBLOCK);
    if (fd < 0)
        fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
    return fd;
}

void LinuxInputManager::closeFd(int fd) {
    if (fd >= 0)
        close(fd);
}

bool LinuxInputManager::readDeviceId(int fd, struct input_id& outId) {
    return ioctl(fd, EVIOCGID, &outId) >= 0;
}

bool LinuxInputManager::readDeviceName(int fd, char* buf, size_t size) {
    return ioctl(fd, EVIOCGNAME(size), buf) >= 0;
}

bool LinuxInputManager::readEventBits(int fd, unsigned char* bits, size_t size) {
    return ioctl(fd, EVIOCGBIT(0, size), bits) >= 0;
}

bool LinuxInputManager::readAbsBits(int fd, unsigned char* bits, size_t size) {
    return ioctl(fd, EVIOCGBIT(EV_ABS, size), bits) >= 0;
}

bool LinuxInputManager::readRelBits(int fd, unsigned char* bits, size_t size) {
    return ioctl(fd, EVIOCGBIT(EV_REL, size), bits) >= 0;
}

bool LinuxInputManager::readKeyBits(int fd, unsigned char* bits, size_t size) {
    return ioctl(fd, EVIOCGBIT(EV_KEY, size), bits) >= 0;
}

bool LinuxInputManager::readAbsInfo(int fd, int code, struct input_absinfo& outInfo) {
    return ioctl(fd, EVIOCGABS(code), &outInfo) >= 0;
}

ssize_t LinuxInputManager::readEvents(int fd, struct input_event* buf, size_t bufSize) {
    return read(fd, buf, bufSize);
}

bool LinuxInputManager::writeEvent(int fd, const struct input_event& ev) {
    return write(fd, &ev, sizeof(ev)) == static_cast<ssize_t>(sizeof(ev));
}

std::string LinuxInputManager::eventCodeToString(int type, int code) {
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
// RealDeviceManager
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

RealDevice* RealDeviceManager::registerDevice(const std::string& path) {
    // Check for an existing (inactive) entry at this path
    RealDevice* existing = nullptr;
    for (auto& [id, dev] : deviceId2Device) {
        if (dev.evdevPath == path) { existing = &dev; break; }
    }

    if (existing) {
        // Reset all stale state before reopening — a different physical device may have
        // connected at the same evdev path (Linux reuses /dev/input/eventX slots).
        existing->axes        = AxisTable{};
        existing->originalAxes= AxisTable{};
        existing->axisInfo.clear();
        existing->centeredAxisMapping.clear();
        existing->lastAxisValues.clear();
        existing->nextVirtualAxisIndex = 10000;
        existing->mouseXYAxisIndex = -1;
        existing->pendingRelX = 0;
        existing->pendingRelY = 0;

        // Reactivate — reopen fd and re-read capabilities
        if (!openDevice(*existing)) return nullptr;

        // Re-read device identity: vendor/product/name may differ if a different device
        // connected at this path. Update all identity fields and regenerate deviceIdStr.
        struct input_id newId;
        if (linuxInput.readDeviceId(existing->fd, newId)) {
            existing->vendorId  = newId.vendor;
            existing->productId = newId.product;
            existing->serial    = std::to_string(newId.version);
        }
        char newName[256] = "Unknown";
        linuxInput.readDeviceName(existing->fd, newName, sizeof(newName));
        existing->deviceName = newName;

        int axisCount = static_cast<int>(existing->axes.getEntries().size());
        existing->deviceIdStr = generateDeviceKey(existing->vendorId, existing->productId,
                                                  existing->serial, existing->usbPath,
                                                  existing->deviceName, axisCount);

        existing->active = true;
        applyAxisRenames(*existing);
        std::cout << "[RealDeviceManager] Device reactivated: "
                  << existing->deviceId << " (" << existing->deviceName << ")" << std::endl;
        return existing;
    }

    // New device — read metadata via a temporary fd
    int fd = linuxInput.openFd(path);
    if (fd < 0) return nullptr;

    struct input_id id;
    if (!linuxInput.readDeviceId(fd, id)) { linuxInput.closeFd(fd); return nullptr; }

    char name[256] = "Unknown";
    linuxInput.readDeviceName(fd, name, sizeof(name));
    linuxInput.closeFd(fd);

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
    applyAxisRenames(device);

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

bool RealDeviceManager::shouldUseFallbackId(const std::string baseId) const {
    for (int i = 0; i < (int)duplicateSerialIds.size(); i++) {
        if (duplicateSerialIds[i] == baseId)
            return true;
    }
    return false;
}

// ---------------------------------------------------------------------------

bool RealDeviceManager::openDevice(RealDevice& device) {
    device.fd = linuxInput.openFd(device.evdevPath);
    if (device.fd < 0) {
        std::cerr << "[RealDeviceManager] Cannot open device: " << device.evdevPath
                  << " - " << strerror(errno) << std::endl;
        return false;
    }

    if (!readDeviceCapabilities(device)) {
        linuxInput.closeFd(device.fd);
        device.fd = -1;
        return false;
    }
    device.originalAxes = device.axes;  // snapshot raw evdev names before any renames
    return true;
}

bool RealDeviceManager::readDeviceCapabilities(RealDevice& device) {
    unsigned char evBits[(EV_MAX + 7) / 8] = {0};
    if (!linuxInput.readEventBits(device.fd, evBits, sizeof(evBits))) return false;

    auto testBit = [](int bit, const unsigned char* array) -> bool {
        return (array[bit / 8] & (1 << (bit % 8))) != 0;
    };

    // Absolute axes
    if (testBit(EV_ABS, evBits)) {
        unsigned char absBits[(ABS_MAX + 7) / 8] = {0};
        linuxInput.readAbsBits(device.fd, absBits, sizeof(absBits));
        for (int code = 0; code <= ABS_MAX; code++) {
            if (!testBit(code, absBits)) continue;
            std::string axisName = LinuxInputManager::eventCodeToString(EV_ABS, code);
            device.axes.addEntry(axisName, code);

            struct input_absinfo absInfo;
            if (linuxInput.readAbsInfo(device.fd, code, absInfo)) {
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
    bool hasRelX = false, hasRelY = false;
    if (testBit(EV_REL, evBits)) {
        unsigned char relBits[(REL_MAX + 7) / 8] = {0};
        linuxInput.readRelBits(device.fd, relBits, sizeof(relBits));
        for (int code = 0; code <= REL_MAX; code++) {
            if (!testBit(code, relBits)) continue;
            std::string axisName = LinuxInputManager::eventCodeToString(EV_REL, code);
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

            if (code == REL_X) hasRelX = true;
            if (code == REL_Y) hasRelY = true;
        }
    }
    // If device has both REL_X and REL_Y, register a combined "Mouse XY" virtual axis.
    // X and Y deltas will be packed together on EV_SYN: low 16 bits = signed X, high 16 bits = signed Y.
    if (hasRelX && hasRelY) {
        device.mouseXYAxisIndex = device.nextVirtualAxisIndex++;
        device.axes.addEntry("Mouse XY", device.mouseXYAxisIndex);
    }

    // Buttons / keys
    if (testBit(EV_KEY, evBits)) {
        unsigned char keyBits[(KEY_MAX + 7) / 8] = {0};
        linuxInput.readKeyBits(device.fd, keyBits, sizeof(keyBits));
        for (int code = 0; code <= KEY_MAX; code++) {
            if (!testBit(code, keyBits)) continue;
            std::string axisName = LinuxInputManager::eventCodeToString(EV_KEY, code);
            device.axes.addEntry(axisName, code);

            AxisInfo info;
            info.minimum = 0; info.maximum = 1; info.defaultValue = 0;
            info.eventType = EV_KEY; info.isCentered = false;
            device.axisInfo[code] = info;
        }
    }

    return true;
}

void RealDeviceManager::load(const std::vector<ConfRealDevice>& devices) {
    std::map<std::string, std::map<std::string,std::string>> allAxisRenames;
    for (const auto& rd : devices) {
        if (!rd.renameAxes.empty())
            allAxisRenames[rd.id] = rd.renameAxes;
    }
    axisRenames = std::move(allAxisRenames);
    for (auto& [id, device] : deviceId2Device)
        applyAxisRenames(device);
}

void RealDeviceManager::applyAxisRenames(RealDevice& device) {
    auto renameIt = axisRenames.find(device.deviceIdStr);
    if (renameIt == axisRenames.end()) {
        device.axes = device.originalAxes;
        return;
    }
    const auto& renames = renameIt->second;
    device.axes = AxisTable{};
    for (const auto& entry : device.originalAxes.getEntries()) {
        auto it = renames.find(entry.name);
        const std::string& name = (it != renames.end()) ? it->second : entry.name;
        device.axes.addEntry(name, entry.index);
    }
}

void RealDeviceManager::closeDevice(RealDevice& device) {
    linuxInput.closeFd(device.fd);
    device.fd = -1;
}

// ---------------------------------------------------------------------------
// processDeviceInput — called by device reading coroutine after wait_file
// ---------------------------------------------------------------------------

bool RealDeviceManager::processDeviceInput(RealDevice* device, corocgo::Channel<AxisEvent>* channel) {
    struct input_event events[64];
    ssize_t bytesRead = linuxInput.readEvents(device->fd, events, sizeof(events));

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

        // EV_SYN: flush any accumulated mouse XY delta as a single combined event
        if (ev.type == EV_SYN) {
            if (device->mouseXYAxisIndex != -1 &&
                (device->pendingRelX != 0 || device->pendingRelY != 0)) {
                // Cast to uint16_t first to strip sign extension before packing
                int32_t packed = (int32_t)(
                    (uint32_t)(uint16_t)(int16_t)device->pendingRelX |
                    ((uint32_t)(uint16_t)(int16_t)device->pendingRelY << 16));
                channel->send(AxisEvent{device->deviceIdStr, device->mouseXYAxisIndex, packed});
                device->pendingRelX = 0;
                device->pendingRelY = 0;
            }
            continue;
        }

        int axisCode = ev.code;
        int rawValue = ev.value;

        auto infoIt = device->axisInfo.find(axisCode);
        if (infoIt == device->axisInfo.end()) {
            channel->send(AxisEvent{device->deviceIdStr, axisCode, rawValue});
            continue;
        }

        const AxisInfo& info = infoIt->second;

        // Combined mouse XY: accumulate REL_X and REL_Y until EV_SYN
        if (info.eventType == EV_REL && device->mouseXYAxisIndex != -1 &&
            (axisCode == REL_X || axisCode == REL_Y)) {
            if (axisCode == REL_X) device->pendingRelX += rawValue;
            else                   device->pendingRelY += rawValue;
            continue;
        }

        if (info.isCentered) {
            auto mappingIt = device->centeredAxisMapping.find(axisCode);
            if (mappingIt == device->centeredAxisMapping.end()) continue;

            int posIdx = mappingIt->second.first;
            int negIdx = mappingIt->second.second;
            int posVal = 0, negVal = 0;

            if (info.eventType == EV_REL) {
                // Relative axes are deltas — pass the raw value through without scaling
                if (rawValue > 0)
                    posVal = std::min(1000, rawValue);
                else if (rawValue < 0)
                    negVal = std::min(1000, -rawValue);
            } else if (rawValue > info.defaultValue) {
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

            if (info.eventType == EV_REL) {
                // Always send both directions so the mapping manager sees a clean
                // press/release cycle on each event. The zero on the inactive
                // direction fires the pending release and resets WaitingForRelease
                // state; the Pico ignores zero-value motion axis updates.
                channel->send(AxisEvent{device->deviceIdStr, posIdx, posVal});
                channel->send(AxisEvent{device->deviceIdStr, negIdx, negVal});
            } else {
                auto lastPosIt = device->lastAxisValues.find(posIdx);
                int lastPos = (lastPosIt != device->lastAxisValues.end()) ? lastPosIt->second : 0;
                if (posVal != lastPos) {
                    channel->send(AxisEvent{device->deviceIdStr, posIdx, posVal});
                    device->lastAxisValues[posIdx] = posVal;
                }

                auto lastNegIt = device->lastAxisValues.find(negIdx);
                int lastNeg = (lastNegIt != device->lastAxisValues.end()) ? lastNegIt->second : 0;
                if (negVal != lastNeg) {
                    channel->send(AxisEvent{device->deviceIdStr, negIdx, negVal});
                    device->lastAxisValues[negIdx] = negVal;
                }
            }
        } else {
            int scaledValue = 0;
            int range = info.maximum - info.minimum;
            if (range > 0) {
                scaledValue = std::min(1000, std::max(0,
                    (rawValue - info.minimum) * 1000 / range));
            }
            channel->send(AxisEvent{device->deviceIdStr, axisCode, scaledValue});
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
    if (!linuxInput.writeEvent(device.fd, ev)) {
        std::cerr << "[RealDeviceManager] Failed to write event to " << deviceId
                  << ": " << strerror(errno) << std::endl;
        return false;
    }

    struct input_event syn;
    memset(&syn, 0, sizeof(syn));
    gettimeofday(&syn.time, nullptr);
    syn.type = EV_SYN; syn.code = SYN_REPORT; syn.value = 0;
    linuxInput.writeEvent(device.fd, syn);
    return true;
}

RealDevice* RealDeviceManager::getDevice(unsigned int deviceId) {
    auto it = deviceId2Device.find(deviceId);
    return (it != deviceId2Device.end()) ? &it->second : nullptr;
}
