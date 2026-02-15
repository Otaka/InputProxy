#ifndef XINPUT_DEVICE_MANAGER_H
#define XINPUT_DEVICE_MANAGER_H

#include <string>
#include <vector>
#include "AbstractDeviceManager.h"
#include "XInputDevice.h"

// Maximum number of XInput gamepad sockets
#define MAX_XINPUT_SOCKETS 4

/**
 * Socket-based device manager for XInput gamepads (Xbox 360 compatible).
 *
 * Structure: Up to 4 XInput gamepad sockets
 * - Socket 0-3: XInput Gamepads
 * - No keyboard/mouse support in XInput mode
 */
class XinputDeviceManager : public AbstractDeviceManager {
public:
    XinputDeviceManager();
    ~XinputDeviceManager() override;

    // ===== AbstractDeviceManager Implementation =====

    // USB device-level configuration (VID/PID/Serial)
    AbstractDeviceManager* vendorId(uint16_t vid) override;
    AbstractDeviceManager* productId(uint16_t pid) override;
    AbstractDeviceManager* manufacturer(const std::string& name) override;
    AbstractDeviceManager* productName(const std::string& name) override;
    AbstractDeviceManager* serialNumber(const std::string& serial) override;

    // Getters for USB descriptors
    uint16_t getVendorId() const { return m_vendorId; }
    uint16_t getProductId() const { return m_productId; }
    const std::string& getManufacturer() const { return m_manufacturer; }
    const std::string& getProductName() const { return m_productName; }
    const std::string& getSerialNumber() const { return m_serialNumber; }

    // Device access
    void setAxis(int socketIndex, int axis, int value) override;
    AbstractVirtualDevice* getDevice(int socketIndex) override;

    // Initialize and update
    bool init() override;
    void update() override;

    // USB descriptor callbacks
    uint8_t const* getDeviceDescriptor() override;
    uint8_t const* getConfigurationDescriptor() override;
    uint8_t const* getHidReportDescriptor(uint8_t itf) override;
    uint16_t getHidReportDescriptorLength(uint8_t itf) override;
    char const* getStringDescriptor(uint8_t index, uint16_t langid) override;

    // Mode identification
    DeviceMode getMode() const override { return DeviceMode::XINPUT_MODE; }

    // ===== XinputDeviceManager-specific Methods =====

    // Socket-based device management
    bool plugDevice(uint8_t socketIndex, XInputDevice* device, const std::string& name);
    bool unplugDevice(uint8_t socketIndex);
    bool isSocketOccupied(uint8_t socketIndex) const;

    // Get occupied device count
    size_t getOccupiedCount() const;

private:
    struct XInputSocket {
        XInputDevice* device;
        std::string name;
        bool occupied;
    };

    XInputSocket sockets[MAX_XINPUT_SOCKETS];

    // USB device-level properties (apply to entire composite device)
    uint16_t m_vendorId;
    uint16_t m_productId;
    std::string m_manufacturer;
    std::string m_productName;
    std::string m_serialNumber;

    // Buffer for configuration descriptor
    std::vector<uint8_t> configDescriptorBuffer;

    // Generate USB configuration descriptor for XInput
    void generateConfigurationDescriptor();

    // Cleanup a device
    void cleanupDevice(XInputSocket& socket);
};

#endif // XINPUT_DEVICE_MANAGER_H
