#include <iostream>
#include <iomanip>
#include <fcntl.h>
#include <unistd.h>
#include <thread>
#include <atomic>
#include "httplib.h"

#define RPCMANAGER_STD_STRING
#include "simplerpc/simplerpc.h"
#include "rpcinterface.h"
#include "EventLoop.h"
#include "UartManager.h"
#include "webtester.h"
#include "RealDeviceManager.h"

using namespace simplerpc;

// Global instances
EventLoop eventLoop;
UartManager*uartManager;

// RPC managers for bidirectional communication
static RpcManager<>* rpcManager = nullptr;

// Server implementation for methods Pico can call (ping, debugPrint)
static Pico2Main pico2MainRpcServer;

// Client to call Pico's methods (ping, setLed, getLedStatus)
static Main2Pico main2PicoRpcClient;

// Initialize RPC system
bool initRpcSystem() {
    std::cout << "Initializing RPC system..." << std::endl;
    
    // Check UART device
    if (!uartManager->testHasUartDevice()) {
        std::cerr << "No UART device found at /dev/serial0" << std::endl;
        return false;
    }

    if (!uartManager->configureUart()) {
        std::cerr << "Failed to configure UART interface to Raspberry PICO" << std::endl;
        return false;
    }
    uartManager->flushInput();
    
    // Create RPC manager
    rpcManager = new RpcManager<>();

    // Add filters to RPC manager (handles packet assembly/disassembly)
    rpcManager->addInputFilter(new StreamFramerInputFilter());
    rpcManager->addOutputFilter( new StreamFramerOutputFilter());

    // Set global timeout for all RPC calls (2 seconds)
    rpcManager->setDefaultTimeout(2000);

    // Setup transport layer: send framed data via UART
    rpcManager->setOnSendCallback([](const char* data, int len) {
        if (data && len > 0) {
            uartManager->uartSend(data, len);
        }
    });

    // Setup error callback
    rpcManager->onError([](int code, const char* msg) {
        std::cerr << "[MAIN RPC ERROR " << code << "] " << (msg ? msg : "") << std::endl;
    });

    // Setup receive path: UART -> RPC
    eventLoop.addFileDescriptor(uartManager->getUartFd(), 
        [](int fd) {
            char buffer[2048];
            ssize_t bytesRead = uartManager->uartRead(buffer, sizeof(buffer));
            // Forward raw UART data to RPC manager (input filter handles deframing)
            if (bytesRead > 0 && rpcManager) {
                rpcManager->processInput(buffer, bytesRead);
            }
        },
        [](int fd) {
            std::cerr << "UART device disconnected!" << std::endl;
        }
    );
    
    // ---------------------------------------------
    // Register SERVER: methods that Pico can call
    // ---------------------------------------------
    pico2MainRpcServer.ping = [](int val) -> int {
        std::cout << "[MAIN SERVER] ping("<< val <<") called from Pico" << std::endl;
        return val;
    };
    
    pico2MainRpcServer.debugPrint = [](std::string value) {
        std::cout << "[PICO LOG]" << value << std::endl;
    };

    pico2MainRpcServer.onBoot = [](std::string deviceId) -> bool {
        std::cout << "[MAIN SERVER] Pico booted with Device ID: " << deviceId << std::endl;
        return true;
    };

    rpcManager->registerServer(pico2MainRpcServer);
    std::cout << "Pico2Main server registered" << std::endl;
    
    // ---------------------------------------------
    // Create CLIENT: to call Pico's methods
    // ---------------------------------------------
    main2PicoRpcClient = rpcManager->createClient<Main2Pico>();
    std::cout << "Main2Pico client created" << std::endl;
    
    std::cout << "RPC system initialized successfully!" << std::endl;
    return true;
}

void runEventLoop() {
    std::cout << "Starting event loop..." << std::endl;
    // Run the event loop (blocks)
    eventLoop.runLoop();
}

int main() {
    uartManager=new UartManager(UART2);
    if(!uartManager->testHasUartDevice()){
        std::cout << "Cannot open uart" << std::endl;    
        return 1;
    }
    std::cout << "=== Raspberry Pi 4 to Pico RPC System ===" << std::endl;

    if (!initRpcSystem()) {
        std::cerr << "Failed to initialize RPC system" << std::endl;
        return 1;
    }

    // Initialize Real Device Manager
    std::vector<std::string> duplicateSerialIds = {
        // Add any problematic device IDs here, e.g.:
        // "0x045e:0x028e:0000" // Example: Xbox 360 controller with duplicate serial
    };

    RealDeviceManager deviceManager(
        eventLoop,
        duplicateSerialIds,
        // onDeviceConnect callback
        [](RealDevice* device) {
            // std::cout << "[MAIN] Device connected: " << device->deviceId
            //           << " (" << device->deviceName << ")" << std::endl;
            // std::cout << "       Vendor: 0x" << std::hex << device->vendorId
            //           << ", Product: 0x" << device->productId << std::dec << std::endl;
            // std::cout << "       Capabilities: " << device->axisName2Index.size() << " inputs" << std::endl;
        },
        // onDeviceDisconnect callback
        [](RealDevice* device) {
            std::cout << "[MAIN] Device disconnected: " << device->deviceId << std::endl;
        },
        // onInput callback
        [&deviceManager](const std::string& deviceId, int axisIndex, int value) {
            RealDevice*device=&(deviceManager.deviceId2Device[deviceId]);
            std::string name=device->axisIndex2Name[axisIndex];
            // TODO: Map and send to virtual device via UART
            std::cout << "[MAIN] Input from " << deviceId
                      << " - axis: " << axisIndex <<" name: "<<name << " value: "<<std::setw(4) << value << std::endl;
        }
    );

    // Start web tester interface
    initWebserver(main2PicoRpcClient);

    runEventLoop();
    
    // Cleanup
    rpcManager->deregisterServer<Pico2Main>();
    delete rpcManager;
    
    std::cout << "Exit" << std::endl;
    return 0;
}