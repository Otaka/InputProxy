#include <iostream>
#include <iomanip>
#include <fcntl.h>
#include <unistd.h>
#include "CoHttpServer.h"
#include "RestApi.h"
#include "../shared/shared.h"
#include "corocrpc/corocrpc.h"
#include "rpcinterface.h"
#include "UartManager.h"
#include "RealDeviceManager.h"
#include "EmulationBoard.h"

using namespace corocrpc;
using namespace corocgo;

// Per-UART link: bundles UartManager + StreamFramer + RPC stack for one Pico connection
struct UartRpcLink {
    UART_CHANNEL         channel;
    UartManager*         uartManager;
    StreamFramer*        framer;
    Channel<RpcPacket>*  rpcOutCh;
    Channel<RpcPacket>*  rpcInCh;
    RpcManager*          rpcManager;
};

std::vector<UartRpcLink> uartLinks;

// Emulation boards (Pico devices connected via UART)
std::vector<EmulationBoard> emulationBoards;
int nextEmulationBoardId = 1;

// Channel for axis events from real devices
Channel<AxisEvent>* axisEventChannel;
RealDeviceManager* deviceManager;


// ---------------------------------------------------------------------------
// UART detection and RPC system setup
// ---------------------------------------------------------------------------

void detectUarts() {
    UART_CHANNEL channels[] = { UART0, UART1, UART2, UART3, UART4, UART5 };
    for (auto ch : channels) {
        if (!UartManager::testUartChannel(ch)) continue;

        auto* uart = new UartManager(ch);
        if (!uart->configureUart()) {
            std::cerr << "UART" << ch << ": detected but failed to configure" << std::endl;
            delete uart;
            continue;
        }
        uart->flushInput();

        UartRpcLink link;
        link.channel     = ch;
        link.uartManager = uart;
        link.framer      = nullptr;
        link.rpcOutCh    = nullptr;
        link.rpcInCh     = nullptr;
        link.rpcManager  = nullptr;
        uartLinks.push_back(std::move(link));
        std::cout << "UART" << ch << ": detected at " << uart->getActiveDevicePath() << std::endl;
    }
    std::cout << "Detected " << uartLinks.size() << " UART channel(s)" << std::endl;
}

bool initRpcSystem() {
    std::cout << "Initializing RPC system..." << std::endl;

    detectUarts();
    if (uartLinks.empty()) {
        std::cerr << "No UART devices found" << std::endl;
        return false;
    }

    for (auto& link : uartLinks) {
        link.framer    = new StreamFramer();
        link.rpcOutCh  = makeChannel<RpcPacket>(8);
        link.rpcInCh   = makeChannel<RpcPacket>(8);
        link.rpcManager = new RpcManager(link.rpcOutCh, link.rpcInCh, /*timeoutMs=*/2000);

        RpcManager*         rpc    = link.rpcManager;
        StreamFramer*       framer = link.framer;
        UartManager*        uart   = link.uartManager;
        Channel<RpcPacket>* rpcOutChannel  = link.rpcOutCh;
        Channel<RpcPacket>* rpcInChannel   = link.rpcInCh;
        UART_CHANNEL        uartChannel     = link.channel;

        // ── Server: methods Pico calls on Main ────────────────────────────

        // ping(int32 val) → int32 val
        rpc->registerMethod(P2M_PING, [rpc](RpcArg* arg) -> RpcArg* {
            int32_t val = arg->getInt32();
            RpcArg* out = rpc->getRpcArg();
            out->putInt32(val);
            return out;
        });

        // debugPrint(string message) → void
        rpc->registerMethod(P2M_DEBUG_PRINT, [uartChannel](RpcArg* arg) -> RpcArg* {
            char debugPrintBuffer[RPC_ARG_BUF_SIZE];
            arg->getString(debugPrintBuffer, sizeof(debugPrintBuffer));
            std::cout << "[UART" << uartChannel << " LOG] " << debugPrintBuffer << std::endl;
            return nullptr;
        });

        // onBoot(string serialString, int32 deviceMode) → bool success
        rpc->registerMethod(P2M_ON_BOOT, [&link, rpc](RpcArg* arg) -> RpcArg* {
            char serial[256];
            arg->getString(serial, sizeof(serial));
            int32_t deviceModeInt = arg->getInt32();
            std::string serialString(serial);
            std::string deviceModeString = (deviceModeInt == HID_MODE) ? "HID_MODE" : "XINPUT_MODE";

            std::cout << "[UART" << link.channel << "] Pico booted with Serial: "
                      << serialString << " in mode " << deviceModeString << std::endl;

            for (auto& board : emulationBoards) {
                if (board.serialString == serialString) {
                    board.active      = true;
                    board.rpc         = rpc;
                    board.uartChannel = link.channel;
                    std::cout << "[UART" << link.channel
                              << "] Reactivated EmulationBoard id=" << board.id << std::endl;
                    RpcArg* out = rpc->getRpcArg();
                    out->putBool(true);
                    return out;
                }
            }

            // New board — register it
            EmulationBoard board;
            board.id           = nextEmulationBoardId++;
            board.serialString = serialString;
            board.rpc          = rpc;
            board.uartChannel  = link.channel;
            board.active       = true;
            emulationBoards.push_back(std::move(board));
            std::cout << "[UART" << link.channel << "] Registered new EmulationBoard id="
                      << (nextEmulationBoardId - 1) << " serial=" << serialString << std::endl;

            RpcArg* out = rpc->getRpcArg();
            out->putBool(true);
            return out;
        });

        // ── Transport bridges ─────────────────────────────────────────────

        // Outbound: rpcOutCh → frame → UART send
        coro([rpcOutChannel, framer, uart]() {
            while (true) {
                ChannelResult<RpcPacket> res = rpcOutChannel->receive();
                if (res.error) break;
                FramedPacket fp = framer->createPacket(
                    0, reinterpret_cast<const char*>(res.value.data), res.value.size);
                if (fp.size > 0)
                    uart->uartSend(reinterpret_cast<const char*>(fp.data), fp.size);
            }
        });

        // Inbound: framer.readCh → deframe → rpcInCh
        coro([rpcInChannel, framer]() {
            while (true) {
                auto res = framer->readCh->receive();
                if (res.error) break;
                RpcPacket pkt;
                pkt.size = res.value.getDataSize();
                memcpy(pkt.data, res.value.getData(), pkt.size);
                rpcInChannel->send(pkt);
            }
        });
    }

    std::cout << "RPC system initialized on " << uartLinks.size() << " channel(s)" << std::endl;
    return true;
}

// ---------------------------------------------------------------------------

void _main() {
    std::cout << "=== Raspberry Pi 4 to Pico RPC System ===" << std::endl;

    if (!initRpcSystem()) {
        std::cerr << "Failed to initialize RPC system" << std::endl;
        return;
    }

    std::vector<std::string> duplicateSerialIds = {};
    deviceManager = new RealDeviceManager(duplicateSerialIds);

    axisEventChannel = makeChannel<AxisEvent>(64);

    // 1. UART → framer coroutines (one per detected channel)
    for (auto& link : uartLinks) {
        coro([&link]() {
            int fd = link.uartManager->getUartFd();
            while (true) {
                auto [flags, err] = wait_file(fd, WAIT_IN);
                if (err) {
                    sleep(200);
                    continue;
                }
                RawChunk chunk;
                ssize_t n = link.uartManager->uartRead(
                    reinterpret_cast<char*>(chunk.data), RawChunk::MAX_SIZE);
                if (n > 0) {
                    chunk.len = static_cast<uint16_t>(n);
                    link.framer->writeCh->send(chunk);
                }
            }
        });
    }

    // 2. Tell each Pico to reboot so it announces itself via onBoot
    for (auto& link : uartLinks) {
        std::cout << "UART" << link.channel << ": sending reboot to Pico..." << std::endl;
        RpcArg* arg = link.rpcManager->getRpcArg();
        link.rpcManager->call(M2P_REBOOT, arg);
        link.rpcManager->disposeRpcArg(arg);
    }

    // 3. Device discovery coroutine — loops every 5 seconds
    coro([]() {
        while (true) {
            auto paths = deviceManager->scanDevices();
            for (auto& path : paths) {
                RealDevice* dev = deviceManager->registerDevice(path);
                if (!dev) continue;
                // Spawn one reading coroutine per device
                coro([dev]() {
                    std::cout << "[CONNECT] device=" << dev->deviceIdStr << std::endl;
                    while (true) {
                        auto [flags, err] = wait_file(dev->fd, WAIT_IN);
                        if (err || !(flags & WAIT_IN)) {
                            dev->active = false;
                            std::cout << "[DISCONNECT] device=" << dev->deviceIdStr << std::endl;
                            break;
                        }
                        if (!deviceManager->processDeviceInput(dev, axisEventChannel)) break;
                    }
                });
            }
            sleep(5000);
        }
    });

    // 4. Axis event processor coroutine
    coro([]() {
        while (true) {
            auto [event, err] = axisEventChannel->receive();
            if (err) break;
            // TODO: mapping
            // TODO: pass to virtual device manager
            std::cout << "[INPUT] device=" << std::setw(2) << event.deviceId
                      << " axis=" << std::setw(5) << event.axisIndex
                      << " value=" << std::setw(4) << event.value << std::endl;
        }
    });

    // 5. HTTP API server
    startRestApi(8080, deviceManager, &emulationBoards);
}

int main() {
    coro(_main);
    scheduler_start();

    // Cleanup
    for (auto& link : uartLinks) {
        delete link.rpcManager;
        delete link.framer;
        delete link.rpcOutCh;
        delete link.rpcInCh;
        delete link.uartManager;
    }

    std::cout << "Exit" << std::endl;
    return 0;
}
