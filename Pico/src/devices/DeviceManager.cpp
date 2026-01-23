#include "DeviceManager.h"
#include "TinyUsbGamepadDevice.h"
#include "tusb.h"
#include <cstring>
#include <algorithm>

// Global DeviceManager instance for TinyUSB callbacks
static DeviceManager* g_deviceManager = nullptr;

// Global accessor functions
extern "C" {
    DeviceManager* getDeviceManager() {
        return g_deviceManager;
    }

    void setDeviceManager(DeviceManager* manager) {
        g_deviceManager = manager;
    }

    // Check if USB device is mounted and ready
    bool isUsbMounted(void) {
        return tud_mounted();
    }
}

// String indices 0-3 are reserved: 0=Language, 1=Manufacturer, 2=Product, 3=Serial
#define FIRST_INTERFACE_STRING_INDEX 4

DeviceManager::DeviceManager()
    : m_vendorId(0x1209),           // Default: pid.codes (open source VID)
      m_productId(0x0003),          // Default: Custom PID
      m_manufacturer("InputProxy"),
      m_productName("InputProxy Keyboard, Mouse & 4 Gamepads"),
      m_serialNumber("20260118") {
    // Initialize all sockets as empty
    for (int i = 0; i < MAX_DEVICE_SOCKETS; i++) {
        deviceSockets[i].occupied = false;
        deviceSockets[i].device = nullptr;
        deviceSockets[i].interfaceNum = 0;
        deviceSockets[i].endpointNum = 0;
        deviceSockets[i].stringIndex = 0;
        deviceSockets[i].axesCount = 0;
    }
}

DeviceManager& DeviceManager::vendorId(uint16_t vid) {
    m_vendorId = vid;
    return *this;
}

DeviceManager& DeviceManager::productId(uint16_t pid) {
    m_productId = pid;
    return *this;
}

DeviceManager& DeviceManager::manufacturer(const std::string& name) {
    m_manufacturer = name;
    return *this;
}

DeviceManager& DeviceManager::productName(const std::string& name) {
    m_productName = name;
    return *this;
}

DeviceManager& DeviceManager::serialNumber(const std::string& serial) {
    m_serialNumber = serial;
    return *this;
}

DeviceManager::~DeviceManager() {
    // Cleanup all occupied sockets
    for (int i = 0; i < MAX_DEVICE_SOCKETS; i++) {
        if (deviceSockets[i].occupied) {
            cleanupDevice(deviceSockets[i]);
        }
    }
}

void DeviceManager::allocateInterface(UsbDevice& info, uint8_t socketIndex) {
    // Assign interface numbers based on socket index for deterministic allocation
    // Socket 0 → Interface 0, Socket 1 → Interface 1, etc.
    info.interfaceNum = socketIndex;

    // Endpoint numbers: 0x81, 0x82, 0x83, etc. (IN endpoints)
    info.endpointNum = 0x81 + socketIndex;

    // String descriptor indices: start after reserved indices (0-3)
    info.stringIndex = FIRST_INTERFACE_STRING_INDEX + socketIndex;
}

void DeviceManager::cleanupDevice(UsbDevice& info) {
    if (info.device) {
        delete info.device;
        info.device = nullptr;
    }
}

bool DeviceManager::plugDevice(uint8_t socketIndex, AbstractVirtualDevice* device, const std::string& name, DeviceType deviceType, uint8_t axesCount) {
    // Validate socket index
    if (socketIndex >= MAX_DEVICE_SOCKETS) {
        return false;
    }
    
    // Check if socket is already occupied
    if (deviceSockets[socketIndex].occupied) {
        return false;  // Socket is occupied, must unplug first
    }
    
    // Validate device pointer
    if (device == nullptr) {
        return false;
    }
    
    // Fill socket info
    UsbDevice& info = deviceSockets[socketIndex];
    info.device = device;
    info.name = name;
    info.deviceType = deviceType;
    info.axesCount = axesCount;
    info.occupied = true;

    // Allocate USB resources based on socket index
    allocateInterface(info, socketIndex);

    // Regenerate USB descriptors
    generateConfigurationDescriptor();

    return true;
}

bool DeviceManager::unplugDevice(uint8_t socketIndex) {
    // Validate socket index
    if (socketIndex >= MAX_DEVICE_SOCKETS) {
        return false;
    }
    
    // Check if socket is occupied
    if (!deviceSockets[socketIndex].occupied) {
        return false;  // Socket is already empty
    }
    
    // Cleanup device
    cleanupDevice(deviceSockets[socketIndex]);
    deviceSockets[socketIndex].occupied = false;
    
    // Regenerate USB descriptors
    generateConfigurationDescriptor();
    
    // Note: Unplugging a device requires USB re-enumeration
    return true;
}

bool DeviceManager::isSocketOccupied(uint8_t socketIndex) const {
    if (socketIndex >= MAX_DEVICE_SOCKETS) {
        return false;
    }
    return deviceSockets[socketIndex].occupied;
}

AbstractVirtualDevice* DeviceManager::getDevice(uint8_t socketIndex) {
    if (socketIndex >= MAX_DEVICE_SOCKETS || !deviceSockets[socketIndex].occupied) {
        return nullptr;
    }
    return deviceSockets[socketIndex].device;
}

UsbDevice* DeviceManager::getDeviceInfo(uint8_t socketIndex) {
    if (socketIndex >= MAX_DEVICE_SOCKETS || !deviceSockets[socketIndex].occupied) {
        return nullptr;
    }
    return &deviceSockets[socketIndex];
}

UsbDevice* DeviceManager::getDeviceByInterface(uint8_t interfaceNum) {
    for (int i = 0; i < MAX_DEVICE_SOCKETS; i++) {
        if (deviceSockets[i].occupied && deviceSockets[i].interfaceNum == interfaceNum) {
            return &deviceSockets[i];
        }
    }
    return nullptr;
}

void DeviceManager::setAxis(uint8_t socketIndex, int axis, int value) {
    if (socketIndex >= MAX_DEVICE_SOCKETS || !deviceSockets[socketIndex].occupied) {
        return;
    }
    
    if (deviceSockets[socketIndex].device) {
        deviceSockets[socketIndex].device->setAxis(axis, value);
    }
}

size_t DeviceManager::getOccupiedCount() const {
    size_t count = 0;
    for (int i = 0; i < MAX_DEVICE_SOCKETS; i++) {
        if (deviceSockets[i].occupied) {
            count++;
        }
    }
    return count;
}

bool DeviceManager::init() {
    // Initialize all occupied devices
    for (int i = 0; i < MAX_DEVICE_SOCKETS; i++) {
        if (deviceSockets[i].occupied && deviceSockets[i].device) {
            if (!deviceSockets[i].device->init()) {
                return false;
            }
        }
    }
    return true;
}

void DeviceManager::update() {
    // Update all occupied devices
    for (int i = 0; i < MAX_DEVICE_SOCKETS; i++) {
        if (deviceSockets[i].occupied && deviceSockets[i].device) {
            deviceSockets[i].device->update();
        }
    }
}

void DeviceManager::generateConfigurationDescriptor() {
    configDescriptorBuffer.clear();
    
    // Count occupied interfaces
    uint8_t numInterfaces = 0;
    for (int i = 0; i < MAX_DEVICE_SOCKETS; i++) {
        if (deviceSockets[i].occupied) {
            numInterfaces++;
        }
    }
    
    // Calculate total length
    uint16_t totalLength = TUD_CONFIG_DESC_LEN + (numInterfaces * TUD_HID_DESC_LEN);
    
    // Configuration descriptor header
    uint8_t configHeader[] = {
        // Config: bus powered, max 500mA
        9,                                          // bLength
        TUSB_DESC_CONFIGURATION,                   // bDescriptorType
        (uint8_t)(totalLength & 0xFF),             // wTotalLength (low)
        (uint8_t)((totalLength >> 8) & 0xFF),      // wTotalLength (high)
        numInterfaces,                             // bNumInterfaces
        1,                                         // bConfigurationValue
        0,                                         // iConfiguration
        TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP,       // bmAttributes
        250                                        // bMaxPower (500mA)
    };
    
    configDescriptorBuffer.insert(configDescriptorBuffer.end(), configHeader, configHeader + sizeof(configHeader));

    // Collect all occupied devices and sort by interface number (USB spec requires ascending order)
    std::vector<const UsbDevice*> devices;
    for (int i = 0; i < MAX_DEVICE_SOCKETS; i++) {
        if (deviceSockets[i].occupied) {
            devices.push_back(&deviceSockets[i]);
        }
    }
    // Sort by interface number
    std::sort(devices.begin(), devices.end(), [](const UsbDevice* a, const UsbDevice* b) {
        return a->interfaceNum < b->interfaceNum;
    });

    // Helper lambda to add device descriptor
    auto addDeviceDescriptor = [&](const UsbDevice* info) {
        uint8_t hidProtocol = HID_ITF_PROTOCOL_NONE;
        uint16_t reportDescSize = 0;

        if (info->deviceType == DeviceType::KEYBOARD) {
            hidProtocol = HID_ITF_PROTOCOL_KEYBOARD;
            reportDescSize = hid_report_descriptor_keyboard_size;
        } else if (info->deviceType == DeviceType::MOUSE) {
            hidProtocol = HID_ITF_PROTOCOL_MOUSE;
            reportDescSize = hid_report_descriptor_mouse_size;
        } else if (info->deviceType == DeviceType::GAMEPAD) {
            hidProtocol = HID_ITF_PROTOCOL_NONE;
            TinyUsbGamepadDevice* gamepad = static_cast<TinyUsbGamepadDevice*>(info->device);
            reportDescSize = gamepad->getHidDescriptorSize();
        }

        // Build HID descriptor manually (TUD_HID_DESCRIPTOR macro expanded)
        uint8_t hidDesc[] = {
            // Interface descriptor
            9,                                      // bLength
            TUSB_DESC_INTERFACE,                   // bDescriptorType
            info->interfaceNum,                    // bInterfaceNumber
            0,                                     // bAlternateSetting
            1,                                     // bNumEndpoints (IN endpoint only)
            TUSB_CLASS_HID,                        // bInterfaceClass
            (uint8_t)((info->deviceType == DeviceType::KEYBOARD || info->deviceType == DeviceType::MOUSE) ? 1 : 0), // bInterfaceSubClass
            hidProtocol,                           // bInterfaceProtocol
            info->stringIndex,                     // iInterface - string descriptor index for interface name

            // HID descriptor
            9,                                      // bLength
            HID_DESC_TYPE_HID,                     // bDescriptorType
            0x11, 0x01,                            // bcdHID (HID 1.11)
            0,                                     // bCountryCode
            1,                                     // bNumDescriptors
            HID_DESC_TYPE_REPORT,                  // bDescriptorType[0]
            (uint8_t)(reportDescSize & 0xFF),      // wDescriptorLength[0] (low)
            (uint8_t)((reportDescSize >> 8) & 0xFF), // wDescriptorLength[0] (high)

            // Endpoint descriptor
            7,                                      // bLength
            TUSB_DESC_ENDPOINT,                    // bDescriptorType
            info->endpointNum,                     // bEndpointAddress (IN)
            TUSB_XFER_INTERRUPT,                   // bmAttributes
            0x40, 0x00,                            // wMaxPacketSize (64 bytes)
            10                                      // bInterval (10ms)
        };

        configDescriptorBuffer.insert(configDescriptorBuffer.end(), hidDesc, hidDesc + sizeof(hidDesc));
    };

    // Add descriptors in interface number order (already sorted)
    for (const auto* dev : devices) {
        addDeviceDescriptor(dev);
    }
}

const uint8_t* DeviceManager::getConfigurationDescriptor(uint16_t* length) {
    if (length) {
        *length = configDescriptorBuffer.size();
    }
    return configDescriptorBuffer.data();
}

const uint8_t* DeviceManager::getDeviceReportDescriptor(uint8_t interfaceNum, uint16_t* length) {
    UsbDevice* info = getDeviceByInterface(interfaceNum);
    if (!info) {
        if (length) *length = 0;
        return nullptr;
    }
    
    // Return the appropriate report descriptor
    if (info->deviceType == DeviceType::KEYBOARD) {
        if (length) *length = hid_report_descriptor_keyboard_size;
        return hid_report_descriptor_keyboard;
    } else if (info->deviceType == DeviceType::MOUSE) {
        if (length) *length = hid_report_descriptor_mouse_size;
        return hid_report_descriptor_mouse;
    } else if (info->deviceType == DeviceType::GAMEPAD) {
        // Get descriptor directly from gamepad instance (each has its own buffer)
        TinyUsbGamepadDevice* gamepad = static_cast<TinyUsbGamepadDevice*>(info->device);
        if (gamepad) {
            if (length) *length = gamepad->getHidDescriptorSize();
            return gamepad->getHidDescriptor();
        }
    }
    
    if (length) *length = 0;
    return nullptr;
}

const char* DeviceManager::getInterfaceString(uint8_t stringIndex) const {
    // String indices 0-3 are reserved, interface strings start at FIRST_INTERFACE_STRING_INDEX
    if (stringIndex < FIRST_INTERFACE_STRING_INDEX) {
        return nullptr;
    }

    // Find device with matching string index
    for (int i = 0; i < MAX_DEVICE_SOCKETS; i++) {
        if (deviceSockets[i].occupied && deviceSockets[i].stringIndex == stringIndex) {
            return deviceSockets[i].name.c_str();
        }
    }
    return nullptr;
}

// ==============================================================================
// TinyUSB Callback Functions (extern "C" for TinyUSB)
// ==============================================================================

extern "C" {

// String Descriptors
enum {
    STRID_LANGID = 0,
    STRID_MANUFACTURER,
    STRID_PRODUCT,
    STRID_SERIAL,
};

// Dynamic Device Descriptor - updated with DeviceManager values
static tusb_desc_device_t desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,  // USB 2.0
    .bDeviceClass       = 0x00,    // Use class information from interface descriptors
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0x1209,  // Default: pid.codes (open source VID)
    .idProduct          = 0x0003,  // Default: Custom PID
    .bcdDevice          = 0x0200,  // Device version 2.0
    .iManufacturer      = STRID_MANUFACTURER,
    .iProduct           = STRID_PRODUCT,
    .iSerialNumber      = STRID_SERIAL,
    .bNumConfigurations = 0x01
};

// Invoked when received GET DEVICE DESCRIPTOR
uint8_t const * tud_descriptor_device_cb(void) {
    // Update device descriptor with DeviceManager values
    DeviceManager* manager = getDeviceManager();
    if (manager) {
        desc_device.idVendor = manager->getVendorId();
        desc_device.idProduct = manager->getProductId();
    }
    return (uint8_t const *) &desc_device;
}

// Invoked when received GET CONFIGURATION DESCRIPTOR
uint8_t const * tud_descriptor_configuration_cb(uint8_t index) {
    (void) index;
    
    // Use DeviceManager's dynamically generated descriptor
    DeviceManager* manager = getDeviceManager();
    if (manager) {
        uint16_t length;
        const uint8_t* desc = manager->getConfigurationDescriptor(&length);
        if (desc && length > 0) {
            return desc;
        }
    }
    
    return nullptr;
}

// Language descriptor (index 0)
static const char language_desc[] = { 0x09, 0x04 };  // English (US)

static uint16_t _desc_str[64];

// Invoked when received GET STRING DESCRIPTOR request
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void) langid;

    uint8_t chr_count;
    const char* str = NULL;

    if (index == 0) {
        memcpy(&_desc_str[1], language_desc, 2);
        chr_count = 1;
    } else if (index == STRID_MANUFACTURER) {
        // Manufacturer string from DeviceManager
        DeviceManager* manager = getDeviceManager();
        str = manager ? manager->getManufacturer().c_str() : "InputProxy";
    } else if (index == STRID_PRODUCT) {
        // Product string from DeviceManager
        DeviceManager* manager = getDeviceManager();
        str = manager ? manager->getProductName().c_str() : "InputProxy Device";
    } else if (index == STRID_SERIAL) {
        // Serial string from DeviceManager
        DeviceManager* manager = getDeviceManager();
        str = manager ? manager->getSerialNumber().c_str() : "000000";
    } else {
        // Dynamic interface string descriptors
        DeviceManager* manager = getDeviceManager();
        if (manager) {
            str = manager->getInterfaceString(index);
        }
    }

    if (index != 0) {
        if (str == NULL) {
            return NULL;
        }

        // Convert ASCII string to UTF-16
        chr_count = strlen(str);
        if (chr_count > 63) chr_count = 63;

        for (uint8_t i = 0; i < chr_count; i++) {
            _desc_str[1 + i] = str[i];
        }
    }

    // First byte is length (including header), second byte is string type
    _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * chr_count + 2);

    return _desc_str;
}

// Invoked when received GET HID REPORT DESCRIPTOR
uint8_t const * tud_hid_descriptor_report_cb(uint8_t itf) {
    DeviceManager* manager = getDeviceManager();
    if (manager) {
        uint16_t length;
        const uint8_t* desc = manager->getDeviceReportDescriptor(itf, &length);
        if (desc && length > 0) {
            return desc;
        }
    }
    return NULL;
}

// Invoked when received GET_REPORT control request
uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type,
                               uint8_t* buffer, uint16_t reqlen) {
    DeviceManager* manager = getDeviceManager();
    if (manager) {
        UsbDevice* info = manager->getDeviceByInterface(itf);
        if (info && info->device) {
            if (info->deviceType == DeviceType::KEYBOARD) {
                TinyUsbKeyboardDevice* keyboard = static_cast<TinyUsbKeyboardDevice*>(info->device);
                return keyboard->getReport(report_id, report_type, buffer, reqlen);
            } else if (info->deviceType == DeviceType::MOUSE) {
                TinyUsbMouseDevice* mouse = static_cast<TinyUsbMouseDevice*>(info->device);
                return mouse->getReport(report_id, report_type, buffer, reqlen);
            } else if (info->deviceType == DeviceType::GAMEPAD) {
                TinyUsbGamepadDevice* gamepad = static_cast<TinyUsbGamepadDevice*>(info->device);
                return gamepad->getReport(report_id, report_type, buffer, reqlen);
            }
        }
    }
    return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint (Report ID = 0, Type = 0)
void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type,
                           uint8_t const* buffer, uint16_t bufsize) {
    DeviceManager* manager = getDeviceManager();
    if (manager) {
        UsbDevice* info = manager->getDeviceByInterface(itf);
        if (info && info->device) {
            if (info->deviceType == DeviceType::KEYBOARD) {
                TinyUsbKeyboardDevice* keyboard = static_cast<TinyUsbKeyboardDevice*>(info->device);
                keyboard->setReport(report_id, report_type, buffer, bufsize);
            } else if (info->deviceType == DeviceType::MOUSE) {
                TinyUsbMouseDevice* mouse = static_cast<TinyUsbMouseDevice*>(info->device);
                mouse->setReport(report_id, report_type, buffer, bufsize);
            } else if (info->deviceType == DeviceType::GAMEPAD) {
                TinyUsbGamepadDevice* gamepad = static_cast<TinyUsbGamepadDevice*>(info->device);
                gamepad->setReport(report_id, report_type, buffer, bufsize);
            }
        }
    }
}

} // extern "C"
