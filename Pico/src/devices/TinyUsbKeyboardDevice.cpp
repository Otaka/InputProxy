#include "TinyUsbKeyboardDevice.h"
#include "HidDeviceManager.h"
#include "tusb.h"

// HID Report Descriptor for Keyboard Interface
// Supports: Boot keyboard (6KRO), NKRO keyboard, and Consumer Control
const uint8_t hid_report_descriptor_keyboard[] = {
    // Boot Keyboard Report (Report ID 1) - 6KRO
    HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),
    HID_USAGE(HID_USAGE_DESKTOP_KEYBOARD),
    HID_COLLECTION(HID_COLLECTION_APPLICATION),
        HID_REPORT_ID(REPORT_ID_KEYBOARD_BOOT)
        // Modifier keys (8 bits)
        HID_USAGE_PAGE(HID_USAGE_PAGE_KEYBOARD),
        HID_USAGE_MIN(224),  // Left Ctrl
        HID_USAGE_MAX(231),  // Right GUI
        HID_LOGICAL_MIN(0),
        HID_LOGICAL_MAX(1),
        HID_REPORT_COUNT(8),
        HID_REPORT_SIZE(1),
        HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),

        // Reserved byte
        HID_REPORT_COUNT(1),
        HID_REPORT_SIZE(8),
        HID_INPUT(HID_CONSTANT),

        // LED output report (5 LEDs)
        HID_REPORT_COUNT(5),
        HID_REPORT_SIZE(1),
        HID_USAGE_PAGE(HID_USAGE_PAGE_LED),
        HID_USAGE_MIN(1),  // Num Lock
        HID_USAGE_MAX(5),  // Kana
        HID_OUTPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),

        // LED padding (3 bits)
        HID_REPORT_COUNT(1),
        HID_REPORT_SIZE(3),
        HID_OUTPUT(HID_CONSTANT),

        // Keycodes (6 keys)
        HID_REPORT_COUNT(6),
        HID_REPORT_SIZE(8),
        HID_LOGICAL_MIN(0),
        HID_LOGICAL_MAX(255),
        HID_USAGE_PAGE(HID_USAGE_PAGE_KEYBOARD),
        HID_USAGE_MIN(0),
        HID_USAGE_MAX(255),
        HID_INPUT(HID_DATA | HID_ARRAY | HID_ABSOLUTE),
    HID_COLLECTION_END,

    // NKRO Keyboard Report (Report ID 2) - Bitmap
    HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),
    HID_USAGE(HID_USAGE_DESKTOP_KEYBOARD),
    HID_COLLECTION(HID_COLLECTION_APPLICATION),
        HID_REPORT_ID(REPORT_ID_KEYBOARD_NKRO)
        // Modifier keys (8 bits)
        HID_USAGE_PAGE(HID_USAGE_PAGE_KEYBOARD),
        HID_USAGE_MIN(224),  // Left Ctrl
        HID_USAGE_MAX(231),  // Right GUI
        HID_LOGICAL_MIN(0),
        HID_LOGICAL_MAX(1),
        HID_REPORT_COUNT(8),
        HID_REPORT_SIZE(1),
        HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),

        // All keys bitmap (224 bits = 28 bytes for keycodes 0-223)
        HID_USAGE_PAGE(HID_USAGE_PAGE_KEYBOARD),
        HID_USAGE_MIN(0),
        HID_USAGE_MAX(223),
        HID_LOGICAL_MIN(0),
        HID_LOGICAL_MAX(1),
        HID_REPORT_COUNT(224),
        HID_REPORT_SIZE(1),
        HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),
    HID_COLLECTION_END,

    // Consumer Control Report (Report ID 3) - Multimedia keys (up to 30 simultaneous)
    HID_USAGE_PAGE(HID_USAGE_PAGE_CONSUMER),
    HID_USAGE(HID_USAGE_CONSUMER_CONTROL),
    HID_COLLECTION(HID_COLLECTION_APPLICATION),
        HID_REPORT_ID(REPORT_ID_CONSUMER_CONTROL)
        HID_LOGICAL_MIN(0),
        HID_LOGICAL_MAX_N(0x03FF, 2),
        HID_USAGE_MIN(0),
        HID_USAGE_MAX_N(0x03FF, 2),
        HID_REPORT_COUNT(30),
        HID_REPORT_SIZE(16),
        HID_INPUT(HID_DATA | HID_ARRAY | HID_ABSOLUTE),
    HID_COLLECTION_END,
};

const uint16_t hid_report_descriptor_keyboard_size = sizeof(hid_report_descriptor_keyboard);

TinyUsbKeyboardDevice::TinyUsbKeyboardDevice()
    : keyboardLeds(0)
    , bootReportChanged(false)
    , nkroReportChanged(false)
    , consumerReportChanged(false)
{
    memset(nkroKeymap, 0, sizeof(nkroKeymap));
    memset(&bootReport, 0, sizeof(bootReport));
    memset(consumerKeys, 0, sizeof(consumerKeys));
}

bool TinyUsbKeyboardDevice::init() {
    // Clear all state
    memset(nkroKeymap, 0, sizeof(nkroKeymap));
    memset(&bootReport, 0, sizeof(bootReport));
    memset(consumerKeys, 0, sizeof(consumerKeys));
    keyboardLeds = 0;
    bootReportChanged = false;
    nkroReportChanged = false;
    consumerReportChanged = false;

    return true;
}

void TinyUsbKeyboardDevice::setAxis(int code, int value) {
    // code: HID keycode (1-256) for regular keys (continuous)
    //       257-1280 for consumer control keys (code - 257 + 1 = consumer usage code)
    // value: 0 = released, non-zero = pressed

    if (code < 1) {
        return;
    }

    // Regular keyboard keys (1-256) - convert to 0-based HID keycode
    if (code >= 1 && code <= 256) {
        uint8_t keycode = static_cast<uint8_t>(code - 1);
        if (value > 0) {
            pressKey(keycode);
        } else {
            releaseKey(keycode);
        }
        return;
    }

    // Consumer control keys (257-1280) - map to consumer usage codes 1-1024
    if (code >= 257 && code <= 1280) {
        uint16_t consumer_code = static_cast<uint16_t>(code - 257 + 1);
        if (consumer_code > 0x03FF) {
            return; // Invalid consumer code
        }

        if (value > 0) {
            pressConsumerKey(consumer_code);
        } else {
            releaseConsumerKey(consumer_code);
        }
        return;
    }
}

void TinyUsbKeyboardDevice::setOnEvent(std::function<void(int, int)> lambda) {
    eventCallback = lambda;
}

void TinyUsbKeyboardDevice::pressConsumerKey(uint16_t usage_code) {
    if (usage_code == 0) {
        return; // 0 means no key
    }
    
    // Check if key is already pressed
    for (int i = 0; i < 30; i++) {
        if (consumerKeys[i] == usage_code) {
            return; // Already pressed
        }
    }
    
    // Find empty slot and add key
    for (int i = 0; i < 30; i++) {
        if (consumerKeys[i] == 0) {
            consumerKeys[i] = usage_code;
            consumerReportChanged = true;
            return;
        }
    }
    // All slots full - ignore
}

void TinyUsbKeyboardDevice::releaseConsumerKey(uint16_t usage_code) {
    if (usage_code == 0) {
        // Release all keys if 0 is passed
        bool hadKeys = false;
        for (int i = 0; i < 30; i++) {
            if (consumerKeys[i] != 0) {
                consumerKeys[i] = 0;
                hadKeys = true;
            }
        }
        if (hadKeys) {
            consumerReportChanged = true;
        }
        return;
    }
    
    // Find and release specific key
    for (int i = 0; i < 30; i++) {
        if (consumerKeys[i] == usage_code) {
            consumerKeys[i] = 0;
            consumerReportChanged = true;
            return;
        }
    }
}

void TinyUsbKeyboardDevice::update() {
    // Send HID reports if device is ready and reports have changed
    // NOTE: Use HID instance index 0 (first HID device), not interface number
    if (tud_hid_n_ready(0)) {
        sendReports();
    }
}

void TinyUsbKeyboardDevice::pressKey(uint8_t keycode) {
    if (isKeyPressed(keycode)) {
        return; // Already pressed
    }

    // Set bit in NKRO bitmap
    uint8_t byte_index = keycode / 8;
    uint8_t bit_index = keycode % 8;
    nkroKeymap[byte_index] |= (1 << bit_index);
    nkroReportChanged = true;

    // Update boot report
    updateBootReport();
}

void TinyUsbKeyboardDevice::releaseKey(uint8_t keycode) {
    if (!isKeyPressed(keycode)) {
        return; // Already released
    }

    // Clear bit in NKRO bitmap
    uint8_t byte_index = keycode / 8;
    uint8_t bit_index = keycode % 8;
    nkroKeymap[byte_index] &= ~(1 << bit_index);
    nkroReportChanged = true;

    // Update boot report
    updateBootReport();
}

bool TinyUsbKeyboardDevice::isKeyPressed(uint8_t keycode) {
    uint8_t byte_index = keycode / 8;
    uint8_t bit_index = keycode % 8;
    return (nkroKeymap[byte_index] & (1 << bit_index)) != 0;
}

bool TinyUsbKeyboardDevice::isModifierKey(uint8_t keycode) {
    return keycode >= 224 && keycode <= 231;
}

uint8_t TinyUsbKeyboardDevice::getModifierBit(uint8_t keycode) {
    if (isModifierKey(keycode)) {
        return 1 << (keycode - 224);
    }
    return 0;
}

void TinyUsbKeyboardDevice::updateBootReport() {
    // Update modifiers
    bootReport.modifiers = 0;
    for (uint8_t keycode = 224; keycode <= 231; keycode++) {
        if (isKeyPressed(keycode)) {
            bootReport.modifiers |= getModifierBit(keycode);
        }
    }

    // Update regular keys (max 6)
    uint8_t key_index = 0;
    memset(bootReport.keys, 0, sizeof(bootReport.keys));

    for (uint16_t keycode = 0; keycode < 224 && key_index < 6; keycode++) {
        if (isKeyPressed(keycode)) {
            bootReport.keys[key_index++] = keycode;
        }
    }

    bootReportChanged = true;
}

void TinyUsbKeyboardDevice::sendReports() {
    // NOTE: Use HID instance index 0 (first HID device), not interface number
    // Send boot keyboard report if changed
    if (bootReportChanged && tud_hid_n_ready(0)) {
        tud_hid_n_report(0, REPORT_ID_KEYBOARD_BOOT,
                        &bootReport, sizeof(bootReport));
        bootReportChanged = false;
        return; // Only send one report per update
    }

    // Send NKRO report if changed (optional - boot report is usually sufficient)
    // Uncomment if you need full NKRO support
    /*
    if (nkroReportChanged && tud_hid_n_ready(0)) {
        struct {
            uint8_t report_id;
            uint8_t modifiers;
            uint8_t keymap[28]; // 224 bits for keycodes 0-223
        } nkro_report;

        nkro_report.report_id = REPORT_ID_KEYBOARD_NKRO;
        nkro_report.modifiers = bootReport.modifiers;
        memcpy(nkro_report.keymap, nkroKeymap, 28);

        tud_hid_n_report(0, REPORT_ID_KEYBOARD_NKRO,
                        &nkro_report, sizeof(nkro_report));
        nkroReportChanged = false;
        return;
    }
    */

    // Send consumer control report if changed
    if (consumerReportChanged && tud_hid_n_ready(0)) {
        tud_hid_n_report(0, REPORT_ID_CONSUMER_CONTROL,
                        consumerKeys, sizeof(consumerKeys));
        consumerReportChanged = false;
        return;
    }
}

uint16_t TinyUsbKeyboardDevice::getReportDescriptor(uint8_t* buffer, uint16_t reqlen) {
    uint16_t len = hid_report_descriptor_keyboard_size;
    if (reqlen < len) {
        len = reqlen;
    }
    memcpy(buffer, hid_report_descriptor_keyboard, len);
    return len;
}

uint16_t TinyUsbKeyboardDevice::getReport(uint8_t report_id, hid_report_type_t report_type,
                                          uint8_t* buffer, uint16_t reqlen) {
    if (report_type == HID_REPORT_TYPE_INPUT) {
        if (report_id == REPORT_ID_KEYBOARD_BOOT) {
            if (reqlen >= sizeof(bootReport)) {
                memcpy(buffer, &bootReport, sizeof(bootReport));
                return sizeof(bootReport);
            }
        }
        // Add other report types as needed
    }
    return 0;
}

void TinyUsbKeyboardDevice::setReport(uint8_t report_id, hid_report_type_t report_type,
                                      uint8_t const* buffer, uint16_t bufsize) {
    if (report_type == HID_REPORT_TYPE_OUTPUT) {
        // Keyboard LED output report
        if (bufsize >= 1) {
            uint8_t new_leds = buffer[0];

            // Check if LEDs changed
            if (new_leds != keyboardLeds) {
                // Notify about LED changes
                if (eventCallback) {
                    // Bit 0: Num Lock
                    // Bit 1: Caps Lock
                    // Bit 2: Scroll Lock
                    // Bit 3: Compose
                    // Bit 4: Kana
                    for (int i = 0; i < 5; i++) {
                        bool old_state = (keyboardLeds & (1 << i)) != 0;
                        bool new_state = (new_leds & (1 << i)) != 0;
                        if (old_state != new_state) {
                            eventCallback(i, new_state ? 1 : 0);
                        }
                    }
                }
                keyboardLeds = new_leds;
            }
        }
    }
}



AxesDescription TinyUsbKeyboardDevice::axesDescription() {
    // Build static AxisDescription array (only once)
    static AxisDescription keyboardAxes[258];
    static bool initialized = false;

    if (!initialized) {
        // Map all 258 keyboard axes (0-257) with their names and indices
        for (int i = 0; i < 258; i++) {
            keyboardAxes[i].name = (char*)KEYBOARD_AXES_NAMES[i];
            keyboardAxes[i].axisIndex = i;
        }
        initialized = true;
    }

    AxesDescription desc;
    desc.axes = keyboardAxes;
    desc.axesCount = 258; // 0-257 (index 0-4 unused, 5-257 for HID keycodes and keys)
    return desc;
}

// ==============================================================================
// TinyUsbKeyboardBuilder - Builder pattern for creating keyboard devices
// ==============================================================================

TinyUsbKeyboardBuilder::TinyUsbKeyboardBuilder()
    : m_name("Keyboard")
{
}

TinyUsbKeyboardBuilder& TinyUsbKeyboardBuilder::name(const std::string& deviceName) {
    m_name = deviceName;
    return *this;
}

AbstractVirtualDevice* TinyUsbKeyboardBuilder::build() {
    return new TinyUsbKeyboardDevice();
}

std::string TinyUsbKeyboardBuilder::getName() const {
    return m_name;
}

DeviceType TinyUsbKeyboardBuilder::getDeviceType() const {
    return DeviceType::KEYBOARD;
}
