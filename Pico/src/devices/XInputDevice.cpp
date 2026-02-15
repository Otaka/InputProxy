
#include "XInputDevice.h"
#include "pico/time.h"

XInputDevice::XInputDevice(uint8_t gamepad_index)
    : gamepadIndex(gamepad_index)
    , reportChanged(false)
    , lastReportTime(0)
{
    memset(&report, 0, sizeof(report));
    report.report_id = 0x00;
    report.report_size = 0x14;  // 20 bytes
}

bool XInputDevice::init() {
    // Clear all state
    memset(&report, 0, sizeof(report));
    report.report_id = 0x00;
    report.report_size = 0x14;
    reportChanged = true;  // Force sending initial report
    lastReportTime = 0;

    return true;
}

void XInputDevice::setAxis(int code, int value) {
    // Handle buttons (0-15)
    if (code >= XBOX_BTN_DPAD_UP && code <= XBOX_BTN_Y) {
        setButton(code, value > 0);
        return;
    }

    // Clamp value to 0-1000
    if (value < 0) value = 0;
    if (value > 1000) value = 1000;

    if (code == XBOX_AXIS_LEFT_TRIGGER) {
        report.trigger_l = convertToTriggerValue(value);
        reportChanged = true;
        return;
    }
    if (code == XBOX_AXIS_RIGHT_TRIGGER) {
        report.trigger_r = convertToTriggerValue(value);
        reportChanged = true;
        return;
    }

    if (code >= XBOX_AXIS_LX_MINUS && code <= XBOX_AXIS_RY_PLUS) {
        switch (code) {
            case XBOX_AXIS_LX_MINUS:
                // Left stick X negative: 0 = center, 1000 = full left (-32768)
                report.joystick_lx = -((value * 32768) / 1000);
                reportChanged = true;
                break;
            case XBOX_AXIS_LX_PLUS:
                // Left stick X positive: 0 = center, 1000 = full right (32767)
                report.joystick_lx = (value * 32767) / 1000;
                reportChanged = true;
                break;
            case XBOX_AXIS_LY_MINUS:
                // Left stick Y negative: 0 = center, 1000 = full down (-32768)
                report.joystick_ly = -((value * 32768) / 1000);
                reportChanged = true;
                break;
            case XBOX_AXIS_LY_PLUS:
                // Left stick Y positive: 0 = center, 1000 = full up (32767)
                report.joystick_ly = (value * 32767) / 1000;
                reportChanged = true;
                break;
            case XBOX_AXIS_RX_MINUS:
                // Right stick X negative: 0 = center, 1000 = full left (-32768)
                report.joystick_rx = -((value * 32768) / 1000);
                reportChanged = true;
                break;
            case XBOX_AXIS_RX_PLUS:
                // Right stick X positive: 0 = center, 1000 = full right (32767)
                report.joystick_rx = (value * 32767) / 1000;
                reportChanged = true;
                break;
            case XBOX_AXIS_RY_MINUS:
                // Right stick Y negative: 0 = center, 1000 = full down (-32768)
                report.joystick_ry = -((value * 32768) / 1000);
                reportChanged = true;
                break;
            case XBOX_AXIS_RY_PLUS:
                // Right stick Y positive: 0 = center, 1000 = full up (32767)
                report.joystick_ry = (value * 32767) / 1000;
                reportChanged = true;
                break;
        }
    }
}

void XInputDevice::setOnEvent(std::function<void(int, int)> lambda) {
    eventCallback = lambda;
}

void XInputDevice::update() {
    // Send XInput report only when something changes
    if (tud_xinput_ready(gamepadIndex) && reportChanged) {
        sendReport();
    }
}

AxesDescription XInputDevice::axesDescription() {
    // Build static AxisDescription array (only once)
    static AxisDescription xboxAxes[26];
    static bool initialized = false;

    if (!initialized) {
        // Map all 26 Xbox360 axes (0-25) with their names and indices
        for (int i = 0; i < 26; i++) {
            xboxAxes[i].name = (char*)XBOX360_AXES_NAMES[i];
            xboxAxes[i].axisIndex = i;
        }
        initialized = true;
    }

    AxesDescription desc;
    desc.axes = xboxAxes;
    desc.axesCount = 26;  // 16 buttons + 2 triggers + 8 stick axes
    return desc;
}

void XInputDevice::setButton(uint8_t button, bool pressed) {
    // Xbox 360 button layout (16-bit field):
    // Bit 0: D-pad Up, 1: D-pad Down, 2: D-pad Left, 3: D-pad Right
    // Bit 4: Start, 5: Back, 6: Left Stick, 7: Right Stick
    // Bit 8: LB, 9: RB, 10: Guide, 11: Reserved
    // Bit 12: A, 13: B, 14: X, 15: Y

    uint16_t mask = 1 << button;

    if (pressed) {
        report.buttons |= mask;
    } else {
        report.buttons &= ~mask;
    }

    reportChanged = true;
}

void XInputDevice::sendReport() {
    if (tud_xinput_ready(gamepadIndex)) {
        tud_xinput_report(gamepadIndex, &report);
        reportChanged = false;
    }
}

uint8_t XInputDevice::convertToTriggerValue(int value) {
    // Convert from 0-1000 range to 0-255 range
    if (value < 0) value = 0;
    if (value > 1000) value = 1000;

    return (value * 255) / 1000;
}

int16_t XInputDevice::convertToStickValue(int valueMinus, int valuePlus) {
    // Convert from two 0-1000 ranges (minus and plus) to -32768 to 32767
    if (valueMinus > 0) {
        return -((valueMinus * 32768) / 1000);
    } else if (valuePlus > 0) {
        return (valuePlus * 32767) / 1000;
    }
    return 0;
}

void XInputDevice::handleRumble(const xinput_out_report_t* report) {
    if (eventCallback) {
        // Report rumble values to the callback using proper event codes
        // Left motor (large rumble) - value 0-255
        eventCallback(XBOX_RUMBLE_LEFT, report->rumble_l);
        // Right motor (small rumble) - value 0-255
        eventCallback(XBOX_RUMBLE_RIGHT, report->rumble_r);
    }
}
