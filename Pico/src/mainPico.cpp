#define RPCMANAGER_STD_STRING

#include <string.h>
#include <cstdlib>
#include <ctime>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/bootrom.h"
#include "pico/time.h"
#include "tusb.h"
#include "../shared/corocgo.h"
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
#include "hardware/watchdog.h"

using namespace simplerpc;
using namespace corocgo;

static UartManagerPico* uartManager = nullptr;
static char uartInputBuffer[5120];
// RPC Manager for UART communication with Mainboard
static RpcManager<>* uartRpcManager = nullptr;
static Pico2Main pico2MainRpcClient;
static Main2Pico main2PicoRpcServer;
std::vector<std::string>messagesToPrint;
static bool ledState = false;

// Device Manager - manages devices based on current mode (HID or XInput)
static AbstractDeviceManager* deviceManager = nullptr;

// Persistent storage for configuration (mode, etc.)
static PersistentStorage persistentStorage;

static Channel<bool>*rebootChannel;
static Channel<std::string>*logChannel;

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

void initUartRpcSystem() {
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
        rebootChannel->sendExternalNoBlock(true);
        return true;
    };

    main2PicoRpcServer.reboot = []() {
        rebootChannel->sendExternalNoBlock(false);
    };

    main2PicoRpcServer.setMode = [](uint8_t mode) {
        DeviceMode newMode = static_cast<DeviceMode>(mode);

        // If same as current mode, skip
        if (deviceManager && deviceManager->getMode() == newMode) {
            return;
        }

        // Different mode: persist and schedule reboot
        persistentStorage.put("mode", newMode == DeviceMode::XINPUT_MODE ? "XINPUT" : "HID");
        persistentStorage.flush();
        rebootChannel->sendExternalNoBlock(false);
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
}

void reboot(){
    sleep_ms(100);  // Brief delay to ensure all communication completes
    watchdog_enable(1, false);
    while(true) { tight_loop_contents(); }
}

int _main() {
    srand(to_us_since_boot(get_absolute_time()));
    rebootChannel=makeChannel<bool>(1,1);
    logChannel = makeChannel<std::string>(10);

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

    // Initialize device manager from saved mode (default HID)
    std::string savedMode = persistentStorage.get("mode");
    DeviceMode bootMode = (savedMode == "XINPUT") ? DeviceMode::XINPUT_MODE : DeviceMode::HID_MODE;
    if (bootMode == DeviceMode::XINPUT_MODE) {
        XinputDeviceManager* xinputManager = new XinputDeviceManager();
        xinputManager->vendorId(0x045E)
                        ->productId(0x028E)
                        ->manufacturer("Microsoft")
                        ->productName("Xbox 360 Controller")
                        ->serialNumber("000000");
        deviceManager = xinputManager;
    } else {
        HidDeviceManager* hidManager = new HidDeviceManager();
        hidManager->vendorId(0x1209)
                    ->productId(0x0003)
                    ->manufacturer("InputProxy")
                    ->productName("InputProxy Composite Device")
                    ->serialNumber("20260118");
        deviceManager = hidManager;
        hidManager->plugDevice(0, TinyUsbKeyboardBuilder().build(), "", DeviceType::KEYBOARD, 3);
    }
    setDeviceManager(deviceManager);
    deviceManager->init();

    uartManager = new UartManagerPico();

    initUartRpcSystem();

    coro([&deviceId](){
        while (!pico2MainRpcClient.onBoot(deviceId)) {
            sleep(300);
        }
    });
    
    //log coroutine
    coro([]() {
        while (true) {
            auto [logLine, error] = logChannel->receive();
            if (error)
                break;
            pico2MainRpcClient.debugPrint(logLine.c_str());
        }
    });

    //reboot coroutine
    coro([]() {
        while (true) {
            auto [rebootValue, error] = rebootChannel->receive();
            if (error)
                break;
            if (rebootValue==true) {
                rebootToBootsel();
            } else {
                reboot();
            }
        }
    });

    //uart read coroutine
    coro([]() {
        while (true) {
            size_t len = uartManager->read(uartInputBuffer, sizeof(uartInputBuffer));
            if (len > 0 && uartRpcManager) {
                uartRpcManager->processInput(uartInputBuffer, len);
            }
        }
    });

    //send periodic message
    coro([]() {
        int index=0;
        char buffer[256];
        while (true) {
            sleep(2000);
            sprintf(buffer, "Hello world %d", index++);
            logChannel->send(buffer);
        }
    });

    //every tick coroutine
    while (true) {
        // Update all devices through DeviceManager
        deviceManager->update();

        //process usb tasks
        tud_task();

        coro_yield();
    }
}

int main() {
    coro(_main);
    scheduler_start();
    return 0;
}
