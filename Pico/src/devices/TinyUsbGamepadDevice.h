#ifndef TINYUSB_GAMEPAD_DEVICE_H
#define TINYUSB_GAMEPAD_DEVICE_H

#include "AbstractVirtualDevice.h"
#include <functional>
#include <stdint.h>
#include <cstring>
#include <string>
#include "tusb.h"

// HID Report ID for gamepad
#define REPORT_ID_GAMEPAD 1

// Gamepad configuration limits
#define GAMEPAD_MAX_BUTTONS 32
#define GAMEPAD_MAX_AXES    8

// Gamepad button codes (0-31) - used directly as button indices
// Buttons 0-31 map to gamepad buttons 1-32

// Gamepad axis bitmask flags
#define FLAG_MASK_GAMEPAD_AXIS_LX     0x01  // Bit 0: Left stick X
#define FLAG_MASK_GAMEPAD_AXIS_LY     0x02  // Bit 1: Left stick Y
#define FLAG_MASK_GAMEPAD_AXIS_LZ     0x04  // Bit 2: Left stick Z
#define FLAG_MASK_GAMEPAD_AXIS_RX     0x08  // Bit 3: Right stick X
#define FLAG_MASK_GAMEPAD_AXIS_RY     0x10  // Bit 4: Right stick Y
#define FLAG_MASK_GAMEPAD_AXIS_RZ     0x20  // Bit 5: Right stick Z
#define FLAG_MASK_GAMEPAD_AXIS_DIAL   0x40  // Bit 6: Dial
#define FLAG_MASK_GAMEPAD_AXIS_SLIDER 0x80  // Bit 7: Slider

// Gamepad axis codes (for setAxis)
// Values are 0-1000, mapped to 0-255 for HID
#define GAMEPAD_AXIS_LX     100  // Left stick X
#define GAMEPAD_AXIS_LY     101  // Left stick Y
#define GAMEPAD_AXIS_LZ     102  // Left stick Z
#define GAMEPAD_AXIS_RX     103  // Right stick X
#define GAMEPAD_AXIS_RY     104  // Right stick Y
#define GAMEPAD_AXIS_RZ     105  // Right stick Z
#define GAMEPAD_AXIS_DIAL   106  // Dial
#define GAMEPAD_AXIS_SLIDER 107  // Slider

// Hat switch codes (D-pad as buttons)
#define GAMEPAD_HAT_UP      200
#define GAMEPAD_HAT_DOWN    201
#define GAMEPAD_HAT_LEFT    202
#define GAMEPAD_HAT_RIGHT   203

// Maximum gamepads supported
#ifndef MAX_GAMEPADS
#define MAX_GAMEPADS 4
#endif

// HID Interface Index for Gamepads
// Interface 0: Keyboard
// Interface 1: Mouse
// Interface 2-5: Gamepads 0-3
enum {
    ITF_NUM_GAMEPAD_0 = 2,
    ITF_NUM_GAMEPAD_1 = 3,
    ITF_NUM_GAMEPAD_2 = 4,
    ITF_NUM_GAMEPAD_3 = 5
};

class TinyUsbGamepadDevice : public AbstractVirtualDevice {
public:
    // Constructor: gamepad_index (0-3), num_buttons (1-32), axes_bitmask (FLAG_MASK_GAMEPAD_AXIS_*), has_hat
    TinyUsbGamepadDevice(uint8_t gamepad_index = 0, uint8_t num_buttons = 16, uint8_t axes_bitmask = 0x3F, bool has_hat = true);
    virtual ~TinyUsbGamepadDevice();

    // AbstractVirtualDevice interface
    void setAxis(int code, int value) override;
    void setOnEvent(std::function<void(int, int)> lambda) override;
    bool init() override;
    void update() override;

    // Direct control methods
    void pressButton(uint8_t button);
    void releaseButton(uint8_t button);
    void setAxisValue(uint8_t axis, uint8_t value);  // axis 0-7, value 0-255
    void setHat(int8_t direction);  // 0-7 for directions, -1 or 0x0F for center

    // TinyUSB callbacks
    uint16_t getReportDescriptor(uint8_t* buffer, uint16_t reqlen);
    uint16_t getReport(uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen);
    void setReport(uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize);

    // Accessors
    uint8_t getInterfaceNum() const { return ITF_NUM_GAMEPAD_0 + gamepadIndex; }
    uint8_t getGamepadIndex() const { return gamepadIndex; }
    uint8_t getNumButtons() const { return numButtons; }
    uint8_t getAxesBitMask() const { return axesBitMask; }
    uint8_t getNumAxes() const;  // Count of enabled axes
    bool getHasHat() const { return hasHat; }
    uint16_t getHidDescriptorSize() const { return hidDescriptorSize; }

private:
    // Packed report structure matching the HID descriptor exactly
    // This is sent directly to the host
    struct __attribute__((packed)) GamepadReport {
        uint32_t buttons;                   // 32 buttons as bitfield
        uint8_t axes[GAMEPAD_MAX_AXES];     // 8 axes, 0-255 each
        uint8_t hat;                        // Hat switch: 0-7 directions, 0x0F = center
    };

    GamepadReport report;

    // Configuration
    uint8_t gamepadIndex;
    uint8_t numButtons;
    uint8_t axesBitMask;  // Bitmask of enabled axes
    bool hasHat;

    // D-pad button states for calculating hat
    bool hatUp, hatDown, hatLeft, hatRight;

    // State tracking
    bool reportChanged;

    // Event callback
    std::function<void(int, int)> eventCallback;

    // Dynamic HID descriptor
    uint8_t* hidDescriptor;
    uint16_t hidDescriptorSize;
    uint16_t reportSize;  // Actual size of report to send

    // Axis mapping: maps axis code (GAMEPAD_AXIS_LX, etc.) to report index
    int8_t axisCodeToReportIndex[8];  // -1 if axis not enabled

    // Internal helpers
    void buildHidDescriptor();
    void sendReport();
    void updateHatFromButtons();
    uint8_t calculateHatValue() const;
    uint8_t getAxisReportIndex(uint8_t axisCode) const;
};

// Forward declaration
enum class DeviceType;

// ==============================================================================
// TinyUsbGamepadBuilder - Builder pattern for creating gamepad devices
// ==============================================================================
class TinyUsbGamepadBuilder {
public:
    TinyUsbGamepadBuilder();
    
    // Builder methods
    TinyUsbGamepadBuilder& name(const std::string& deviceName);
    TinyUsbGamepadBuilder& gamepadIndex(uint8_t index);
    TinyUsbGamepadBuilder& buttons(uint8_t count);
    TinyUsbGamepadBuilder& axes(uint8_t bitmask);  // Use FLAG_MASK_GAMEPAD_AXIS_* flags
    TinyUsbGamepadBuilder& hat(bool enable = true);
    
    // Build the device
    AbstractVirtualDevice* build();
    
    // Get configured properties
    std::string getName() const;
    DeviceType getDeviceType() const;
    uint8_t getAxesCount() const;

private:
    std::string m_name;
    uint8_t m_gamepadIndex;
    uint8_t m_buttonCount;
    uint8_t m_axesBitMask;
    bool m_hasHat;
};

#endif // TINYUSB_GAMEPAD_DEVICE_H
