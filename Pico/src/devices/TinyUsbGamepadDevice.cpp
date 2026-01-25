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
    , continuousAxisMap(nullptr)
    , continuousAxisCount(0)
    , axesDescriptionInitialized(false)
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

    // Build continuous axis mapping and description buffer
    buildAxisMapping();
    buildAxesDescriptionBuffer();
}

TinyUsbGamepadDevice::~TinyUsbGamepadDevice() {
    if (hidDescriptor) {
        free(hidDescriptor);
        hidDescriptor = nullptr;
    }
    if (continuousAxisMap) {
        free(continuousAxisMap);
        continuousAxisMap = nullptr;
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

void TinyUsbGamepadDevice::buildAxisMapping() {
    // Count total controls: buttons + (axes * 2 for +/- directions) + hat directions
    uint8_t numEnabledAxes = getNumAxes();
    int totalControls = numButtons;  // Start with buttons

    // Add axes (each axis has 2 directions: minus and plus)
    totalControls += numEnabledAxes * 2;

    // Add hat directions if enabled
    if (hasHat) {
        totalControls += 4;  // Up, Down, Left, Right
    }

    continuousAxisCount = totalControls + 1;  // +1 for index 0 (unused)

    // Allocate mapping array
    continuousAxisMap = (AxisMapping*)malloc(continuousAxisCount * sizeof(AxisMapping));
    if (!continuousAxisMap) {
        continuousAxisCount = 0;
        return;
    }

    int currentIndex = 1;  // Start from 1 (0 is unused)

    // Map buttons (continuous index 1..numButtons -> button codes 0..numButtons-1)
    for (uint8_t i = 0; i < numButtons; i++) {
        continuousAxisMap[currentIndex].code = i;  // Button code is just the button index
        currentIndex++;
    }

    // Map axes (for each enabled axis, create minus and plus entries)
    for (int axisIdx = 0; axisIdx < 8; axisIdx++) {
        if (axesBitMask & (1 << axisIdx)) {
            // Axis MINUS direction (e.g., GAMEPAD_AXIS_LX_MINUS)
            continuousAxisMap[currentIndex].code = GAMEPAD_AXIS_LX_MINUS + (axisIdx * 2);
            currentIndex++;

            // Axis PLUS direction (e.g., GAMEPAD_AXIS_LX_PLUS)
            continuousAxisMap[currentIndex].code = GAMEPAD_AXIS_LX_PLUS + (axisIdx * 2);
            currentIndex++;
        }
    }

    // Map hat directions if enabled
    if (hasHat) {
        continuousAxisMap[currentIndex++].code = GAMEPAD_HAT_UP;
        continuousAxisMap[currentIndex++].code = GAMEPAD_HAT_DOWN;
        continuousAxisMap[currentIndex++].code = GAMEPAD_HAT_LEFT;
        continuousAxisMap[currentIndex++].code = GAMEPAD_HAT_RIGHT;
    }
}

// Static button name strings (stored in ROM, not RAM)
static const char* gamepadButtonNames[32] = {
    "Button 1",  "Button 2",  "Button 3",  "Button 4",
    "Button 5",  "Button 6",  "Button 7",  "Button 8",
    "Button 9",  "Button 10", "Button 11", "Button 12",
    "Button 13", "Button 14", "Button 15", "Button 16",
    "Button 17", "Button 18", "Button 19", "Button 20",
    "Button 21", "Button 22", "Button 23", "Button 24",
    "Button 25", "Button 26", "Button 27", "Button 28",
    "Button 29", "Button 30", "Button 31", "Button 32"
};

// Static axis direction name strings (stored in ROM, not RAM)
static const char* gamepadAxisMinusNames[8] = {
    "Stick LX-", "Stick LY-", "Stick LZ-", "Stick RX-",
    "Stick RY-", "Stick RZ-", "Dial-", "Slider-"
};

static const char* gamepadAxisPlusNames[8] = {
    "Stick LX+", "Stick LY+", "Stick LZ+", "Stick RX+",
    "Stick RY+", "Stick RZ+", "Dial+", "Slider+"
};

// Static hat direction name strings (stored in ROM, not RAM)
static const char* gamepadHatNames[4] = {
    "Hat Up", "Hat Down", "Hat Left", "Hat Right"
};

void TinyUsbGamepadDevice::buildAxesDescriptionBuffer() {
    // Clear the buffer
    memset(axesDescriptionBuffer, 0, sizeof(axesDescriptionBuffer));

    if (continuousAxisCount == 0 || !continuousAxisMap) {
        return;
    }

    // Layout of buffer:
    // - First part: AxesDescription struct (char** pointer + uint count)
    // - Second part: Array of char* pointers (one per axis)
    // No need for string data part - we use static ROM strings

    AxesDescription* desc = (AxesDescription*)axesDescriptionBuffer;
    char** namePointers = (char**)(axesDescriptionBuffer + sizeof(AxesDescription));

    desc->axisNames = namePointers;
    desc->axesCount = continuousAxisCount;

    // Empty name for index 0 (unused)
    static const char* emptyString = "";
    namePointers[0] = (char*)emptyString;

    // Point to static ROM strings for each continuous index
    for (int i = 1; i < continuousAxisCount; i++) {
        int code = continuousAxisMap[i].code;

        // Buttons (0-31)
        if (code < GAMEPAD_MAX_BUTTONS) {
            namePointers[i] = (char*)gamepadButtonNames[code];
        }
        // Axis MINUS codes (110-124)
        else if (code >= GAMEPAD_AXIS_LX_MINUS && code <= GAMEPAD_AXIS_SLIDER_MINUS) {
            uint8_t axisIdx = (code - GAMEPAD_AXIS_LX_MINUS) / 2;
            if (axisIdx < 8) {
                namePointers[i] = (char*)gamepadAxisMinusNames[axisIdx];
            } else {
                namePointers[i] = (char*)emptyString;
            }
        }
        // Axis PLUS codes (111-125)
        else if (code >= GAMEPAD_AXIS_LX_PLUS && code <= GAMEPAD_AXIS_SLIDER_PLUS) {
            uint8_t axisIdx = (code - GAMEPAD_AXIS_LX_PLUS) / 2;
            if (axisIdx < 8) {
                namePointers[i] = (char*)gamepadAxisPlusNames[axisIdx];
            } else {
                namePointers[i] = (char*)emptyString;
            }
        }
        // Hat directions (200-203)
        else if (code >= GAMEPAD_HAT_UP && code <= GAMEPAD_HAT_RIGHT) {
            int hatIdx = code - GAMEPAD_HAT_UP;
            if (hatIdx >= 0 && hatIdx < 4) {
                namePointers[i] = (char*)gamepadHatNames[hatIdx];
            } else {
                namePointers[i] = (char*)emptyString;
            }
        }
        else {
            namePointers[i] = (char*)emptyString;
        }
    }

    axesDescriptionInitialized = true;
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
    if (code < 1 || !continuousAxisMap) return;

    // Check if this is a valid continuous axis index (1 to continuousAxisCount-1)
    if (code >= continuousAxisCount) return;

    // Map continuous index to actual control code
    int actualCode = continuousAxisMap[code].code;

    // Buttons (codes 0-31)
    if (actualCode < GAMEPAD_MAX_BUTTONS) {
        if (value > 0) {
            pressButton(static_cast<uint8_t>(actualCode));
        } else {
            releaseButton(static_cast<uint8_t>(actualCode));
        }
        return;
    }

    // Axes MINUS codes (110-124) - Negative direction (0-127)
    if (actualCode >= GAMEPAD_AXIS_LX_MINUS && actualCode <= GAMEPAD_AXIS_SLIDER_MINUS) {
        // Determine which base axis this corresponds to
        uint8_t baseAxisIndex = (actualCode - GAMEPAD_AXIS_LX_MINUS) / 2;
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

    // Axes PLUS codes (111-125) - Positive direction (127-255)
    if (actualCode >= GAMEPAD_AXIS_LX_PLUS && actualCode <= GAMEPAD_AXIS_SLIDER_PLUS) {
        // Determine which base axis this corresponds to
        uint8_t baseAxisIndex = (actualCode - GAMEPAD_AXIS_LX_PLUS) / 2;
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
    if (actualCode >= GAMEPAD_HAT_UP && actualCode <= GAMEPAD_HAT_RIGHT) {
        bool pressed = (value > 0);
        switch (actualCode) {
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

AxesDescription TinyUsbGamepadDevice::axesDescription() {
    if (!axesDescriptionInitialized) {
        // Return empty description if not initialized
        AxesDescription empty;
        empty.axisNames = nullptr;
        empty.axesCount = 0;
        return empty;
    }

    // Return the description from the buffer
    AxesDescription* desc = (AxesDescription*)axesDescriptionBuffer;
    return *desc;
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
