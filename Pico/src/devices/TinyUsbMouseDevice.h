#ifndef TINYUSB_MOUSE_DEVICE_H
#define TINYUSB_MOUSE_DEVICE_H

#include "AbstractVirtualDevice.h"
#include <functional>
#include <stdint.h>
#include <cstring>
#include <string>
#include "tusb.h"

// HID Report IDs
#define REPORT_ID_MOUSE 1  // Standard mouse with buttons, X/Y movement, and wheel

// Mouse button codes (for setAxis) - continuous from 1
#define MOUSE_BUTTON_LEFT   1
#define MOUSE_BUTTON_RIGHT  2
#define MOUSE_BUTTON_MIDDLE 3
#define MOUSE_BUTTON_BACK   4
#define MOUSE_BUTTON_FORWARD 5

// Mouse axis codes (for setAxis) - continuous from 6
// Each axis split into minus (left/down) and plus (right/up)
// Values are 0-1000, setting one direction resets the opposite
#define MOUSE_AXIS_X_MINUS      6  // X Left
#define MOUSE_AXIS_X_PLUS       7  // X Right
#define MOUSE_AXIS_Y_MINUS      8  // Y Up
#define MOUSE_AXIS_Y_PLUS       9  // Y Down
#define MOUSE_AXIS_WHEEL_MINUS  10  // Wheel Down
#define MOUSE_AXIS_WHEEL_PLUS   11  // Wheel Up
#define MOUSE_AXIS_H_WHEEL_MINUS 12 // H-Wheel Left
#define MOUSE_AXIS_H_WHEEL_PLUS  13 // H-Wheel Right

// HID Interface Index for Mouse (defined in usb_descriptors.c)
// Interface 0: Keyboard
// Interface 1: Mouse
enum {
    ITF_NUM_MOUSE = 1
};

class TinyUsbMouseDevice : public AbstractVirtualDevice {
public:
    TinyUsbMouseDevice();
    virtual ~TinyUsbMouseDevice() = default;

    // AbstractVirtualDevice interface implementation
    void setAxis(int code, int value) override;
    void setOnEvent(std::function<void(int, int)> lambda) override;
    bool init() override;
    void update() override;
    AxesDescription axesDescription() override;

    // Mouse control methods
    void pressButton(uint8_t button);      // 0=left, 1=right, 2=middle, 3=back, 4=forward
    void releaseButton(uint8_t button);
    void moveRelative(int8_t x, int8_t y);  // Relative movement
    void scroll(int8_t wheel, int8_t h_wheel = 0); // Vertical and horizontal scroll

    // TinyUSB HID callbacks (called from tud_hid_* callbacks)
    uint16_t getReportDescriptor(uint8_t* buffer, uint16_t reqlen);
    uint16_t getReport(uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen);
    void setReport(uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize);

    // Get interface number
    uint8_t getInterfaceNum() const { return ITF_NUM_MOUSE; }

private:
    // Mouse report structure
    struct MouseReport {
        uint8_t buttons;   // Bit 0: left, 1: right, 2: middle, 3: back, 4: forward
        int8_t x;          // Relative X movement
        int8_t y;          // Relative Y movement
        int8_t wheel;      // Vertical wheel
        int8_t h_wheel;    // Horizontal wheel (pan)
    } __attribute__((packed));

    MouseReport mouseReport;

    // Callback for events from PC
    std::function<void(int, int)> eventCallback;

    // Report tracking
    bool reportChanged;

    // Helper methods
    void sendReport();
};

// Global HID report descriptor for mouse interface
extern const uint8_t hid_report_descriptor_mouse[];
extern const uint16_t hid_report_descriptor_mouse_size;

// Forward declaration
enum class DeviceType;

// ==============================================================================
// TinyUsbMouseBuilder - Builder pattern for creating mouse devices
// ==============================================================================
class TinyUsbMouseBuilder {
public:
    TinyUsbMouseBuilder();
    
    // Builder methods
    TinyUsbMouseBuilder& name(const std::string& deviceName);
    
    // Build the device
    AbstractVirtualDevice* build();
    
    // Get configured properties
    std::string getName() const;
    DeviceType getDeviceType() const;

private:
    std::string m_name;
};

#endif // TINYUSB_MOUSE_DEVICE_H
