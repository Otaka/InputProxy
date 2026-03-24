#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include "CoHttpServer.h"
#include "RestApi.h"
#include "../shared/shared.h"
#include "../shared/PicoConfig.h"
#include "../shared/crc32.h"
#include "corocrpc/corocrpc.h"
#include "rpcinterface.h"
#include "UartManager.h"
#include "RealDeviceManager.h"
#include "EmulationBoard.h"
#include "EmulatedDeviceManager.h"
#include "MainboardConfig.h"
#include "MappingManager.h"

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

EmulatedDeviceManager* emulatedDeviceManager = nullptr;
MappingManager* mappingManager = nullptr;
std::vector<BoardEntry> boardConfigs;          // loaded from config.json at startup


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

        // onBoot(string picoId, uint32 configCrc32) → bool success
        rpc->registerMethod(P2M_ON_BOOT, [&link, rpc](RpcArg* arg) -> RpcArg* {
            if (!emulatedDeviceManager) {
                std::cerr << "[UART" << link.channel << "] onBoot called before EmulatedDeviceManager init\n";
                RpcArg* out = rpc->getRpcArg(); out->putBool(false); return out;
            }
            char picoIdBuf[64] = {};
            arg->getString(picoIdBuf, sizeof(picoIdBuf));
            // corocrpc encodes all integers as int32. Read as int32, then reinterpret bits
            // as uint32. Values with high bit set arrive as negative int32 but recover
            // correctly: e.g. 0xFFFFFFFF → -1 → (uint32_t)-1 == 0xFFFFFFFF.
            uint32_t receivedCrc = static_cast<uint32_t>(arg->getInt32());
            std::string picoId(picoIdBuf);

            std::cout << "[UART" << link.channel << "] onBoot picoId=" << picoId
                      << " crc=0x" << std::hex << receivedCrc << std::dec << std::endl;

            // Find this board in loaded configs
            const BoardEntry* entry = nullptr;
            for (const auto& e : boardConfigs)
                if (e.picoId == picoId) { entry = &e; break; }

            if (!entry) {
                std::cout << "[UART" << link.channel << "] unknown picoId=" << picoId
                          << " — registering as active board with no devices" << std::endl;
                EmulationBoard* board = nullptr;
                for (auto& b : emulationBoards)
                    if (b.serialString == picoId) { board = &b; break; }
                if (!board) {
                    EmulationBoard newBoard;
                    newBoard.id           = nextEmulationBoardId++;
                    newBoard.serialString = picoId;
                    newBoard.rpc          = rpc;
                    newBoard.uartChannel  = link.channel;
                    newBoard.active       = true;
                    newBoard.picoConfig   = {};
                    emulationBoards.push_back(std::move(newBoard));
                    board = &emulationBoards.back();
                } else {
                    board->rpc         = rpc;
                    board->uartChannel = link.channel;
                    board->active      = true;
                }
                emulatedDeviceManager->registerBoard(board, {});
                if (mappingManager) mappingManager->onBoardRegistered();
                RpcArg* out = rpc->getRpcArg();
                out->putBool(true);
                return out;
            }

            // Compute CRC of our canonical config
            std::string canonical = serializePicoConfig(entry->config);
            uint32_t expectedCrc  = crc32(canonical.c_str(), canonical.size());

            // Find or create EmulationBoard
            EmulationBoard* board = nullptr;
            for (auto& b : emulationBoards)
                if (b.serialString == picoId) { board = &b; break; }
            if (!board) {
                EmulationBoard newBoard;
                newBoard.id           = nextEmulationBoardId++;
                newBoard.serialString = picoId;
                newBoard.rpc          = rpc;
                newBoard.uartChannel  = link.channel;
                newBoard.active       = false;
                newBoard.picoConfig   = entry->config;
                emulationBoards.push_back(std::move(newBoard));
                board = &emulationBoards.back();
            } else {
                board->rpc         = rpc;
                board->uartChannel = link.channel;
                board->picoConfig  = entry->config;
            }

            if (receivedCrc == expectedCrc) {
                board->active = true;
                auto vdevices = buildVirtualDevices(*entry);
                emulatedDeviceManager->registerBoard(board, vdevices);
                if (mappingManager) mappingManager->onBoardRegistered();
                std::cout << "[UART" << link.channel << "] picoId=" << picoId
                          << " config match — board active" << std::endl;
                RpcArg* out = rpc->getRpcArg();
                out->putBool(true);
                return out;
            }

            // CRC mismatch — push config
            std::cout << "[UART" << link.channel << "] picoId=" << picoId
                      << " CRC mismatch (got 0x" << std::hex << receivedCrc
                      << " expected 0x" << expectedCrc << std::dec
                      << ") — sending setConfiguration" << std::endl;
            std::string errMsg;
            bool ok = board->setConfiguration(canonical, errMsg);
            if (!ok) {
                std::cerr << "[UART" << link.channel << "] setConfiguration rejected: " << errMsg << std::endl;
            }
            RpcArg* out = rpc->getRpcArg();
            out->putBool(false);
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
static std::string resolveAxisName(const std::string& deviceIdStr, int axisIndex) {
    for(auto& [id, dev] : deviceManager->getDevices())
        if(dev.deviceIdStr == deviceIdStr)
            return dev.axes.getName(axisIndex);
    return {};
}

void logRealDeviceEvent(AxisEvent&event) {
    bool printEvent=true;
    bool printAxisStringName=true;
    if(printEvent){
        std::cout<<"Event="<<event.deviceIdStr<<" axisIndex="<<event.axisIndex;
        if(printAxisStringName){
            std::cout<<"("<<resolveAxisName(event.deviceIdStr, event.axisIndex)<<")";
        }
        std::cout<<" value="<<event.value<<std::endl;
    }
}

void _main() {
    std::cout << "=== Raspberry Pi 4 to Pico RPC System ===" << std::endl;

    nlohmann::json configRoot = parseConfigFile("config.json");
    boardConfigs = loadMainboardConfig(configRoot);
    std::cout << "Loaded " << boardConfigs.size() << " emulation board config(s)" << std::endl;
    emulatedDeviceManager = new EmulatedDeviceManager();
    mappingManager = new MappingManager();
    mappingManager->loadFromConfig(configRoot, emulatedDeviceManager);
    // Reserve capacity so push_back never reallocates — EmulationBoard* pointers stored
    // in VirtualOutputDevice::board must remain stable for the process lifetime.
    emulationBoards.reserve(16);

    if (!initRpcSystem()) {
        std::cerr << "Failed to initialize RPC system" << std::endl;
        return;
    }

    std::vector<std::string> duplicateSerialIds = {};
    deviceManager = new RealDeviceManager(duplicateSerialIds);
    deviceManager->loadFromConfig(configRoot);

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
        link.rpcManager->callNoResponse(M2P_REBOOT, arg);
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
                    if (mappingManager) mappingManager->onRealDeviceConnected(dev->deviceIdStr, *dev);
                    while (true) {
                        auto [flags, err] = wait_file(dev->fd, WAIT_IN);
                        if (err || !(flags & WAIT_IN)) {
                            dev->active = false;
                            break;
                        }
                        if (!deviceManager->processDeviceInput(dev, axisEventChannel)) break;
                    }
                    std::cout << "[DISCONNECT] device=" << dev->deviceIdStr << std::endl;
                    if (mappingManager) mappingManager->onRealDeviceDisconnected(dev->deviceIdStr);
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
            logRealDeviceEvent(event);
            if (mappingManager)
                mappingManager->axisEvent(event.deviceIdStr, event.axisIndex, event.value);
        }
    });

    // 5. HTTP API server
    auto reloadConfigFn = []() {
        std::cout << "[config] reloading config.json..." << std::endl;
        nlohmann::json configRoot = parseConfigFile("config.json");
        boardConfigs = loadMainboardConfig(configRoot);

        emulatedDeviceManager->clear();
        mappingManager->clear();
        deviceManager->loadFromConfig(configRoot);
        mappingManager->loadFromConfig(configRoot, emulatedDeviceManager);

        // Re-register currently connected real devices into fresh mapping state
        for (auto& [id, dev] : deviceManager->getDevices())
            if (dev.active)
                mappingManager->onRealDeviceConnected(dev.deviceIdStr, dev);

        // Reboot all Pico boards — they'll re-send onBoot and pick up new config
        for (auto& link : uartLinks) {
            RpcArg* arg = link.rpcManager->getRpcArg();
            link.rpcManager->callNoResponse(M2P_REBOOT, arg);
            link.rpcManager->disposeRpcArg(arg);
        }
        std::cout << "[config] reload complete" << std::endl;
    };

    startRestApi(8080, deviceManager, &emulationBoards, emulatedDeviceManager,
                 &mappingManager->getLayerManager(), reloadConfigFn);
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
