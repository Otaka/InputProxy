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
#include "../shared/PicoConfig.h"
#include "../shared/crc32.h"
#include "UartManagerPico.h"
#include "PersistentStorage.h"
#include "devices/AbstractDeviceManager.h"
#include "devices/HidDeviceManager.h"
#include "devices/XinputDeviceManager.h"
#include "devices/TinyUsbMouseDevice.h"

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
        const char* rebootMethodMessage = "Received reboot request";
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

    // setConfiguration(string configJson) → bool ok, string errorMsg
    rpc->registerMethod(M2P_SET_CONFIGURATION, [rpc](RpcArg* arg) -> RpcArg* {
        char jsonBuf[960] = {}; // 900 byte limit + headroom
        arg->getString(jsonBuf, sizeof(jsonBuf));
        int jsonLen = (int)strlen(jsonBuf);

        if (jsonLen <= 0 || jsonLen > 900) {
            RpcArg* out = rpc->getRpcArg();
            out->putBool(false);
            out->putString("config too large or empty");
            return out;
        }

        PicoConfig cfg;
        std::string err;
        if (!parsePicoConfig(jsonBuf, jsonLen, cfg, err)) {
            RpcArg* out = rpc->getRpcArg();
            out->putBool(false);
            out->putString(err.c_str());
            return out;
        }

        persistentStorage.put("config", std::string(jsonBuf, jsonLen));
        persistentStorage.flush();

        // Send success reply before rebooting
        RpcArg* out = rpc->getRpcArg();
        out->putBool(true);
        out->putString("");
        // Reboot via channel
        rebootChannel->send(false);
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

// Announces this Pico to Main with its configCrc32. Returns true when Main acknowledges.
// Must be called from a coroutine (blocks until RPC_OK or timeout).
static bool rpcOnBoot(const std::string& deviceId, uint32_t configCrc32) {
    RpcArg* arg = rpcManager->getRpcArg();
    arg->putString(deviceId.c_str());
    arg->putInt32(static_cast<int32_t>(configCrc32)); // uint32 sent as int32 bit-pattern
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

    persistentStorage.load();

    // Get or generate deviceId
    std::string deviceId = persistentStorage.get("deviceId");
    if (deviceId.empty()) {
        deviceId = generateRandomDeviceId();
        persistentStorage.put("deviceId", deviceId);
        persistentStorage.flush();
    }

    // -- Config-driven initialization --
    std::string configJson  = persistentStorage.get("config");
    PicoConfig  picoConfig;
    uint32_t    configCrc32 = 0;
    std::string parseError;

    if (configJson.empty() || !parsePicoConfig(configJson.c_str(), (int)configJson.size(), picoConfig, parseError)) {
        if (!configJson.empty()) {
            // Corrupt stored config — clear it
            logChannel->send("Config parse failed: " + parseError + " — using fallback");
            persistentStorage.remove("config");
            persistentStorage.flush();
        }
        // Fallback: HID mode, one keyboard
        picoConfig = PicoConfig();
        picoConfig.mode = HID_MODE;
        PicoDeviceConfig kbd;
        kbd.type = PicoDeviceType::KEYBOARD;
        kbd.name = "Keyboard";
        picoConfig.devices.push_back(kbd);
        configCrc32 = 0;  // defined constant — NOT crc32("", 0)
    } else {
        configCrc32 = crc32(configJson.c_str(), configJson.size());
    }

    // Build device manager from PicoConfig
    if (picoConfig.mode == XINPUT_MODE) {
        XinputDeviceManager* xm = new XinputDeviceManager();
        xm->vendorId(picoConfig.vid)
          ->productId(picoConfig.pid)
          ->manufacturer(picoConfig.manufacturer)
          ->productName(picoConfig.product)
          ->serialNumber(picoConfig.serial);
        for (const auto& d : picoConfig.devices) {
            if (d.type == PicoDeviceType::XBOX360_GAMEPAD)
                xm->plugGamepad(d.name.empty() ? std::string("Xbox 360 Controller") : d.name);
        }
        deviceManager = xm;
    } else {
        HidDeviceManager* hm = new HidDeviceManager();
        hm->vendorId(picoConfig.vid)
          ->productId(picoConfig.pid)
          ->manufacturer(picoConfig.manufacturer)
          ->productName(picoConfig.product)
          ->serialNumber(picoConfig.serial);
        for (const auto& d : picoConfig.devices) {
            switch (d.type) {
                case PicoDeviceType::KEYBOARD:
                    hm->plugKeyboard(d.name.empty() ? std::string("Keyboard") : d.name);
                    break;
                case PicoDeviceType::MOUSE:
                    hm->plugMouse(d.name.empty() ? std::string("Mouse") : d.name);
                    break;
                case PicoDeviceType::HID_GAMEPAD:
                    hm->plugGamepad(d.name.empty() ? std::string("Gamepad") : d.name,
                                    d.buttons, d.axesMask, d.hat);
                    break;
                default: break;
            }
        }
        hm->prepareDescriptors();
        deviceManager = hm;
    }
    setDeviceManager(deviceManager);

    tusb_init();
    tud_task();

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

    rpcOnBoot(deviceId, configCrc32);
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
            sleep(30000);
            sprintf(buffer, "Hello from pico %d", index++);
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
