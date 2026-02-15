// USB Descriptor Callbacks - Routes to Active Device Manager
// This file contains global TinyUSB callbacks that delegate to the active
// AbstractDeviceManager instance (either HidDeviceManager or XinputDeviceManager)

#include "devices/AbstractDeviceManager.h"
#include "devices/HidDeviceManager.h"
#include "tusb.h"

// Forward declaration: Device manager is initialized in mainPico.cpp
extern "C" {
    AbstractDeviceManager* getDeviceManager();
}

// ==============================================================================
// TinyUSB Descriptor Callbacks (extern "C" linkage required by TinyUSB)
// ==============================================================================

extern "C" {

// Invoked when received GET DEVICE DESCRIPTOR
// Application returns pointer to device descriptor
uint8_t const* tud_descriptor_device_cb(void) {
    AbstractDeviceManager* manager = getDeviceManager();
    if (manager) {
        return manager->getDeviceDescriptor();
    }
    return nullptr;
}

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application returns pointer to configuration descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;  // For multiple configurations (index 0, 1, etc.)

    AbstractDeviceManager* manager = getDeviceManager();
    if (manager) {
        return manager->getConfigurationDescriptor();
    }
    return nullptr;
}

// Invoked when received GET STRING DESCRIPTOR request
// Application returns pointer to descriptor, whose contents must exist long enough for transfer to complete
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    AbstractDeviceManager* manager = getDeviceManager();
    if (manager) {
        return (uint16_t const*)manager->getStringDescriptor(index, langid);
    }
    return nullptr;
}

// Invoked when received GET HID REPORT DESCRIPTOR
// Application returns pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const* tud_hid_descriptor_report_cb(uint8_t itf) {
    AbstractDeviceManager* manager = getDeviceManager();
    if (manager) {
        return manager->getHidReportDescriptor(itf);
    }
    return nullptr;
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                               uint8_t* buffer, uint16_t reqlen) {
    // This callback is HID-specific, only HidDeviceManager needs to handle it
    AbstractDeviceManager* manager = getDeviceManager();
    if (manager && manager->getMode() == DeviceMode::HID_MODE) {
        HidDeviceManager* hidManager = static_cast<HidDeviceManager*>(manager);
        UsbDevice* info = hidManager->getDeviceByInterface(instance);
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
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                           uint8_t const* buffer, uint16_t bufsize) {
    // This callback is HID-specific, only HidDeviceManager needs to handle it
    AbstractDeviceManager* manager = getDeviceManager();
    if (manager && manager->getMode() == DeviceMode::HID_MODE) {
        HidDeviceManager* hidManager = static_cast<HidDeviceManager*>(manager);
        UsbDevice* info = hidManager->getDeviceByInterface(instance);
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
