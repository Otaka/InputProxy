#ifndef TINYUSB_KEYBOARD_DEVICE_H
#define TINYUSB_KEYBOARD_DEVICE_H

#include "AbstractVirtualDevice.h"
#include <functional>
#include <stdint.h>
#include <cstring>
#include <string>
#include "tusb.h"

// HID Report IDs
#define REPORT_ID_KEYBOARD_BOOT    1  // 6KRO boot keyboard
#define REPORT_ID_KEYBOARD_NKRO    2  // NKRO bitmap keyboard
#define REPORT_ID_CONSUMER_CONTROL 3  // Multimedia keys

// Common Consumer Control (Multimedia) Usage Codes
// Use with pressConsumerKey(uint16_t usage_code)
// 0x00E9 - Volume Increment (Volume Up)
// 0x00EA - Volume Decrement (Volume Down)
// 0x00E2 - Mute
// 0x00B0 - Play
// 0x00B5 - Scan Next Track
// 0x00B6 - Scan Previous Track
// 0x00B7 - Stop
// 0x00CD - Play/Pause

// HID Interface Index for Keyboard (defined in usb_descriptors.c)
// Interface 0: Keyboard
// Interface 1-3: Reserved for future devices (gamepads, etc.)
enum {
    ITF_NUM_KEYBOARD = 0
};

class TinyUsbKeyboardDevice : public AbstractVirtualDevice {
public:
    TinyUsbKeyboardDevice();
    virtual ~TinyUsbKeyboardDevice() = default;

    // AbstractVirtualDevice interface implementation
    void setAxis(int code, int value) override;
    void setOnEvent(std::function<void(int, int)> lambda) override;
    bool init() override;
    void update() override;

    // Consumer control (multimedia keys)
    void pressConsumerKey(uint16_t usage_code);
    void releaseConsumerKey(uint16_t usage_code = 0); // Pass 0 to release all keys

    // TinyUSB HID callbacks (called from tud_hid_* callbacks)
    uint16_t getReportDescriptor(uint8_t* buffer, uint16_t reqlen);
    uint16_t getReport(uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen);
    void setReport(uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize);

    // Get interface number
    uint8_t getInterfaceNum() const { return ITF_NUM_KEYBOARD; }

private:
    // NKRO bitmap: 256 bits for all possible keycodes
    uint8_t nkroKeymap[32]; // 32 bytes = 256 bits

    // 6KRO boot keyboard report
    struct BootKeyboardReport {
        uint8_t modifiers; // 8 modifier keys (Ctrl, Shift, Alt, GUI)
        uint8_t reserved;  // Reserved byte
        uint8_t keys[6];   // Up to 6 simultaneous keys
    } __attribute__((packed));

    BootKeyboardReport bootReport;

    // Consumer control (multimedia keys) state - up to 30 simultaneous keys
    uint16_t consumerKeys[30];

    // LED state from PC (CapsLock, NumLock, ScrollLock)
    uint8_t keyboardLeds;

    // Callback for events from PC
    std::function<void(int, int)> eventCallback;

    // Report tracking
    bool bootReportChanged;
    bool nkroReportChanged;
    bool consumerReportChanged;

    // Helper methods
    void pressKey(uint8_t keycode);
    void releaseKey(uint8_t keycode);
    void updateBootReport();
    void sendReports();
    bool isKeyPressed(uint8_t keycode);
    bool isModifierKey(uint8_t keycode);
    uint8_t getModifierBit(uint8_t keycode);
};

// Global HID report descriptor for keyboard interface
extern const uint8_t hid_report_descriptor_keyboard[];
extern const uint16_t hid_report_descriptor_keyboard_size;

// Forward declaration
enum class DeviceType;

// ==============================================================================
// TinyUsbKeyboardBuilder - Builder pattern for creating keyboard devices
// ==============================================================================
class TinyUsbKeyboardBuilder {
public:
    TinyUsbKeyboardBuilder();
    
    // Builder methods
    TinyUsbKeyboardBuilder& name(const std::string& deviceName);
    
    // Build the device
    AbstractVirtualDevice* build();
    
    // Get configured properties
    std::string getName() const;
    DeviceType getDeviceType() const;

private:
    std::string m_name;
};

#endif // TINYUSB_KEYBOARD_DEVICE_H
