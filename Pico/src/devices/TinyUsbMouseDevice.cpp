#include "TinyUsbMouseDevice.h"
#include "DeviceManager.h"
#include "tusb.h"

// HID Report Descriptor for Mouse Interface
// Supports: Standard 5-button mouse with wheel and horizontal wheel
const uint8_t hid_report_descriptor_mouse[] = {
    // Standard Mouse Report (Report ID 1)
    HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),
    HID_USAGE(HID_USAGE_DESKTOP_MOUSE),
    HID_COLLECTION(HID_COLLECTION_APPLICATION),
        HID_REPORT_ID(REPORT_ID_MOUSE)
        HID_USAGE(HID_USAGE_DESKTOP_POINTER),
        HID_COLLECTION(HID_COLLECTION_PHYSICAL),
            // Buttons (5 buttons: left, right, middle, back, forward)
            HID_USAGE_PAGE(HID_USAGE_PAGE_BUTTON),
            HID_USAGE_MIN(1),
            HID_USAGE_MAX(5),
            HID_LOGICAL_MIN(0),
            HID_LOGICAL_MAX(1),
            HID_REPORT_COUNT(5),
            HID_REPORT_SIZE(1),
            HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),
            
            // Padding (3 bits to complete the byte)
            HID_REPORT_COUNT(1),
            HID_REPORT_SIZE(3),
            HID_INPUT(HID_CONSTANT),

            // X, Y movement (relative)
            HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),
            HID_USAGE(HID_USAGE_DESKTOP_X),
            HID_USAGE(HID_USAGE_DESKTOP_Y),
            HID_LOGICAL_MIN(0x81),  // -127 in two's complement (8-bit signed)
            HID_LOGICAL_MAX(127),
            HID_REPORT_COUNT(2),
            HID_REPORT_SIZE(8),
            HID_INPUT(HID_DATA | HID_VARIABLE | HID_RELATIVE),

            // Vertical wheel
            HID_USAGE(HID_USAGE_DESKTOP_WHEEL),
            HID_LOGICAL_MIN(0x81),  // -127 in two's complement (8-bit signed)
            HID_LOGICAL_MAX(127),
            HID_REPORT_COUNT(1),
            HID_REPORT_SIZE(8),
            HID_INPUT(HID_DATA | HID_VARIABLE | HID_RELATIVE),

            // Horizontal wheel (AC Pan)
            HID_USAGE_PAGE(HID_USAGE_PAGE_CONSUMER),
            HID_USAGE_N(HID_USAGE_CONSUMER_AC_PAN, 2),
            HID_LOGICAL_MIN(0x81),  // -127 in two's complement (8-bit signed)
            HID_LOGICAL_MAX(127),
            HID_REPORT_COUNT(1),
            HID_REPORT_SIZE(8),
            HID_INPUT(HID_DATA | HID_VARIABLE | HID_RELATIVE),
        HID_COLLECTION_END,
    HID_COLLECTION_END,
};

const uint16_t hid_report_descriptor_mouse_size = sizeof(hid_report_descriptor_mouse);

TinyUsbMouseDevice::TinyUsbMouseDevice()
    : reportChanged(false)
{
    memset(&mouseReport, 0, sizeof(mouseReport));
}

bool TinyUsbMouseDevice::init() {
    // Clear all state
    memset(&mouseReport, 0, sizeof(mouseReport));
    reportChanged = false;

    return true;
}

void TinyUsbMouseDevice::setAxis(int code, int value) {
    // code: Button codes (0-4) for mouse buttons
    //       100-103 for axis movements
    // value: For buttons: 0 = released, non-zero = pressed
    //        For axes: relative movement value (-127 to 127 typically)

    if (code < 0) {
        return;
    }

    // Mouse buttons (0-4)
    if (code >= 0 && code <= 4) {
        if (value > 0) {
            pressButton(static_cast<uint8_t>(code));
        } else {
            releaseButton(static_cast<uint8_t>(code));
        }
        return;
    }

    // Mouse axes (100-107) - split into plus/minus for accurate control
    // Values are 0-1000, clamped to 0-127 for HID report
    switch (code) {
        case MOUSE_AXIS_X_MINUS:  // Left
            mouseReport.x = -static_cast<int8_t>(value > 127 ? 127 : value);
            reportChanged = true;
            break;
        case MOUSE_AXIS_X_PLUS:   // Right
            mouseReport.x = static_cast<int8_t>(value > 127 ? 127 : value);
            reportChanged = true;
            break;
        case MOUSE_AXIS_Y_MINUS:  // Up (negative Y)
            mouseReport.y = -static_cast<int8_t>(value > 127 ? 127 : value);
            reportChanged = true;
            break;
        case MOUSE_AXIS_Y_PLUS:   // Down (positive Y)
            mouseReport.y = static_cast<int8_t>(value > 127 ? 127 : value);
            reportChanged = true;
            break;
        case MOUSE_AXIS_WHEEL_MINUS:  // Wheel Down
            mouseReport.wheel = -static_cast<int8_t>(value > 127 ? 127 : value);
            reportChanged = true;
            break;
        case MOUSE_AXIS_WHEEL_PLUS:   // Wheel Up
            mouseReport.wheel = static_cast<int8_t>(value > 127 ? 127 : value);
            reportChanged = true;
            break;
        case MOUSE_AXIS_H_WHEEL_MINUS:  // H-Wheel Left
            mouseReport.h_wheel = -static_cast<int8_t>(value > 127 ? 127 : value);
            reportChanged = true;
            break;
        case MOUSE_AXIS_H_WHEEL_PLUS:   // H-Wheel Right
            mouseReport.h_wheel = static_cast<int8_t>(value > 127 ? 127 : value);
            reportChanged = true;
            break;
    }
}

void TinyUsbMouseDevice::setOnEvent(std::function<void(int, int)> lambda) {
    eventCallback = lambda;
}

void TinyUsbMouseDevice::pressButton(uint8_t button) {
    if (button > 4) {
        return; // Invalid button
    }

    mouseReport.buttons |= (1 << button);
    reportChanged = true;
}

void TinyUsbMouseDevice::releaseButton(uint8_t button) {
    if (button > 4) {
        return; // Invalid button
    }

    mouseReport.buttons &= ~(1 << button);
    reportChanged = true;
}

void TinyUsbMouseDevice::moveRelative(int8_t x, int8_t y) {
    mouseReport.x = x;
    mouseReport.y = y;
    reportChanged = true;
}

void TinyUsbMouseDevice::scroll(int8_t wheel, int8_t h_wheel) {
    mouseReport.wheel = wheel;
    mouseReport.h_wheel = h_wheel;
    reportChanged = true;
}

void TinyUsbMouseDevice::update() {
    // Send report if USB is ready and there are changes
    if (tud_hid_n_ready(ITF_NUM_MOUSE) && reportChanged) {
        sendReport();
    }
}

void TinyUsbMouseDevice::sendReport() {
    // Send mouse report
    tud_hid_n_report(ITF_NUM_MOUSE, REPORT_ID_MOUSE, &mouseReport, sizeof(mouseReport));
    
    // After sending movement/scroll, reset those values (buttons remain)
    // This prevents repeated movements from a single input
    mouseReport.x = 0;
    mouseReport.y = 0;
    mouseReport.wheel = 0;
    mouseReport.h_wheel = 0;
    
    reportChanged = false;
}

uint16_t TinyUsbMouseDevice::getReportDescriptor(uint8_t* buffer, uint16_t reqlen) {
    uint16_t len = hid_report_descriptor_mouse_size;
    if (reqlen < len) {
        len = reqlen;
    }
    memcpy(buffer, hid_report_descriptor_mouse, len);
    return len;
}

uint16_t TinyUsbMouseDevice::getReport(uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
    (void) report_type;

    if (report_id == REPORT_ID_MOUSE) {
        if (reqlen < sizeof(mouseReport)) {
            return 0;
        }
        memcpy(buffer, &mouseReport, sizeof(mouseReport));
        return sizeof(mouseReport);
    }

    return 0;
}

void TinyUsbMouseDevice::setReport(uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) bufsize;
    
    // Mouse typically doesn't receive SET_REPORT
    // If needed in the future, implement here
}

// ==============================================================================
// TinyUsbMouseBuilder - Builder pattern for creating mouse devices
// ==============================================================================

TinyUsbMouseBuilder::TinyUsbMouseBuilder()
    : m_name("Mouse")
{
}

TinyUsbMouseBuilder& TinyUsbMouseBuilder::name(const std::string& deviceName) {
    m_name = deviceName;
    return *this;
}

AbstractVirtualDevice* TinyUsbMouseBuilder::build() {
    return new TinyUsbMouseDevice();
}

std::string TinyUsbMouseBuilder::getName() const {
    return m_name;
}

DeviceType TinyUsbMouseBuilder::getDeviceType() const {
    return DeviceType::MOUSE;
}
