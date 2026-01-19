#include "TinyUsbGamepadDevice2.h"
#include "tusb.h"

// HID Report Descriptor for Gamepad
// Structure: 32 buttons (4 bytes) + 8 axes (8 bytes) + 1 hat switch (1 byte) = 13 bytes
const uint8_t hid_report_descriptor_gamepad[] = {
    HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),
    HID_USAGE(HID_USAGE_DESKTOP_GAMEPAD),
    HID_COLLECTION(HID_COLLECTION_APPLICATION),
        HID_REPORT_ID(REPORT_ID_GAMEPAD)

        // 32 Buttons (4 bytes)
        HID_USAGE_PAGE(HID_USAGE_PAGE_BUTTON),
        HID_USAGE_MIN(1),
        HID_USAGE_MAX(32),
        HID_LOGICAL_MIN(0),
        HID_LOGICAL_MAX(1),
        HID_REPORT_COUNT(32),
        HID_REPORT_SIZE(1),
        HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),

        // 8 Axes (8 bytes) - all as generic axes for maximum compatibility
        HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),
        HID_USAGE(HID_USAGE_DESKTOP_X),
        HID_USAGE(HID_USAGE_DESKTOP_Y),
        HID_USAGE(HID_USAGE_DESKTOP_Z),
        HID_USAGE(HID_USAGE_DESKTOP_RZ),
        HID_USAGE(HID_USAGE_DESKTOP_RX),
        HID_USAGE(HID_USAGE_DESKTOP_RY),
        HID_USAGE(HID_USAGE_DESKTOP_SLIDER),
        HID_USAGE(HID_USAGE_DESKTOP_DIAL),
        HID_LOGICAL_MIN(0),
        HID_LOGICAL_MAX_N(255, 2),
        HID_REPORT_COUNT(8),
        HID_REPORT_SIZE(8),
        HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),

        // Hat Switch (1 byte)
        HID_USAGE(HID_USAGE_DESKTOP_HAT_SWITCH),
        HID_LOGICAL_MIN(0),
        HID_LOGICAL_MAX(7),
        HID_PHYSICAL_MIN(0),
        HID_PHYSICAL_MAX_N(315, 2),
        HID_REPORT_COUNT(1),
        HID_REPORT_SIZE(8),
        HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE | HID_NULL_STATE),

    HID_COLLECTION_END,
};

const uint16_t hid_report_descriptor_gamepad_size = sizeof(hid_report_descriptor_gamepad);

TinyUsbGamepadDevice::TinyUsbGamepadDevice(uint8_t gamepad_index, uint8_t num_buttons, uint8_t num_axes, bool has_hat)
    : gamepadIndex(gamepad_index)
    , numButtons(num_buttons > GAMEPAD_MAX_BUTTONS ? GAMEPAD_MAX_BUTTONS : num_buttons)
    , numAxes(num_axes > GAMEPAD_MAX_AXES ? GAMEPAD_MAX_AXES : num_axes)
    , hasHat(has_hat)
    , hatUp(false)
    , hatDown(false)
    , hatLeft(false)
    , hatRight(false)
    , reportChanged(false)
{
    memset(&report, 0, sizeof(report));
    // Center all axes at 127
    for (int i = 0; i < GAMEPAD_MAX_AXES; i++) {
        report.axes[i] = 127;
    }
    // Hat centered (null state)
    report.hat = 0x0F;
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

    // Axes (codes 100-107)
    if (code >= GAMEPAD_AXIS_LX && code <= GAMEPAD_AXIS_7) {
        uint8_t axisIndex = code - GAMEPAD_AXIS_LX;
        // Scale 0-1000 to 0-255
        uint8_t scaledValue = static_cast<uint8_t>((value * 255) / 1000);
        setAxisValue(axisIndex, scaledValue);
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
    // Send report using the same pattern as the working mouse implementation
    // The report ID is passed separately, TinyUSB handles prepending it
    tud_hid_n_report(getInterfaceNum(), REPORT_ID_GAMEPAD, &report, sizeof(report));
    reportChanged = false;
}

uint16_t TinyUsbGamepadDevice::getReportDescriptor(uint8_t* buffer, uint16_t reqlen) {
    uint16_t len = hid_report_descriptor_gamepad_size;
    if (reqlen < len) {
        len = reqlen;
    }
    memcpy(buffer, hid_report_descriptor_gamepad, len);
    return len;
}

uint16_t TinyUsbGamepadDevice::getReport(uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
    (void)report_type;

    if (report_id == REPORT_ID_GAMEPAD) {
        if (reqlen < sizeof(report)) {
            return 0;
        }
        memcpy(buffer, &report, sizeof(report));
        return sizeof(report);
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
