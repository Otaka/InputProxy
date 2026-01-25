#include "TinyUsbKeyboardDevice.h"
#include "DeviceManager.h"
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

// Static axis names for keyboard (stored in ROM, not RAM)
// We provide names for common keys (1-256 for regular HID keycodes)
// Index 0 is unused (axes start from 1)
static const char* keyboardAxisNames[257] = {
    "",          // 0: unused
    "Key 0",     // 1: HID keycode 0
    "Key 1",     // 2: HID keycode 1
    "Key 2",     // 3: HID keycode 2
    "A",         // 4: HID keycode 3
    "B",         // 5: HID keycode 4
    "C",         // 6: HID keycode 5
    "D",         // 7: HID keycode 6
    "E",         // 8: HID keycode 7
    "F",         // 9: HID keycode 8
    "G",         // 10: HID keycode 9
    "H",         // 11: HID keycode 10
    "I",         // 12: HID keycode 11
    "J",         // 13: HID keycode 12
    "K",         // 14: HID keycode 13
    "L",         // 15: HID keycode 14
    "M",         // 16: HID keycode 15
    "N",         // 17: HID keycode 16
    "O",         // 18: HID keycode 17
    "P",         // 19: HID keycode 18
    "Q",         // 20: HID keycode 19
    "R",         // 21: HID keycode 20
    "S",         // 22: HID keycode 21
    "T",         // 23: HID keycode 22
    "U",         // 24: HID keycode 23
    "V",         // 25: HID keycode 24
    "W",         // 26: HID keycode 25
    "X",         // 27: HID keycode 26
    "Y",         // 28: HID keycode 27
    "Z",         // 29: HID keycode 28
    "1",         // 30: HID keycode 29
    "2",         // 31: HID keycode 30
    "3",         // 32: HID keycode 31
    "4",         // 33: HID keycode 32
    "5",         // 34: HID keycode 33
    "6",         // 35: HID keycode 34
    "7",         // 36: HID keycode 35
    "8",         // 37: HID keycode 36
    "9",         // 38: HID keycode 37
    "0",         // 39: HID keycode 38
    "Enter",     // 40: HID keycode 39
    "Escape",    // 41: HID keycode 40
    "Backspace", // 42: HID keycode 41
    "Tab",       // 43: HID keycode 42
    "Space",     // 44: HID keycode 43
    "Key 44", "Key 45", "Key 46", "Key 47", "Key 48", "Key 49", "Key 50", "Key 51",
    "Key 52", "Key 53", "Key 54", "Key 55", "Key 56", "Key 57", "Key 58", "Key 59",
    "Key 60", "Key 61", "Key 62", "Key 63", "Key 64", "Key 65", "Key 66", "Key 67",
    "Key 68", "Key 69", "Key 70", "Key 71", "Key 72", "Key 73", "Key 74", "Key 75",
    "Key 76", "Key 77", "Key 78", "Key 79", "Key 80", "Key 81", "Key 82", "Key 83",
    "Key 84", "Key 85", "Key 86", "Key 87", "Key 88", "Key 89", "Key 90", "Key 91",
    "Key 92", "Key 93", "Key 94", "Key 95", "Key 96", "Key 97", "Key 98", "Key 99",
    "Key 100", "Key 101", "Key 102", "Key 103", "Key 104", "Key 105", "Key 106", "Key 107",
    "Key 108", "Key 109", "Key 110", "Key 111", "Key 112", "Key 113", "Key 114", "Key 115",
    "Key 116", "Key 117", "Key 118", "Key 119", "Key 120", "Key 121", "Key 122", "Key 123",
    "Key 124", "Key 125", "Key 126", "Key 127", "Key 128", "Key 129", "Key 130", "Key 131",
    "Key 132", "Key 133", "Key 134", "Key 135", "Key 136", "Key 137", "Key 138", "Key 139",
    "Key 140", "Key 141", "Key 142", "Key 143", "Key 144", "Key 145", "Key 146", "Key 147",
    "Key 148", "Key 149", "Key 150", "Key 151", "Key 152", "Key 153", "Key 154", "Key 155",
    "Key 156", "Key 157", "Key 158", "Key 159", "Key 160", "Key 161", "Key 162", "Key 163",
    "Key 164", "Key 165", "Key 166", "Key 167", "Key 168", "Key 169", "Key 170", "Key 171",
    "Key 172", "Key 173", "Key 174", "Key 175", "Key 176", "Key 177", "Key 178", "Key 179",
    "Key 180", "Key 181", "Key 182", "Key 183", "Key 184", "Key 185", "Key 186", "Key 187",
    "Key 188", "Key 189", "Key 190", "Key 191", "Key 192", "Key 193", "Key 194", "Key 195",
    "Key 196", "Key 197", "Key 198", "Key 199", "Key 200", "Key 201", "Key 202", "Key 203",
    "Key 204", "Key 205", "Key 206", "Key 207", "Key 208", "Key 209", "Key 210", "Key 211",
    "Key 212", "Key 213", "Key 214", "Key 215", "Key 216", "Key 217", "Key 218", "Key 219",
    "Key 220", "Key 221", "Key 222", "Key 223", "Key 224", "Key 225", "Key 226", "Key 227",
    "Key 228", "Key 229", "Key 230", "Key 231", "Key 232", "Key 233", "Key 234", "Key 235",
    "Key 236", "Key 237", "Key 238", "Key 239", "Key 240", "Key 241", "Key 242", "Key 243",
    "Key 244", "Key 245", "Key 246", "Key 247", "Key 248", "Key 249", "Key 250", "Key 251",
    "Key 252", "Key 253", "Key 254", "Key 255"
};

AxesDescription TinyUsbKeyboardDevice::axesDescription() {
    AxesDescription desc;
    desc.axisNames = (char**)keyboardAxisNames;
    desc.axesCount = 257; // 0-256 (index 0 unused, 1-256 for HID keycodes 0-255)
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
