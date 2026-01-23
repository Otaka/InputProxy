#define RPCMANAGER_STD_STRING

#include <string.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/bootrom.h"
#include "pico/time.h"
#include "pico/mutex.h"
#include "tusb.h"

#include "UartManagerPico.h"
#include "../../shared/simplerpc/simplerpc.h"
#include "../../shared/rpcinterface.h"
#include "devices/DeviceManager.h"
#include "devices/TinyUsbKeyboardDevice.h"
#include "devices/TinyUsbMouseDevice.h"
#include "devices/TinyUsbGamepadDevice.h"
#include "Timer.h"

using namespace simplerpc;

static UartManagerPico* uartManager = nullptr;

// RPC Manager for UART communication with Mainboard
static RpcManager<>* uartRpcManager = nullptr;
static Pico2Main pico2MainRpcClient;
static Main2Pico main2PicoRpcServer;

static bool ledState = false;

// Device Manager - manages all HID devices
static DeviceManager* deviceManager = nullptr;

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

void handleUartMessage(const char* data, size_t length) {
    // Forward raw UART data to UART RPC manager (input filter handles deframing)
    if (data && length > 0 && uartRpcManager) {
        uartRpcManager->processInput(data, length);
    }
}

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

    uartRpcManager->registerServer(main2PicoRpcServer);

    pico2MainRpcClient = uartRpcManager->createClient<Pico2Main>();
    return true;
}

void initPicoLed() {
    stdio_init_all();
    cyw43_arch_init();
}

int main() {
    initPicoLed();

    // Initialize TinyUSB
    tusb_init();

    enableDefaultLed(true);
    sleep_ms(100);
    enableDefaultLed(false);

    uartManager = new UartManagerPico(UartPort::UART_0);
    uartManager->onMessage(handleUartMessage);

    if (!initUartRpcSystem())
        return 1;

    // Create and initialize Device Manager
    deviceManager = new DeviceManager();

    // Configure USB device-level properties (VID/PID/Serial)
    deviceManager->vendorId(0x1209)           // pid.codes (open source VID)
                 .productId(0x0003)          // Custom PID
                 .manufacturer("InputProxy")
                 .productName("InputProxy Composite Device")
                 .serialNumber("20260118");

    setDeviceManager(deviceManager);

    // Add devices using DeviceManager with builder pattern
    // Socket 0: Keyboard
    TinyUsbKeyboardBuilder keyboardBuilder;
    deviceManager->plugDevice(0, keyboardBuilder.name("InputProxy Keyboard").build(), 
                             keyboardBuilder.getName(), keyboardBuilder.getDeviceType());
    
    // Socket 1: Mouse
    TinyUsbMouseBuilder mouseBuilder;
    deviceManager->plugDevice(1, mouseBuilder.name("InputProxy Mouse").build(), 
                             mouseBuilder.getName(), mouseBuilder.getDeviceType());
    
    // Sockets 2-5: 4 Gamepads
    for (int i = 0; i < 4; i++) {
        TinyUsbGamepadBuilder gamepadBuilder;
        std::string gamepadName = "InputProxy Gamepad " + std::to_string(i + 1);
        if(i==0) {
            gamepadBuilder.gamepadIndex(i)
                      .name(gamepadName)
                      .axes(FLAG_MASK_GAMEPAD_AXIS_LX | FLAG_MASK_GAMEPAD_AXIS_LY)
                      .buttons(1)
                      .hat(true); 
            
        } else if (i==1) {
            gamepadBuilder.gamepadIndex(i)
                      .name(gamepadName)
                      .axes(FLAG_MASK_GAMEPAD_AXIS_LX | FLAG_MASK_GAMEPAD_AXIS_LY)
                      .buttons(1)
                      .hat(true); 
        } else if (i==2) {
            gamepadBuilder.gamepadIndex(i)
                      .name(gamepadName)
                      .axes(FLAG_MASK_GAMEPAD_AXIS_LX | FLAG_MASK_GAMEPAD_AXIS_LY)
                      .buttons(1)
                      .hat(true); 
        } else {
            gamepadBuilder.gamepadIndex(i)
                      .name(gamepadName)
                      .axes(FLAG_MASK_GAMEPAD_AXIS_LX | FLAG_MASK_GAMEPAD_AXIS_LY)
                      .buttons(1)
                      .hat(true);
        }

        //work correctly
        // gamepadBuilder.gamepadIndex(i)
        //               .name(gamepadName)
        //               .buttons(1)
        //               .axes(FLAG_MASK_GAMEPAD_AXIS_LZ | FLAG_MASK_GAMEPAD_AXIS_RZ)
        //               .hat(true);
        //working
        
        // working - 16 buttons, hat, 2 axes
        // gamepadBuilder.gamepadIndex(i)
        //               .name(gamepadName)
        //               .axes(FLAG_MASK_GAMEPAD_AXIS_LZ | FLAG_MASK_GAMEPAD_AXIS_RZ)
        //               .hat(true);
        
        // working, no axes 17 buttons, no hat
        // gamepadBuilder.gamepadIndex(i)
        //               .name(gamepadName)
        //               .hat(true);

        //working
        // gamepadBuilder.gamepadIndex(i)
        //                .name(gamepadName)
        //                .buttons(0)
        //                .hat(true);
        
        //working 2 buttons, no hat
        // gamepadBuilder.gamepadIndex(i)
        //                .name(gamepadName)
        //                .buttons(1)
        //                .hat(true);
        
        // working 1 button, no hat
        // gamepadBuilder.gamepadIndex(i)
        //                .name(gamepadName)
        //                .buttons(1)
        //                .hat(false);

        // not working
        // gamepadBuilder.gamepadIndex(i)
        //                .name(gamepadName)
        //                .buttons(0)
        //                .hat(false);
        
        //working - no buttons, 2 axes, hat
        // gamepadBuilder.gamepadIndex(i)
        //                .name(gamepadName)
        //                .axes(FLAG_MASK_GAMEPAD_AXIS_LZ | FLAG_MASK_GAMEPAD_AXIS_RZ)
        //                .buttons(0)
        //                .hat(true);
        deviceManager->plugDevice(2 + i, gamepadBuilder.build(),
                                 gamepadBuilder.getName(),
                                 gamepadBuilder.getDeviceType(),
                                 gamepadBuilder.getAxesCount());
    }

    // Initialize all devices
    if (!deviceManager->init()) {
        pico2MainRpcClient.debugPrint("Failed to initialize devices");
        return 1;
    }

    // Give USB stack time to enumerate properly
    for (int i = 0; i < 50; i++) {
        tud_task();
        sleep_ms(10);
    }

    while (true) {
        tud_task();

        // Update all devices through DeviceManager
        if (deviceManager) {
            deviceManager->update();
        }
    }
    return 0;
}
