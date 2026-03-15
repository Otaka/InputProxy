#include <string.h>
#include <cstdlib>
#include <cstdarg>
#include <algorithm>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/bootrom.h"
#include "pico/time.h"
#include "pico/stdio.h"
#include "tusb.h"
#include "hardware/watchdog.h"
#include "hardware/uart.h"

#include "../shared/Corocgo/corocgo.h"
#include "../shared/Corocgo/corocrpc/corocrpc.h"
#include "../shared/rpcinterface.h"
#include "../shared/shared.h"
#include "UartManagerPico.h"
#include "PersistentStorage.h"
#include "devices/AbstractDeviceManager.h"
#include "devices/HidDeviceManager.h"
#include "devices/XinputDeviceManager.h"
#include "devices/TinyUsbKeyboardDevice.h"
#include "devices/TinyUsbMouseDevice.h"
#include "devices/TinyUsbGamepadDevice.h"
#include "devices/XInputDevice.h"

using namespace corocrpc;
using namespace corocgo;

static UartManagerPico* uartManager  = nullptr;
static StreamFramer*    framer       = nullptr;
static Channel<RpcPacket>* rpcOutCh  = nullptr;
static Channel<RpcPacket>* rpcInCh   = nullptr;
static RpcManager*      rpcManager   = nullptr;

static bool ledState = false;

static AbstractDeviceManager* deviceManager = nullptr;
static PersistentStorage persistentStorage;

static Channel<bool>*        rebootChannel = nullptr;
static Channel<std::string>* logChannel    = nullptr;

static char uartInputBuffer[2120];

// ---------------------------------------------------------------------------
// LED / utility helpers
// ---------------------------------------------------------------------------

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
    static const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    const int charsetSize = sizeof(charset) - 1;
    std::string result;
    result.reserve(5);
    for (int i = 0; i < 5; i++)
        result += charset[rand() % charsetSize];
    return result;
}

void reboot() {
    sleep_ms(100);
    watchdog_enable(1, false);
    while (true) { tight_loop_contents(); }
}

// ---------------------------------------------------------------------------
// plugDevice helper (same logic as before, extracted for clarity)
// ---------------------------------------------------------------------------

static bool doPlugDevice(int slotIndex, const DeviceConfiguration& dc) {
    if (!deviceManager) return false;

    DeviceMode currentMode = deviceManager->getMode();

    if (currentMode == DeviceMode::HID_MODE) {
        if (dc.deviceType == 0) {  // Keyboard
            return deviceManager->plugDevice(slotIndex,
                TinyUsbKeyboardBuilder().name("InputProxy Keyboard").build());
        } else if (dc.deviceType == 1) {  // Mouse
            return deviceManager->plugDevice(slotIndex,
                TinyUsbMouseBuilder().name("InputProxy Mouse").build());
        } else if (dc.deviceType == 2) {  // HID Gamepad
            std::string name = "InputProxy Gamepad " + std::to_string(slotIndex);
            TinyUsbGamepadBuilder gb;
            gb.gamepadIndex(slotIndex)
              .name(name)
              .axes(dc.config.hidGamepadConfig.axesMask)
              .buttons(dc.config.hidGamepadConfig.buttons)
              .hat(dc.config.hidGamepadConfig.hat != 0);
            return deviceManager->plugDevice(slotIndex, gb.build());
        }
    } else if (currentMode == DeviceMode::XINPUT_MODE) {
        if (dc.deviceType == 3) {  // Xbox360 Gamepad
            std::string name = "Xbox 360 Controller " + std::to_string(slotIndex + 1);
            return deviceManager->plugDevice(slotIndex, new XInputDevice(slotIndex, name));
        }
    }

    return false;
}

// ---------------------------------------------------------------------------
// RPC setup
// ---------------------------------------------------------------------------

void initUartRpcSystem() {
    framer    = new StreamFramer();
    rpcOutCh  = makeChannel<RpcPacket>(8);
    rpcInCh   = makeChannel<RpcPacket>(8);
    rpcManager = new RpcManager(rpcOutCh, rpcInCh, /*timeoutMs=*/5000);

    RpcManager* rpc = rpcManager;

    // ── Server: methods Main calls on Pico ───────────────────────────────

    // ping(int32 val) → int32 val
    rpc->registerMethod(M2P_PING, [rpc](RpcArg* arg) -> RpcArg* {
       // toggleDefaultLed();
        int32_t val = arg->getInt32();
        RpcArg* out = rpc->getRpcArg();
        out->putInt32(val+1);
        return out;
    });

    // setLed(bool state) → void
    rpc->registerMethod(M2P_SET_LED, [](RpcArg* arg) -> RpcArg* {
        enableDefaultLed(arg->getBool());
        return nullptr;
    });

    // getLedStatus() → bool state
    rpc->registerMethod(M2P_GET_LED_STATUS, [rpc](RpcArg* arg) -> RpcArg* {
        RpcArg* out = rpc->getRpcArg();
        out->putBool(checkDefaultLed());
        return out;
    });

    // rebootFlashMode() → bool (triggers USB boot; no real reply)
    rpc->registerMethod(M2P_REBOOT_FLASH_MODE, [rpc](RpcArg* arg) -> RpcArg* {
        rebootChannel->send(true);
        RpcArg* out = rpc->getRpcArg();
        out->putBool(true);
        return out;
    });

    // reboot() → void (Pico reboots; no reply sent)
    rpc->registerMethod(M2P_REBOOT, [](RpcArg* arg) -> RpcArg* {
        toggleDefaultLed();
        const char*rebootMethodMessage="Received reboot request";
        logChannel->send(rebootMethodMessage);

        sleep(1000);
        rebootChannel->send(false);
        return nullptr;
    });

    // setAxis(int32 device, int32 axis, int32 value) → void
    rpc->registerMethod(M2P_SET_AXIS, [](RpcArg* arg) -> RpcArg* {
        int32_t device = arg->getInt32();
        int32_t axis   = arg->getInt32();
        int32_t value  = arg->getInt32();
        if (deviceManager) {
            toggleDefaultLed();
            deviceManager->setAxis(device, axis, value);
        }
        return nullptr;
    });

    // setMode(int32 mode) → void  (saves to flash, reboots)
    rpc->registerMethod(M2P_SET_MODE, [](RpcArg* arg) -> RpcArg* {
        DeviceMode newMode = static_cast<DeviceMode>(arg->getInt32());
        if (deviceManager && deviceManager->getMode() == newMode) return nullptr;
        persistentStorage.put("mode", newMode == DeviceMode::XINPUT_MODE ? "XINPUT" : "HID");
        persistentStorage.flush();
        rebootChannel->send(false);
        return nullptr;
    });

    // getMode() → int32 mode
    rpc->registerMethod(M2P_GET_MODE, [rpc](RpcArg* arg) -> RpcArg* {
        RpcArg* out = rpc->getRpcArg();
        out->putInt32(deviceManager ? static_cast<int32_t>(deviceManager->getMode()) : -1);
        return out;
    });

    // plugDevice(int32 slotIndex, int32 deviceType, int32 hat, int32 axesMask, int32 buttons) → bool
    rpc->registerMethod(M2P_PLUG_DEVICE, [rpc](RpcArg* arg) -> RpcArg* {
        int32_t slotIndex  = arg->getInt32();
        int32_t deviceType = arg->getInt32();
        int32_t hat        = arg->getInt32();
        int32_t axesMask   = arg->getInt32();
        int32_t buttons    = arg->getInt32();

        DeviceConfiguration dc;
        dc.deviceType = deviceType;
        dc.config.hidGamepadConfig.hat      = static_cast<uint8_t>(hat);
        dc.config.hidGamepadConfig.axesMask = static_cast<uint16_t>(axesMask);
        dc.config.hidGamepadConfig.buttons  = static_cast<uint8_t>(buttons);

        bool ok = doPlugDevice(static_cast<int>(slotIndex), dc);
        RpcArg* out = rpc->getRpcArg();
        out->putBool(ok);
        return out;
    });

    // unplugDevice(int32 slotIndex) → bool
    rpc->registerMethod(M2P_UNPLUG_DEVICE, [rpc](RpcArg* arg) -> RpcArg* {
        int32_t slotIndex = arg->getInt32();
        bool ok = false;
        if (deviceManager) {
            ok = deviceManager->unplugDevice(slotIndex);
        }
        RpcArg* out = rpc->getRpcArg();
        out->putBool(ok);
        return out;
    });

    // ── Transport bridges ─────────────────────────────────────────────────

    // Outbound: rpcOutCh → frame → UART send
    coro([]() {
        while (true) {
            auto res = rpcOutCh->receive();
            if (res.error) break;
            FramedPacket fp = framer->createPacket(
                0, reinterpret_cast<const char*>(res.value.data), res.value.size);
            if (fp.size > 0)
                uartManager->sendData(reinterpret_cast<const char*>(fp.data), fp.size);
        }
    });

    // Inbound: framer.readCh → deframe → rpcInCh
    coro([]() {
        while (true) {
            auto res = framer->readCh->receive();
            if (res.error) break;
            static RpcPacket pkt;
            pkt.size = res.value.getDataSize();
            memcpy(pkt.data, res.value.getData(), pkt.size);
            rpcInCh->send(pkt);
        }
    });
}

// ---------------------------------------------------------------------------
// RPC call wrappers (Pico → Main)
// ---------------------------------------------------------------------------

// Announces this Pico to Main. Returns true when Main acknowledges.
// Must be called from a coroutine (blocks until RPC_OK or timeout).
static bool rpcOnBoot(const std::string& deviceId, DeviceMode bootMode) {
    RpcArg* arg = rpcManager->getRpcArg();
    arg->putString(deviceId.c_str());
    arg->putInt32(static_cast<int32_t>(bootMode));
    RpcResult res = rpcManager->call(P2M_ON_BOOT, arg);
    rpcManager->disposeRpcArg(arg);
    bool accepted = (res.error == RPC_OK && res.arg && res.arg->getBool());
    if (res.arg) rpcManager->disposeRpcArg(res.arg);
    return accepted;
}

// Sends a log message to Main (void; result ignored).
// Must be called from a coroutine.
static void rpcDebugPrint(const std::string& message) {
    RpcArg* arg = rpcManager->getRpcArg();
    arg->putString(message.c_str());
    rpcManager->callNoResponse(P2M_DEBUG_PRINT, arg);
    rpcManager->disposeRpcArg(arg);
}

// ---------------------------------------------------------------------------

int _main() {
    srand(to_us_since_boot(get_absolute_time()));
    rebootChannel = makeChannel<bool>(1, 1);
    logChannel    = makeChannel<std::string>(10);

    initPicoLed();
    tusb_init();
    tud_task();

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
        XinputDeviceManager* xm = new XinputDeviceManager();
        xm->vendorId(0x045E)->productId(0x028E)
          ->manufacturer("Microsoft")
          ->productName("Xbox 360 Controller")
          ->serialNumber("000000");
        deviceManager = xm;
    } else {
        HidDeviceManager* hm = new HidDeviceManager();
        hm->vendorId(0x1209)->productId(0x0003)
          ->manufacturer("InputProxy")
          ->productName("InputProxy Composite Device")
          ->serialNumber("20260118");
        deviceManager = hm;
        deviceManager->plugDevice(0, TinyUsbKeyboardBuilder().name("InputProxy Keyboard").build());
        deviceManager->plugDevice(1, TinyUsbMouseBuilder().name("InputProxy Mouse").build());
        deviceManager->plugDevice(2, TinyUsbGamepadBuilder().name("InputProxy Gamepad").axes(FLAG_MASK_GAMEPAD_AXIS_LX|FLAG_MASK_GAMEPAD_AXIS_LY).buttons(32).hat(true).build());
        deviceManager->plugDevice(3, TinyUsbGamepadBuilder().name("InputProxy Gamepad 2").axes(FLAG_MASK_GAMEPAD_AXIS_LX|FLAG_MASK_GAMEPAD_AXIS_LY|FLAG_MASK_GAMEPAD_AXIS_LZ).buttons(8).hat(false).build());
    }
    setDeviceManager(deviceManager);
    //keyboard test
    coro([](){
        int keyboardDeviceIndex=0;
        while(true) {
            deviceManager->setAxis(keyboardDeviceIndex,KEY_NUM_LOCK,1000);
            sleep(1000);
            deviceManager->setAxis(keyboardDeviceIndex,KEY_NUM_LOCK,0);
            sleep(1000);
        }
    });
    
    //mouse test
    coro([](){
        int mouseDeviceIndex=1;
        for(int i=0;i<20;i++){
            deviceManager->setAxis(mouseDeviceIndex,MOUSE_AXIS_X_MINUS,1);
            sleep(20);
        }
        sleep(1000);

        for(int i=0;i<20;i++){
            deviceManager->setAxis(mouseDeviceIndex,MOUSE_AXIS_X_PLUS,1);
            sleep(20);
        }
        sleep(1000);
    });
    
    
    //gamepad test
    coro([](){
        int gamepadDeviceIndex=2;
        while(true) {
            for(int i=0;i<2;i++){
                deviceManager->setAxis(gamepadDeviceIndex+i,GAMEPAD_BTN_1,1000);
                sleep(200);
                deviceManager->setAxis(gamepadDeviceIndex+i,GAMEPAD_BTN_1,0);
                sleep(200);
                deviceManager->setAxis(gamepadDeviceIndex+i,GAMEPAD_BTN_2,1000);
                sleep(200);
                deviceManager->setAxis(gamepadDeviceIndex+i,GAMEPAD_BTN_2,0);
                sleep(200);
                deviceManager->setAxis(gamepadDeviceIndex+i,GAMEPAD_AXIS_LX_MINUS,1000);
                sleep(200);
                deviceManager->setAxis(gamepadDeviceIndex+i,GAMEPAD_AXIS_LX_MINUS,0);
                sleep(200);
                deviceManager->setAxis(gamepadDeviceIndex+i,GAMEPAD_AXIS_LY_MINUS,1000);
                sleep(200);
                deviceManager->setAxis(gamepadDeviceIndex+i,GAMEPAD_AXIS_LY_MINUS,0);
                sleep(200);
            }
        }
    });
    

    /*TinyUsbGamepadBuilder gb;
    gb.gamepadIndex(1).name("InputProxy Gamepad 0").axes(2).buttons(8).hat(true);
    deviceManager->plugDevice(1, gb.build());
    coro([](){
        while(true){
            deviceManager->setAxis(1,10,1000);
            sleep(500);
            deviceManager->setAxis(1,10,0);
            sleep(500);
        }
    });*/

    deviceManager->init();
    uartManager = new UartManagerPico();
    initUartRpcSystem();

    enableDefaultLed(true);
    sleep_ms(100);
    enableDefaultLed(false);
    // UART → framer coroutine (interrupt-driven read, feeds StreamFramer)
    coro([]() {
        while (true) {
            size_t len = uartManager->read(uartInputBuffer, sizeof(uartInputBuffer));
            if (len == 0) continue;
            // Feed bytes to framer in MAX_SIZE chunks
            size_t offset = 0;
            while (offset < len) {
                RawChunk chunk;
                uint16_t chunkLen = static_cast<uint16_t>(
                    std::min(len - offset, (size_t)RawChunk::MAX_SIZE));
                memcpy(chunk.data, uartInputBuffer + offset, chunkLen);
                chunk.len = chunkLen;
                framer->writeCh->send(chunk);
                offset += chunkLen;
            }
        }
    });

    rpcOnBoot(deviceId, bootMode);
    // Log coroutine — drains logChannel and sends debugPrint to Main
    coro([]() {
        while (true) {
            auto [logLine, error] = logChannel->receive();
            if (error) break;
            rpcDebugPrint(logLine);
        }
    });

    // Reboot coroutine
    coro([]() {
        while (true) {
            auto [rebootValue, error] = rebootChannel->receive();
            if (error) break;
            if (rebootValue) rebootToBootsel();
            else             reboot();
        }
    });

    // Periodic heartbeat — sends "Hello world N" to Main every second
    coro([]() {
        int index = 0;
        char buffer[256];
        while (true) {
            sleep(1000);
            //toggleDefaultLed();
            sprintf(buffer, "Hello world %d", index++);
            logChannel->send(buffer);
        }
    });

    // Main loop — update USB devices every tick
    while (true) {
        deviceManager->update();
        tud_task();
        coro_yield();
    }

    return 0;
}


int main() {
    coro(_main);
    scheduler_start();
    return 0;
}
