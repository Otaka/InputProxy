#define RPCMANAGER_STD_STRING

#include <string.h>
#include <cstdlib>
#include <ctime>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/bootrom.h"
#include "pico/time.h"
#include "pico/mutex.h"
#include "tusb.h"

#include "UartManagerPico.h"
#include "../shared/simplerpc/simplerpc.h"
#include "../shared/rpcinterface.h"
#include "../shared/shared.h"
#include "PersistentStorage.h"
#include "devices/AbstractDeviceManager.h"
#include "devices/HidDeviceManager.h"
#include "devices/XinputDeviceManager.h"
#include "devices/TinyUsbKeyboardDevice.h"
#include "devices/TinyUsbMouseDevice.h"
#include "devices/TinyUsbGamepadDevice.h"
#include "devices/XInputDevice.h"
#include "Timer.h"
#include "hardware/watchdog.h"

using namespace simplerpc;

static UartManagerPico* uartManager = nullptr;

// RPC Manager for UART communication with Mainboard
static RpcManager<>* uartRpcManager = nullptr;
static Pico2Main pico2MainRpcClient;
static Main2Pico main2PicoRpcServer;

static bool ledState = false;

// Device Manager - manages devices based on current mode (HID or XInput)
static AbstractDeviceManager* deviceManager = nullptr;

// Persistent storage for configuration (mode, etc.)
static PersistentStorage persistentStorage;

// Flag to trigger reboot after mode change
static volatile bool requiresReboot = false;

void initPicoLed() {
    stdio_init_all();
    cyw43_arch_init();
}

void enableDefaultLed(bool enable) {
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, enable);
    ledState = enable;
}

void toggleDefaultLed() {
    ledState = !ledState;
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, ledState);
}

bool checkDefaultLed() {
    return ledState;
}

void rebootToBootsel() {
    sleep_ms(100);
    reset_usb_boot(0, 0);
}

void usbAwareSleep(uint timeMs) {
    // Sleep while processing USB tasks to keep the USB stack alive
    const uint chunkMs = 5;  // Process USB every 5ms
    uint chunks = timeMs / chunkMs;
    uint remainder = timeMs % chunkMs;

    for (uint i = 0; i < chunks; i++) {
        tud_task();
        sleep_ms(chunkMs);
    }

    if (remainder > 0) {
        tud_task();
        sleep_ms(remainder);
    }
}

std::string generateRandomDeviceId() {
    const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    const int charsetSize = sizeof(charset) - 1;
    std::string result;
    result.reserve(5);

    for (int i = 0; i < 5; i++) {
        result += charset[rand() % charsetSize];
    }

    return result;
}

void handleUartMessage(const char* data, size_t length) {
    // Forward raw UART data to UART RPC manager (input filter handles deframing)
    if (data && length > 0 && uartRpcManager) {
        uartRpcManager->processInput(data, length);
    }
}

/*void initializeEmptyDeviceManager() {
    // Get mode from storage (default to HID_MODE)
    std::string modeStr = persistentStorage.get("mode");
    DeviceMode currentMode = (modeStr == "xinput") ? DeviceMode::XINPUT_MODE : DeviceMode::HID_MODE;

    if (currentMode == DeviceMode::XINPUT_MODE) {
        // XInput mode: Create empty XInput manager
        XinputDeviceManager* xinputManager = new XinputDeviceManager();
        xinputManager->vendorId(0x045E)      // Microsoft VID
                     ->productId(0x028E)      // Xbox 360 Controller PID
                     ->manufacturer("Microsoft")
                     ->productName("Xbox 360 Controller")
                     ->serialNumber("000000");

        deviceManager = xinputManager;
    } else {
        // HID mode: Create empty HID manager
        HidDeviceManager* hidManager = new HidDeviceManager();
        hidManager->vendorId(0x1209)           // pid.codes (open source VID)
                 ->productId(0x0003)          // Custom PID
                 ->manufacturer("InputProxy")
                 ->productName("InputProxy Composite Device")
                 ->serialNumber("20260118");

        deviceManager = hidManager;
    }

    // Initialize the device manager (without any devices plugged)
    setDeviceManager(deviceManager);
    deviceManager->init();
}*/

bool initUartRpcSystem() {
    // Initialize UART RPC Manager for Mainboard communication
    uartRpcManager = new RpcManager<>();
    uartRpcManager->addInputFilter(new StreamFramerInputFilter());
    uartRpcManager->addOutputFilter(new StreamFramerOutputFilter());
    uartRpcManager->setDefaultTimeout(5000);

    // UART sends via UartManager
    uartRpcManager->setOnSendCallback([](const char* data, int len) {
        if (data && len > 0 && uartManager) {
            uartManager->sendData(data, len);
        }
    });

    main2PicoRpcServer.ping = [](int val) -> int {
        return val;
    };

    main2PicoRpcServer.setLed = [](bool state) {
        enableDefaultLed(state);
    };

    main2PicoRpcServer.setAxis = [](int device, int axis, int value) {
        if (!deviceManager) return;
        toggleDefaultLed();
        deviceManager->setAxis(device, axis, value);
    };

    main2PicoRpcServer.getLedStatus = []() -> bool {
        return checkDefaultLed();
    };

    main2PicoRpcServer.rebootFlashMode = []() -> bool {
        rebootToBootsel();
        return true;
    };

    main2PicoRpcServer.reboot = []() {
        requiresReboot = true;
    };

    main2PicoRpcServer.setMode = [](uint8_t mode) {
        DeviceMode newMode = static_cast<DeviceMode>(mode);

        // Check if device manager already exists
        if (deviceManager) {
            DeviceMode currentMode = deviceManager->getMode();

            // If mode is already set (even to the same value), reboot
            requiresReboot = true;
            return;
        }

        // Create and initialize device manager based on requested mode
        if (newMode == DeviceMode::XINPUT_MODE) {
            // XInput mode: Create empty XInput manager
            XinputDeviceManager* xinputManager = new XinputDeviceManager();
            xinputManager->vendorId(0x045E)      // Microsoft VID
                         ->productId(0x028E)      // Xbox 360 Controller PID
                         ->manufacturer("Microsoft")
                         ->productName("Xbox 360 Controller")
                         ->serialNumber("000000");

            deviceManager = xinputManager;
        } else {
            // HID mode: Create empty HID manager
            HidDeviceManager* hidManager = new HidDeviceManager();
            hidManager->vendorId(0x1209)           // pid.codes (open source VID)
                     ->productId(0x0003)          // Custom PID
                     ->manufacturer("InputProxy")
                     ->productName("InputProxy Composite Device")
                     ->serialNumber("20260118");

            deviceManager = hidManager;
        }

        // Initialize the device manager (without any devices plugged)
        setDeviceManager(deviceManager);
        deviceManager->init();
    };

    main2PicoRpcServer.getMode = []() -> uint8_t {
        if (!deviceManager) {
            return 0xFF;  // Return invalid value when no mode is set
        }
        return static_cast<uint8_t>(deviceManager->getMode());
    };

    main2PicoRpcServer.plugDevice = [](int slotIndex, DeviceConfiguration deviceConfig) -> bool {
        if (!deviceManager) {
            return false;
        }

        DeviceMode currentMode = deviceManager->getMode();

        // Check if device type is compatible with current mode
        // deviceType: 0=Keyboard, 1=Mouse, 2=HID Gamepad, 3=Xbox360 Gamepad
        if (deviceConfig.deviceType == 3) { // Xbox360 Gamepad
            if (currentMode != DeviceMode::XINPUT_MODE) {
                return false; // XInput devices only work in XInput mode
            }
        } else if (deviceConfig.deviceType >= 0 && deviceConfig.deviceType <= 2) {
            if (currentMode != DeviceMode::HID_MODE) {
                return false; // HID devices only work in HID mode
            }
        } else {
            return false; // Unknown device type
        }

        // Create and plug the device
        if (currentMode == DeviceMode::HID_MODE) {
            HidDeviceManager* hidManager = static_cast<HidDeviceManager*>(deviceManager);

            if (deviceConfig.deviceType == 0) { // Keyboard
                TinyUsbKeyboardBuilder keyboardBuilder;
                return hidManager->plugDevice(slotIndex, keyboardBuilder.name("InputProxy Keyboard").build(),
                                            keyboardBuilder.getName(), keyboardBuilder.getDeviceType());
            } else if (deviceConfig.deviceType == 1) { // Mouse
                TinyUsbMouseBuilder mouseBuilder;
                return hidManager->plugDevice(slotIndex, mouseBuilder.name("InputProxy Mouse").build(),
                                            mouseBuilder.getName(), mouseBuilder.getDeviceType());
            } else if (deviceConfig.deviceType == 2) { // HID Gamepad
                TinyUsbGamepadBuilder gamepadBuilder;
                std::string gamepadName = "InputProxy Gamepad " + std::to_string(slotIndex);
                gamepadBuilder.gamepadIndex(slotIndex)
                          .name(gamepadName)
                          .axes(deviceConfig.config.hidGamepadConfig.axesMask)
                          .buttons(deviceConfig.config.hidGamepadConfig.buttons)
                          .hat(deviceConfig.config.hidGamepadConfig.hat != 0);

                return hidManager->plugDevice(slotIndex, gamepadBuilder.build(),
                                            gamepadBuilder.getName(),
                                            gamepadBuilder.getDeviceType(),
                                            gamepadBuilder.getAxesCount());
            }
        } else if (currentMode == DeviceMode::XINPUT_MODE) {
            XinputDeviceManager* xinputManager = static_cast<XinputDeviceManager*>(deviceManager);

            if (deviceConfig.deviceType == 3) { // Xbox360 Gamepad
                XInputDevice* gamepad = new XInputDevice(slotIndex);
                std::string gamepadName = "Xbox 360 Controller " + std::to_string(slotIndex + 1);
                return xinputManager->plugDevice(slotIndex, gamepad, gamepadName);
            }
        }

        return false;
    };

    main2PicoRpcServer.unplugDevice = [](int slotIndex) -> bool {
        if (!deviceManager) {
            return false;
        }

        DeviceMode currentMode = deviceManager->getMode();

        if (currentMode == DeviceMode::HID_MODE) {
            HidDeviceManager* hidManager = static_cast<HidDeviceManager*>(deviceManager);
            return hidManager->unplugDevice(slotIndex);
        } else if (currentMode == DeviceMode::XINPUT_MODE) {
            XinputDeviceManager* xinputManager = static_cast<XinputDeviceManager*>(deviceManager);
            return xinputManager->unplugDevice(slotIndex);
        }

        return false;
    };

    uartRpcManager->registerServer(main2PicoRpcServer);

    pico2MainRpcClient = uartRpcManager->createClient<Pico2Main>();

    return true;
}

void reboot(){
    sleep_ms(100);  // Brief delay to ensure all communication completes
    watchdog_enable(1, false);
    while(true) { tight_loop_contents(); }
}

int main() {
    srand(to_us_since_boot(get_absolute_time()));

    initPicoLed();

    // Initialize TinyUSB
    tusb_init();

    enableDefaultLed(true);
    sleep_ms(100);
    enableDefaultLed(false);

    // Load persistent storage
    persistentStorage.load();

    // Get or generate deviceId
    std::string deviceId = persistentStorage.get("deviceId");
    if (deviceId.empty()) {
        deviceId = generateRandomDeviceId();
        persistentStorage.put("deviceId", deviceId);
        persistentStorage.flush();
    }

    uartManager = new UartManagerPico(UartPort::UART_0);
    uartManager->onMessage(handleUartMessage);

    if (!initUartRpcSystem())
        return 1;

    // Call onBoot in loop until success
    bool bootSuccess = false;
    while (!bootSuccess) {
        bootSuccess = pico2MainRpcClient.onBoot(deviceId);
        if (!bootSuccess) {
            usbAwareSleep(300);
        }
    }

    // Device manager will be initialized via setMode RPC call
    // Start without any mode set

    while (true) {
        tud_task();

        // Update all devices through DeviceManager
        if (deviceManager) {
            deviceManager->update();
        }

        // Check if reboot is required (after mode change)
        if (requiresReboot) {
            reboot();
        }
    }
    return 0;
}
