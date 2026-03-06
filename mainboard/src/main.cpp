#include <iostream>
#include <iomanip>
#include <fcntl.h>
#include <unistd.h>
#include "CoHttpServer.h"
#include "RestApi.h"
#include "../shared/shared.h"
#define RPCMANAGER_STD_STRING
#include "simplerpc/simplerpc.h"
#include "rpcinterface.h"
#include "UartManager.h"
#include "RealDeviceManager.h"
#include "EmulationBoard.h"

using namespace simplerpc;
using namespace corocgo;

// Per-UART link: bundles UartManager + RPC stack for one Pico connection
struct UartRpcLink {
    UART_CHANNEL channel;
    UartManager* uartManager;
    RpcManager<>* rpcManager;
    Pico2Main pico2MainServer;
    Main2Pico main2PicoClient;
};

std::vector<UartRpcLink> uartLinks;

// Emulation boards (Pico devices connected via UART)
std::vector<EmulationBoard> emulationBoards;
int nextEmulationBoardId = 1;

// Channel for axis events from real devices
Channel<AxisEvent>* axisEventChannel;
RealDeviceManager*deviceManager;

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
        link.channel = ch;
        link.uartManager = uart;
        link.rpcManager = nullptr;
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
        auto* rpc = new RpcManager<>();
        rpc->addInputFilter(new StreamFramerInputFilter());
        rpc->addOutputFilter(new StreamFramerOutputFilter());
        rpc->setDefaultTimeout(2000);

        UartManager* uart = link.uartManager;
        rpc->setOnSendCallback([uart](const char* data, int len) {
            if (data && len > 0) uart->uartSend(data, len);
        });

        UART_CHANNEL ch = link.channel;
        rpc->onError([ch](int code, const char* msg) {
            std::cerr << "[RPC ERROR UART" << ch << " code=" << code << "] "
                      << (msg ? msg : "") << std::endl;
        });

        // Server: methods Pico can call on this UART
        link.pico2MainServer.ping = [ch](int val) -> int {
            std::cout << "[UART" << ch << "] ping(" << val << ") from Pico" << std::endl;
            return val;
        };
        link.pico2MainServer.debugPrint = [ch](std::string value) {
            std::cout << "[UART" << ch << " LOG] " << value << std::endl;
        };
        link.pico2MainServer.onBoot = [&link](std::string serialString, int deviceModeInt) -> bool {
            std::string deviceModeString=deviceModeInt==HID_MODE?"HID_MODE":"XINPUT_MODE";

            std::cout << "[UART" << link.channel << "] Pico booted with Serial: "
                      << serialString << " in mode "<<deviceModeString<< std::endl;

            // Check if we already have this board by serialString
            for (auto& board : emulationBoards) {
                if (board.serialString == serialString) {
                    board.active = true;
                    board.main2PicoRpcClient = link.main2PicoClient;
                    board.uartChannel = link.channel;
                    std::cout << "[UART" << link.channel
                              << "] Reactivated EmulationBoard id=" << board.id << std::endl;
                    return true;
                }
            }

            // New board — register it
            EmulationBoard board;
            board.id = nextEmulationBoardId++;
            board.serialString = serialString;
            board.main2PicoRpcClient = link.main2PicoClient;
            board.uartChannel = link.channel;
            board.active = true;
            emulationBoards.push_back(std::move(board));
            std::cout << "[UART" << link.channel << "] Registered new EmulationBoard id="
                      << (nextEmulationBoardId - 1) << " serial=" << serialString << std::endl;
            return true;
        };

        rpc->registerServer(link.pico2MainServer);
        link.main2PicoClient = rpc->createClient<Main2Pico>();
        link.rpcManager = rpc;

        // Tell the Pico to reboot so it announces itself via onBoot
        std::cout << "UART" << ch << ": sending reboot to Pico..." << std::endl;
        link.main2PicoClient.reboot();
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
    deviceManager=new RealDeviceManager(duplicateSerialIds);

    axisEventChannel = makeChannel<AxisEvent>(64);

    // 1. UART → RPC coroutines (one per detected channel)
    for (auto& link : uartLinks) {
        coro([&link]() {
            int fd = link.uartManager->getUartFd();
            char buffer[2048];
            while (true) {
                auto [flags, err] = wait_file(fd, WAIT_IN);
                if (err) break;
                ssize_t n = link.uartManager->uartRead(buffer, sizeof(buffer));
                if (n > 0)
                    link.rpcManager->processInput(buffer, n);
            }
        });
    }

    // 2. Device discovery coroutine — loops every 5 seconds
    coro([]() {
        while (true) {
            auto paths = deviceManager->scanDevices();
            for (auto& path : paths) {
                RealDevice* dev = deviceManager->registerDevice(path);
                if (!dev) continue;
                // Spawn one reading coroutine per device
                coro([dev]() {
                    std::cout << "[CONNECT] device=" << dev->deviceIdStr<<std::endl;
                    while (true) {
                        auto [flags, err] = wait_file(dev->fd, WAIT_IN);
                        if (err || !(flags & WAIT_IN)) {
                            dev->active = false;
                            std::cout << "[DISCONNECT] device=" << dev->deviceIdStr<<std::endl;
                            break;
                        }
                        if (!deviceManager->processDeviceInput(dev, axisEventChannel)) break;
                    }
                });
            }
            sleep(5000);
        }
    });

    // 3. Axis event processor coroutine
    coro([]() {
        while (true) {
            auto [event, err] = axisEventChannel->receive();
            if (err) break;
            // TODO: mapping
            // TODO: pass to virtual device manager
            std::cout << "[INPUT] device="<< std::setw(2) << event.deviceId
                      << " axis=" << std::setw(5) << event.axisIndex
                      << " value=" << std::setw(4) << event.value << std::endl;
        }
    });

    // 4. HTTP API server
    startRestApi(8080, deviceManager);
}

int main() {
    coro(_main);
    scheduler_start();

    // Cleanup
    for (auto& link : uartLinks) {
        link.rpcManager->deregisterServer<Pico2Main>();
        delete link.rpcManager;
        delete link.uartManager;
    }

    std::cout << "Exit" << std::endl;
    return 0;
}