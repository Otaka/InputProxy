#include "TinyUsbGamepadDevice.h"
#include "DeviceManager.h"
#include "tusb.h"
#include <cstdlib>

// HID Usage constants for axes
static const uint8_t AXIS_USAGE_CODES[8] = {
    HID_USAGE_DESKTOP_X,       // LX
    HID_USAGE_DESKTOP_Y,       // LY
    HID_USAGE_DESKTOP_Z,       // LZ
    HID_USAGE_DESKTOP_RX,      // RX
    HID_USAGE_DESKTOP_RY,      // RY
    HID_USAGE_DESKTOP_RZ,      // RZ
    HID_USAGE_DESKTOP_DIAL,    // DIAL
    HID_USAGE_DESKTOP_SLIDER   // SLIDER
};

TinyUsbGamepadDevice::TinyUsbGamepadDevice(uint8_t gamepad_index, uint8_t num_buttons, uint8_t axes_bitmask, bool has_hat)
    : gamepadIndex(gamepad_index)
    , numButtons(num_buttons > GAMEPAD_MAX_BUTTONS ? GAMEPAD_MAX_BUTTONS : num_buttons)
    , axesBitMask(axes_bitmask)
    , hasHat(has_hat)
    , hatUp(false)
    , hatDown(false)
    , hatLeft(false)
    , hatRight(false)
    , reportChanged(false)
    , hidDescriptor(nullptr)
    , hidDescriptorSize(0)
{
    // Initialize axis mapping to -1 (disabled)
    for (int i = 0; i < 8; i++) {
        axisCodeToReportIndex[i] = -1;
    }

    // Build axis mapping based on bitmask
    uint8_t reportIndex = 0;
    for (int i = 0; i < 8; i++) {
        if (axesBitMask & (1 << i)) {
            axisCodeToReportIndex[i] = reportIndex++;
        }
    }

    memset(&report, 0, sizeof(report));
    // Center all enabled axes at 127
    for (int i = 0; i < GAMEPAD_MAX_AXES; i++) {
        report.axes[i] = 127;
    }
    // Hat centered (null state)
    report.hat = 0x0F;

    // Build the HID descriptor
    buildHidDescriptor();
    
    // Calculate actual report size based on configuration
    // Buttons: round up to nearest byte
    uint8_t buttonBytes = (numButtons + 7) / 8;
    uint8_t numEnabledAxes = getNumAxes();
    reportSize = buttonBytes + numEnabledAxes;  // buttons + axes
    if (hasHat) {
        reportSize += 1;  // +1 for hat
    }
}

TinyUsbGamepadDevice::~TinyUsbGamepadDevice() {
    if (hidDescriptor) {
        free(hidDescriptor);
        hidDescriptor = nullptr;
    }
}

uint8_t TinyUsbGamepadDevice::getNumAxes() const {
    uint8_t count = 0;
    for (int i = 0; i < 8; i++) {
        if (axesBitMask & (1 << i)) {
            count++;
        }
    }
    return count;
}

uint8_t TinyUsbGamepadDevice::getAxisReportIndex(uint8_t axisCode) const {
    if (axisCode < GAMEPAD_AXIS_LX || axisCode > GAMEPAD_AXIS_SLIDER) {
        return 0xFF;  // Invalid
    }
    uint8_t axisIndex = axisCode - GAMEPAD_AXIS_LX;
    int8_t reportIndex = axisCodeToReportIndex[axisIndex];
    return (reportIndex >= 0) ? static_cast<uint8_t>(reportIndex) : 0xFF;
}

void TinyUsbGamepadDevice::buildHidDescriptor() {
    uint8_t numEnabledAxes = getNumAxes();

    // Build descriptor byte-by-byte to handle runtime values correctly
    uint8_t tempBuffer[256];
    uint8_t* p = tempBuffer;

    // Usage Page (Generic Desktop)
    *p++ = 0x05; *p++ = 0x01;
    // Usage (Gamepad)
    *p++ = 0x09; *p++ = 0x05;
    // Collection (Application)
    *p++ = 0xA1; *p++ = 0x01;
    // Report ID
    *p++ = 0x85; *p++ = REPORT_ID_GAMEPAD;

    // === Buttons ===
    // Only include button section if numButtons > 0
    if (numButtons > 0) {
        // Usage Page (Button)
        *p++ = 0x05; *p++ = 0x09;
        // Usage Minimum (1)
        *p++ = 0x19; *p++ = 0x01;
        // Usage Maximum (numButtons)
        *p++ = 0x29; *p++ = numButtons;
        // Logical Minimum (0)
        *p++ = 0x15; *p++ = 0x00;
        // Logical Maximum (1)
        *p++ = 0x25; *p++ = 0x01;
        // Report Count (numButtons)
        *p++ = 0x95; *p++ = numButtons;
        // Report Size (1)
        *p++ = 0x75; *p++ = 0x01;
        // Input (Data, Variable, Absolute)
        *p++ = 0x81; *p++ = 0x02;

        // Padding if buttons don't fill complete bytes
        uint8_t paddingBits = (8 - (numButtons % 8)) % 8;
        if (paddingBits > 0) {
            // Report Count (padding)
            *p++ = 0x95; *p++ = paddingBits;
            // Report Size (1)
            *p++ = 0x75; *p++ = 0x01;
            // Input (Constant)
            *p++ = 0x81; *p++ = 0x01;
        }
    }

    // === Axes ===
    if (numEnabledAxes > 0) {
        // Usage Page (Generic Desktop)
        *p++ = 0x05; *p++ = 0x01;

        // Add Usage for each enabled axis
        for (int i = 0; i < 8; i++) {
            if (axesBitMask & (1 << i)) {
                *p++ = 0x09;  // Usage tag
                *p++ = AXIS_USAGE_CODES[i];
            }
        }

        // Logical Minimum (0)
        *p++ = 0x15; *p++ = 0x00;
        // Logical Maximum (255) - 2 byte format
        *p++ = 0x26; *p++ = 0xFF; *p++ = 0x00;
        // Report Count (numEnabledAxes)
        *p++ = 0x95; *p++ = numEnabledAxes;
        // Report Size (8)
        *p++ = 0x75; *p++ = 0x08;
        // Input (Data, Variable, Absolute)
        *p++ = 0x81; *p++ = 0x02;
    }

    // === Hat Switch ===
    if (hasHat) {
        // Usage Page (Generic Desktop) - required for Hat Switch
        *p++ = 0x05; *p++ = 0x01;
        // Usage (Hat Switch)
        *p++ = 0x09; *p++ = 0x39;
        // Logical Minimum (0)
        *p++ = 0x15; *p++ = 0x00;
        // Logical Maximum (7)
        *p++ = 0x25; *p++ = 0x07;
        // Physical Minimum (0)
        *p++ = 0x35; *p++ = 0x00;
        // Physical Maximum (315) - 2 byte format
        *p++ = 0x46; *p++ = 0x3B; *p++ = 0x01;
        // Report Count (1)
        *p++ = 0x95; *p++ = 0x01;
        // Report Size (8)
        *p++ = 0x75; *p++ = 0x08;
        // Input (Data, Variable, Absolute, Null State)
        *p++ = 0x81; *p++ = 0x42;
    }

    // End Collection
    *p++ = 0xC0;

    // Calculate actual size
    hidDescriptorSize = p - tempBuffer;

    // Allocate and copy
    hidDescriptor = (uint8_t*)malloc(hidDescriptorSize);
    if (hidDescriptor) {
        memcpy(hidDescriptor, tempBuffer, hidDescriptorSize);
    } else {
        hidDescriptorSize = 0;
    }
}

bool TinyUsbGamepadDevice::init() {
    memset(&report, 0, sizeof(report));

    // Center all axes
    for (int i = 0; i < GAMEPAD_MAX_AXES; i++) {
        report.axes[i] = 127;
    }

    report.hat = 0x0F;  // Centered
    reportChanged = true;  // Send initial state
    hatUp = hatDown = hatLeft = hatRight = false;

    return true;
}

void TinyUsbGamepadDevice::setAxis(int code, int value) {
    if (code < 0) return;

    // Buttons (codes 0-31)
    if (code < GAMEPAD_MAX_BUTTONS) {
        if (value > 0) {
            pressButton(static_cast<uint8_t>(code));
        } else {
            releaseButton(static_cast<uint8_t>(code));
        }
        return;
    }

    // Axes (codes 100-107) - Full range
    if (code >= GAMEPAD_AXIS_LX && code <= GAMEPAD_AXIS_SLIDER) {
        // Map axis code to base axis index (0-7)
        uint8_t baseAxisIndex = code - GAMEPAD_AXIS_LX;
        int8_t reportIndex = axisCodeToReportIndex[baseAxisIndex];
        if (reportIndex >= 0) {
            // Scale 0-1000 to 0-255
            uint8_t scaledValue = static_cast<uint8_t>((value * 255) / 1000);
            setAxisValue(static_cast<uint8_t>(reportIndex), scaledValue);
        }
        return;
    }

    // Axes MINUS codes (110-124, even numbers) - Negative direction (0-127)
    if (code >= GAMEPAD_AXIS_LX_MINUS && code <= GAMEPAD_AXIS_SLIDER_MINUS) {
        // Determine which base axis this corresponds to
        uint8_t baseAxisIndex = (code - GAMEPAD_AXIS_LX_MINUS) / 2;
        if (baseAxisIndex < 8) {
            int8_t reportIndex = axisCodeToReportIndex[baseAxisIndex];
            if (reportIndex >= 0) {
                // Scale 0-1000 to 0-127 (negative direction)
                uint8_t scaledValue = static_cast<uint8_t>((value * 127) / 1000);
                setAxisValue(static_cast<uint8_t>(reportIndex), scaledValue);
            }
        }
        return;
    }

    // Axes PLUS codes (111-125, odd numbers) - Positive direction (127-255)
    if (code >= GAMEPAD_AXIS_LX_PLUS && code <= GAMEPAD_AXIS_SLIDER_PLUS) {
        // Determine which base axis this corresponds to
        uint8_t baseAxisIndex = (code - GAMEPAD_AXIS_LX_PLUS) / 2;
        if (baseAxisIndex < 8) {
            int8_t reportIndex = axisCodeToReportIndex[baseAxisIndex];
            if (reportIndex >= 0) {
                // Scale 0-1000 to 127-255 (positive direction)
                // Map: 0->127, 1000->255 (range of 128 values)
                uint8_t scaledValue = 127 + static_cast<uint8_t>((value * 128) / 1000);
                setAxisValue(static_cast<uint8_t>(reportIndex), scaledValue);
            }
        }
        return;
    }

    // Hat switch (codes 200-203)
    if (code >= GAMEPAD_HAT_UP && code <= GAMEPAD_HAT_RIGHT) {
        bool pressed = (value > 0);
        switch (code) {
            case GAMEPAD_HAT_UP:    hatUp = pressed; break;
            case GAMEPAD_HAT_DOWN:  hatDown = pressed; break;
            case GAMEPAD_HAT_LEFT:  hatLeft = pressed; break;
            case GAMEPAD_HAT_RIGHT: hatRight = pressed; break;
        }
        updateHatFromButtons();
        return;
    }
}

void TinyUsbGamepadDevice::setOnEvent(std::function<void(int, int)> lambda) {
    eventCallback = lambda;
}

void TinyUsbGamepadDevice::pressButton(uint8_t button) {
    if (button >= GAMEPAD_MAX_BUTTONS) return;

    uint32_t mask = (1UL << button);
    if (!(report.buttons & mask)) {
        report.buttons |= mask;
        reportChanged = true;
    }
}

void TinyUsbGamepadDevice::releaseButton(uint8_t button) {
    if (button >= GAMEPAD_MAX_BUTTONS) return;

    uint32_t mask = (1UL << button);
    if (report.buttons & mask) {
        report.buttons &= ~mask;
        reportChanged = true;
    }
}

void TinyUsbGamepadDevice::setAxisValue(uint8_t axis, uint8_t value) {
    if (axis >= GAMEPAD_MAX_AXES) return;

    if (report.axes[axis] != value) {
        report.axes[axis] = value;
        reportChanged = true;
    }
}

void TinyUsbGamepadDevice::setHat(int8_t direction) {
    uint8_t hatValue;
    if (direction < 0 || direction > 7) {
        hatValue = 0x0F;  // Centered/null
    } else {
        hatValue = static_cast<uint8_t>(direction);
    }

    if (report.hat != hatValue) {
        report.hat = hatValue;
        reportChanged = true;
    }
}

void TinyUsbGamepadDevice::updateHatFromButtons() {
    uint8_t newHat = calculateHatValue();
    if (report.hat != newHat) {
        report.hat = newHat;
        reportChanged = true;
    }
}

uint8_t TinyUsbGamepadDevice::calculateHatValue() const {
    // Hat values: 0=N, 1=NE, 2=E, 3=SE, 4=S, 5=SW, 6=W, 7=NW, 0x0F=center
    if (!hatUp && !hatDown && !hatLeft && !hatRight) return 0x0F;

    if (hatUp && !hatDown) {
        if (hatRight && !hatLeft) return 1;      // NE
        if (hatLeft && !hatRight) return 7;      // NW
        return 0;                                 // N
    }
    if (hatDown && !hatUp) {
        if (hatRight && !hatLeft) return 3;      // SE
        if (hatLeft && !hatRight) return 5;      // SW
        return 4;                                 // S
    }
    if (hatRight && !hatLeft) return 2;          // E
    if (hatLeft && !hatRight) return 6;          // W

    return 0x0F;  // Conflicting inputs, center
}

void TinyUsbGamepadDevice::update() {
    if (reportChanged && tud_hid_n_ready(getInterfaceNum())) {
        sendReport();
    }
}

void TinyUsbGamepadDevice::sendReport() {
    // Build report buffer matching the HID descriptor layout exactly
    uint8_t reportBuffer[64];  // Max possible report size
    uint8_t* p = reportBuffer;
    
    // Buttons section (only if numButtons > 0)
    if (numButtons > 0) {
        uint8_t buttonBytes = (numButtons + 7) / 8;
        // Copy button bytes from the buttons bitfield
        memcpy(p, &report.buttons, buttonBytes);
        p += buttonBytes;
    }
    
    // Axes section (only enabled axes, stored sequentially in report.axes)
    uint8_t numEnabledAxes = getNumAxes();
    if (numEnabledAxes > 0) {
        uint8_t reportIndex = 0;
        for (int i = 0; i < 8; i++) {
            if (axesBitMask & (1 << i)) {
                *p++ = report.axes[reportIndex++];
            }
        }
    }
    
    // Hat section (only if hasHat)
    if (hasHat) {
        *p++ = report.hat;
    }
    
    // Send the properly formatted report
    tud_hid_n_report(getInterfaceNum(), REPORT_ID_GAMEPAD, reportBuffer, reportSize);
    reportChanged = false;
}

uint16_t TinyUsbGamepadDevice::getReportDescriptor(uint8_t* buffer, uint16_t reqlen) {
    if (!hidDescriptor || hidDescriptorSize == 0) {
        return 0;
    }
    uint16_t len = hidDescriptorSize;
    if (reqlen < len) {
        len = reqlen;
    }
    memcpy(buffer, hidDescriptor, len);
    return len;
}

uint16_t TinyUsbGamepadDevice::getReport(uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
    (void)report_type;

    if (report_id == REPORT_ID_GAMEPAD) {
        if (reqlen < reportSize) {
            return 0;
        }

        // Build packed report matching HID descriptor (same as sendReport)
        uint8_t* p = buffer;

        // Buttons section (only if numButtons > 0)
        if (numButtons > 0) {
            uint8_t buttonBytes = (numButtons + 7) / 8;
            memcpy(p, &report.buttons, buttonBytes);
            p += buttonBytes;
        }

        // Axes section (only enabled axes, stored sequentially in report.axes)
        uint8_t numEnabledAxes = getNumAxes();
        if (numEnabledAxes > 0) {
            uint8_t reportIndex = 0;
            for (int i = 0; i < 8; i++) {
                if (axesBitMask & (1 << i)) {
                    *p++ = report.axes[reportIndex++];
                }
            }
        }

        // Hat section (only if hasHat)
        if (hasHat) {
            *p++ = report.hat;
        }

        return reportSize;
    }
    return 0;
}

void TinyUsbGamepadDevice::setReport(uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)bufsize;
    // Gamepad doesn't typically receive reports (no rumble/LED support yet)
}

// ==============================================================================
// TinyUsbGamepadBuilder - Builder pattern for creating gamepad devices
// ==============================================================================

TinyUsbGamepadBuilder::TinyUsbGamepadBuilder()
    : m_name("")
    , m_gamepadIndex(0)
    , m_buttonCount(16)
    , m_axesBitMask(0)  // Default: no axes
    , m_hasHat(true)
{
}

TinyUsbGamepadBuilder& TinyUsbGamepadBuilder::name(const std::string& deviceName) {
    m_name = deviceName;
    return *this;
}

TinyUsbGamepadBuilder& TinyUsbGamepadBuilder::gamepadIndex(uint8_t index) {
    if (index < MAX_GAMEPADS) {
        m_gamepadIndex = index;
    }
    return *this;
}

TinyUsbGamepadBuilder& TinyUsbGamepadBuilder::buttons(uint8_t count) {
    if (count <= GAMEPAD_MAX_BUTTONS) {
        m_buttonCount = count;
    }
    return *this;
}

TinyUsbGamepadBuilder& TinyUsbGamepadBuilder::axes(uint8_t bitmask) {
    m_axesBitMask = bitmask;
    return *this;
}

TinyUsbGamepadBuilder& TinyUsbGamepadBuilder::hat(bool enable) {
    m_hasHat = enable;
    return *this;
}

AbstractVirtualDevice* TinyUsbGamepadBuilder::build() {
    return new TinyUsbGamepadDevice(m_gamepadIndex, m_buttonCount, m_axesBitMask, m_hasHat);
}

std::string TinyUsbGamepadBuilder::getName() const {
    if (!m_name.empty()) {
        return m_name;
    }
    return "Gamepad " + std::to_string(m_gamepadIndex);
}

DeviceType TinyUsbGamepadBuilder::getDeviceType() const {
    return DeviceType::GAMEPAD;
}

uint8_t TinyUsbGamepadBuilder::getAxesCount() const {
    uint8_t count = 0;
    for (int i = 0; i < 8; i++) {
        if (m_axesBitMask & (1 << i)) {
            count++;
        }
    }
    return count;
}
