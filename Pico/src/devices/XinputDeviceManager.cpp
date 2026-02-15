#include "XinputDeviceManager.h"
#include "xinput_descriptors.h"
#include "tusb.h"
#include <cstring>

// String indices for USB descriptors
#define STRID_LANGID 0
#define STRID_MANUFACTURER 1
#define STRID_PRODUCT 2
#define STRID_SERIAL 3
#define STRID_INTERFACE_XINPUT 4

// Interface numbers for the 4 XInput gamepads
#define ITF_NUM_XINPUT_0 0
#define ITF_NUM_XINPUT_1 1
#define ITF_NUM_XINPUT_2 2
#define ITF_NUM_XINPUT_3 3
#define ITF_NUM_TOTAL 4

// Endpoint allocations (each gamepad gets IN and OUT endpoints)
#define EPNUM_XINPUT_0_IN  0x81
#define EPNUM_XINPUT_0_OUT 0x01
#define EPNUM_XINPUT_1_IN  0x82
#define EPNUM_XINPUT_1_OUT 0x02
#define EPNUM_XINPUT_2_IN  0x83
#define EPNUM_XINPUT_2_OUT 0x03
#define EPNUM_XINPUT_3_IN  0x84
#define EPNUM_XINPUT_3_OUT 0x04

XinputDeviceManager::XinputDeviceManager()
    : m_vendorId(0x045E),           // Microsoft VID for Xbox 360 compatibility
      m_productId(0x028E),          // Xbox 360 Controller PID
      m_manufacturer("Microsoft"),
      m_productName("Xbox 360 Controller"),
      m_serialNumber("000000") {
    // Initialize all sockets as empty
    for (int i = 0; i < MAX_XINPUT_SOCKETS; i++) {
        sockets[i].occupied = false;
        sockets[i].device = nullptr;
    }
}

XinputDeviceManager::~XinputDeviceManager() {
    // Cleanup all occupied sockets
    for (int i = 0; i < MAX_XINPUT_SOCKETS; i++) {
        if (sockets[i].occupied) {
            cleanupDevice(sockets[i]);
        }
    }
}

AbstractDeviceManager* XinputDeviceManager::vendorId(uint16_t vid) {
    m_vendorId = vid;
    return this;
}

AbstractDeviceManager* XinputDeviceManager::productId(uint16_t pid) {
    m_productId = pid;
    return this;
}

AbstractDeviceManager* XinputDeviceManager::manufacturer(const std::string& name) {
    m_manufacturer = name;
    return this;
}

AbstractDeviceManager* XinputDeviceManager::productName(const std::string& name) {
    m_productName = name;
    return this;
}

AbstractDeviceManager* XinputDeviceManager::serialNumber(const std::string& serial) {
    m_serialNumber = serial;
    return this;
}

bool XinputDeviceManager::plugDevice(uint8_t socketIndex, XInputDevice* device, const std::string& name) {
    if (socketIndex >= MAX_XINPUT_SOCKETS) {
        return false;
    }

    if (sockets[socketIndex].occupied) {
        return false;  // Socket already occupied
    }

    sockets[socketIndex].device = device;
    sockets[socketIndex].name = name;
    sockets[socketIndex].occupied = true;

    return true;
}

bool XinputDeviceManager::unplugDevice(uint8_t socketIndex) {
    if (socketIndex >= MAX_XINPUT_SOCKETS) {
        return false;
    }

    if (!sockets[socketIndex].occupied) {
        return false;  // Socket already empty
    }

    cleanupDevice(sockets[socketIndex]);
    sockets[socketIndex].occupied = false;

    return true;
}

bool XinputDeviceManager::isSocketOccupied(uint8_t socketIndex) const {
    if (socketIndex >= MAX_XINPUT_SOCKETS) {
        return false;
    }
    return sockets[socketIndex].occupied;
}

void XinputDeviceManager::cleanupDevice(XInputSocket& socket) {
    if (socket.device) {
        delete socket.device;
        socket.device = nullptr;
    }
}

void XinputDeviceManager::setAxis(int socketIndex, int axis, int value) {
    if (socketIndex < 0 || socketIndex >= MAX_XINPUT_SOCKETS) {
        return;
    }

    if (!sockets[socketIndex].occupied || !sockets[socketIndex].device) {
        return;
    }

    sockets[socketIndex].device->setAxis(axis, value);
}

AbstractVirtualDevice* XinputDeviceManager::getDevice(int socketIndex) {
    if (socketIndex < 0 || socketIndex >= MAX_XINPUT_SOCKETS) {
        return nullptr;
    }

    if (!sockets[socketIndex].occupied) {
        return nullptr;
    }

    return sockets[socketIndex].device;
}

size_t XinputDeviceManager::getOccupiedCount() const {
    size_t count = 0;
    for (int i = 0; i < MAX_XINPUT_SOCKETS; i++) {
        if (sockets[i].occupied) {
            count++;
        }
    }
    return count;
}

bool XinputDeviceManager::init() {
    // Initialize TinyUSB
    tusb_init();

    // Initialize all occupied devices
    for (int i = 0; i < MAX_XINPUT_SOCKETS; i++) {
        if (sockets[i].occupied && sockets[i].device) {
            sockets[i].device->init();
        }
    }

    // Generate configuration descriptor
    generateConfigurationDescriptor();

    return true;
}

void XinputDeviceManager::update() {
    // Update all occupied devices
    for (int i = 0; i < MAX_XINPUT_SOCKETS; i++) {
        if (sockets[i].occupied && sockets[i].device) {
            sockets[i].device->update();
        }
    }
}

void XinputDeviceManager::generateConfigurationDescriptor() {
    // Calculate total length: Config descriptor + 4 XInput interface descriptors
    uint16_t totalLength = TUD_CONFIG_DESC_LEN + (TUD_XINPUT_DESC_LEN * 4);

    configDescriptorBuffer.clear();
    configDescriptorBuffer.reserve(totalLength);

    // Configuration descriptor
    uint8_t configDesc[] = {
        // Config number, interface count, string index, total length, attribute, power in mA
        TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, totalLength,
                             TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 250)
    };
    configDescriptorBuffer.insert(configDescriptorBuffer.end(), configDesc, configDesc + sizeof(configDesc));

    // Interface 0: XInput Gamepad 0
    uint8_t xinput0Desc[] = {
        TUD_XINPUT_DESCRIPTOR(ITF_NUM_XINPUT_0, STRID_INTERFACE_XINPUT,
                             EPNUM_XINPUT_0_IN, EPNUM_XINPUT_0_OUT)
    };
    configDescriptorBuffer.insert(configDescriptorBuffer.end(), xinput0Desc, xinput0Desc + sizeof(xinput0Desc));

    // Interface 1: XInput Gamepad 1
    uint8_t xinput1Desc[] = {
        TUD_XINPUT_DESCRIPTOR(ITF_NUM_XINPUT_1, STRID_INTERFACE_XINPUT,
                             EPNUM_XINPUT_1_IN, EPNUM_XINPUT_1_OUT)
    };
    configDescriptorBuffer.insert(configDescriptorBuffer.end(), xinput1Desc, xinput1Desc + sizeof(xinput1Desc));

    // Interface 2: XInput Gamepad 2
    uint8_t xinput2Desc[] = {
        TUD_XINPUT_DESCRIPTOR(ITF_NUM_XINPUT_2, STRID_INTERFACE_XINPUT,
                             EPNUM_XINPUT_2_IN, EPNUM_XINPUT_2_OUT)
    };
    configDescriptorBuffer.insert(configDescriptorBuffer.end(), xinput2Desc, xinput2Desc + sizeof(xinput2Desc));

    // Interface 3: XInput Gamepad 3
    uint8_t xinput3Desc[] = {
        TUD_XINPUT_DESCRIPTOR(ITF_NUM_XINPUT_3, STRID_INTERFACE_XINPUT,
                             EPNUM_XINPUT_3_IN, EPNUM_XINPUT_3_OUT)
    };
    configDescriptorBuffer.insert(configDescriptorBuffer.end(), xinput3Desc, xinput3Desc + sizeof(xinput3Desc));
}

// ===== USB Descriptor Callbacks =====

uint8_t const* XinputDeviceManager::getDeviceDescriptor() {
    // Device descriptor for XInput mode (gamepads only)
    // Using vendor-specific class for pure XInput device
    static tusb_desc_device_t desc_device = {
        .bLength            = sizeof(tusb_desc_device_t),
        .bDescriptorType    = TUSB_DESC_DEVICE,
        .bcdUSB             = 0x0200,  // USB 2.0
        .bDeviceClass       = 0xFF,    // Vendor-specific (for pure XInput)
        .bDeviceSubClass    = 0xFF,
        .bDeviceProtocol    = 0xFF,
        .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
        .idVendor           = 0x045E,  // Microsoft (updated dynamically)
        .idProduct          = 0x028E,  // Xbox 360 Controller (updated dynamically)
        .bcdDevice          = 0x0114,  // Device version 1.14
        .iManufacturer      = STRID_MANUFACTURER,
        .iProduct           = STRID_PRODUCT,
        .iSerialNumber      = STRID_SERIAL,
        .bNumConfigurations = 0x01
    };

    // Update with configured VID/PID
    desc_device.idVendor = m_vendorId;
    desc_device.idProduct = m_productId;

    return (uint8_t const*)&desc_device;
}

uint8_t const* XinputDeviceManager::getConfigurationDescriptor() {
    return configDescriptorBuffer.data();
}

uint8_t const* XinputDeviceManager::getHidReportDescriptor(uint8_t itf) {
    // XInput doesn't use HID report descriptors (vendor-specific class)
    (void)itf;
    return nullptr;
}

uint16_t XinputDeviceManager::getHidReportDescriptorLength(uint8_t itf) {
    // XInput doesn't use HID report descriptors
    (void)itf;
    return 0;
}

char const* XinputDeviceManager::getStringDescriptor(uint8_t index, uint16_t langid) {
    (void)langid;

    static uint16_t _desc_str[32];
    uint8_t chr_count;

    const char* str = nullptr;

    switch (index) {
        case STRID_LANGID:
            // Language ID
            memcpy(&_desc_str[1], "\x09\x04", 2);  // English (US)
            chr_count = 1;
            _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * chr_count + 2);
            return (char const*)_desc_str;

        case STRID_MANUFACTURER:
            str = m_manufacturer.c_str();
            break;

        case STRID_PRODUCT:
            str = m_productName.c_str();
            break;

        case STRID_SERIAL:
            str = m_serialNumber.c_str();
            break;

        case STRID_INTERFACE_XINPUT:
            str = "XInput Gamepad Interface";
            break;

        default:
            return nullptr;
    }

    if (str == nullptr) {
        return nullptr;
    }

    // Convert ASCII string to UTF-16
    chr_count = strlen(str);
    if (chr_count > 31) chr_count = 31;

    for (uint8_t i = 0; i < chr_count; i++) {
        _desc_str[1 + i] = str[i];
    }

    // First byte is length (including header), second byte is string type
    _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * chr_count + 2);

    return (char const*)_desc_str;
}

// ===== Global rumble callback for TinyUSB driver =====

// This callback is called by tud_driver_xinput when rumble data is received
extern "C" {
void tud_xinput_receive_report_cb(uint8_t gamepad_index, xinput_out_report_t const* report) {
    // Forward rumble events to the appropriate device
    XinputDeviceManager* manager = static_cast<XinputDeviceManager*>(getDeviceManager());
    if (manager) {
        XInputDevice* device = static_cast<XInputDevice*>(manager->getDevice(gamepad_index));
        if (device) {
            device->handleRumble(report);
        }
    }
}
}
