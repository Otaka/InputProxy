#ifndef ABSTRACT_DEVICE_MANAGER_H
#define ABSTRACT_DEVICE_MANAGER_H

#include <string>
#include <cstdint>

// Forward declaration
class AbstractVirtualDevice;

// Device mode enumeration
enum class DeviceMode : uint8_t {
    HID_MODE = 0,      // Keyboard + Mouse + HID Gamepads (DEFAULT)
    XINPUT_MODE = 1    // XInput Gamepads only (Xbox 360)
};

/**
 * Abstract base class for device managers.
 *
 * This interface defines the common operations for managing virtual USB devices.
 * Concrete implementations:
 * - HidDeviceManager: Socket-based management for HID devices (keyboard, mouse, gamepads)
 * - XinputDeviceManager: Socket-based management for XInput gamepads (Xbox 360)
 */
class AbstractDeviceManager {
public:
    virtual ~AbstractDeviceManager() = default;

    // ===== Initialization and Update =====

    /**
     * Initialize the device manager and all plugged devices.
     * @return true if initialization successful, false otherwise
     */
    virtual bool init() = 0;

    /**
     * Update all devices (called in main loop).
     * This processes USB events and updates device states.
     */
    virtual void update() = 0;

    // ===== USB Device Configuration (Builder Pattern) =====

    /**
     * Set USB Vendor ID for the composite device.
     * @param vid Vendor ID (e.g., 0x1209 for pid.codes, 0x045E for Microsoft)
     * @return Pointer to this manager for chaining
     */
    virtual AbstractDeviceManager* vendorId(uint16_t vid) = 0;

    /**
     * Set USB Product ID for the composite device.
     * @param pid Product ID
     * @return Pointer to this manager for chaining
     */
    virtual AbstractDeviceManager* productId(uint16_t pid) = 0;

    /**
     * Set manufacturer string for USB device descriptor.
     * @param name Manufacturer name
     * @return Pointer to this manager for chaining
     */
    virtual AbstractDeviceManager* manufacturer(const std::string& name) = 0;

    /**
     * Set product name string for USB device descriptor.
     * @param name Product name
     * @return Pointer to this manager for chaining
     */
    virtual AbstractDeviceManager* productName(const std::string& name) = 0;

    /**
     * Set serial number string for USB device descriptor.
     * @param serial Serial number
     * @return Pointer to this manager for chaining
     */
    virtual AbstractDeviceManager* serialNumber(const std::string& serial) = 0;

    // ===== Device Access =====

    /**
     * Set axis/button value for a device in a specific socket.
     * @param socketIndex Socket index (0-based)
     * @param axis Axis or button code
     * @param value Value to set (typically 0-1000 range)
     */
    virtual void setAxis(int socketIndex, int axis, int value) = 0;

    /**
     * Get device instance from a specific socket.
     * @param socketIndex Socket index (0-based)
     * @return Pointer to device, or nullptr if socket is empty
     */
    virtual AbstractVirtualDevice* getDevice(int socketIndex) = 0;

    // ===== USB Descriptor Callbacks =====

    /**
     * Get USB device descriptor.
     * Called by TinyUSB during enumeration.
     * @return Pointer to device descriptor
     */
    virtual uint8_t const* getDeviceDescriptor() = 0;

    /**
     * Get USB configuration descriptor.
     * Called by TinyUSB during enumeration.
     * @return Pointer to configuration descriptor
     */
    virtual uint8_t const* getConfigurationDescriptor() = 0;

    /**
     * Get HID report descriptor for a specific interface.
     * Called by TinyUSB for HID interfaces.
     * @param itf Interface number
     * @return Pointer to HID report descriptor, or nullptr if not HID
     */
    virtual uint8_t const* getHidReportDescriptor(uint8_t itf) = 0;

    /**
     * Get HID report descriptor length for a specific interface.
     * Called by TinyUSB for HID interfaces.
     * @param itf Interface number
     * @return Length of HID report descriptor
     */
    virtual uint16_t getHidReportDescriptorLength(uint8_t itf) = 0;

    /**
     * Get string descriptor by index.
     * Called by TinyUSB during enumeration.
     * @param index String descriptor index
     * @param langid Language ID
     * @return Pointer to string descriptor (UTF-16 format)
     */
    virtual char const* getStringDescriptor(uint8_t index, uint16_t langid) = 0;

    // ===== Mode Identification =====

    /**
     * Get the current device mode.
     * @return DeviceMode enum value (HID_MODE or XINPUT_MODE)
     */
    virtual DeviceMode getMode() const = 0;
};

// Global accessor functions (extern "C" for TinyUSB callbacks)
#ifdef __cplusplus
extern "C" {
#endif

AbstractDeviceManager* getDeviceManager();
void setDeviceManager(AbstractDeviceManager* manager);

#ifdef __cplusplus
}
#endif

#endif // ABSTRACT_DEVICE_MANAGER_H
