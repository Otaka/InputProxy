#include <iostream>
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

using namespace simplerpc;

// Global instances
EventLoop eventLoop;
UartManager uartManager;

// RPC managers for bidirectional communication
static RpcManager<>* rpcManager = nullptr;

// Server implementation for methods Pico can call (ping, debugPrint)
static Pico2Main mainRpcServer;

// Client to call Pico's methods (ping, setLed, getLedStatus)
static Main2Pico picoRpcClient;

// Initialize RPC system
bool initRpcSystem() {
    std::cout << "Initializing RPC system..." << std::endl;
    
    // Check UART device
    if (!uartManager.testHasUartDevice()) {
        std::cerr << "No UART device found at /dev/serial0" << std::endl;
        return false;
    }

    if (!uartManager.configureUart()) {
        std::cerr << "Failed to configure UART interface to Raspberry PICO" << std::endl;
        return false;
    }
    uartManager.flushInput();
    
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
            uartManager.uartSend(data, len);
        }
    });
    
    // Setup error callback
    rpcManager->onError([](int code, const char* msg) {
        std::cerr << "[MAIN RPC ERROR " << code << "] " << (msg ? msg : "") << std::endl;
    });
    
    // Setup receive path: UART -> RPC
    eventLoop.addFileDescriptor(uartManager.getUartFd(), 
        [](int fd) {
            char buffer[2048];
            ssize_t bytesRead = uartManager.uartRead(buffer, sizeof(buffer));
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
    mainRpcServer.ping = [](int val) -> int {
        std::cout << "[MAIN SERVER] ping("<< val <<") called from Pico" << std::endl;
        return val;
    };
    
    mainRpcServer.debugPrint = [](std::string value) {
        std::cout << "[PICO LOG]" << value << std::endl;
    };
    
    rpcManager->registerServer(mainRpcServer);
    std::cout << "Pico2Main server registered" << std::endl;
    
    // ---------------------------------------------
    // Create CLIENT: to call Pico's methods
    // ---------------------------------------------
    picoRpcClient = rpcManager->createClient<Main2Pico>();
    std::cout << "Main2Pico client created" << std::endl;
    
    std::cout << "RPC system initialized successfully!" << std::endl;
    return true;
}

void runEventLoop() {
    std::cout << "Starting event loop..." << std::endl;
    
    //Example: Test RPC calls to Pico in a separate thread
    std::thread testThread([]() {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        std::cout << "\n=== Testing RPC calls to Pico ===" << std::endl;
        int pingCounter=0;
        try {
            for(;;) {
                // Test ping to Pico
                // int pong = picoRpcClient.ping(pingCounter++);
                // std::cout << "[MAIN->PICO] ping(" << (pingCounter-1) << ") -> pong(" << pong << ")" << std::endl;
                
                // Toggle LED
                //picoRpcClient.setLed(true);
                //std::this_thread::sleep_for(std::chrono::seconds(1));
                
                //bool ledStatus = picoRpcClient.getLedStatus();
                //std::cout << "[MAIN] Pico LED status: " << (ledStatus ? "ON" : "OFF") << std::endl;
                
                //picoRpcClient.setLed(false);
                //std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        } catch (const std::exception& e) {
            std::cerr << "[MAIN CLIENT] Exception: " << e.what() << std::endl;
        }
    });
    testThread.detach();
    
    // Run the event loop (blocks)
    eventLoop.runLoop();
}

int main() {
    std::cout << "=== Raspberry Pi 4 to Pico RPC System ===" << std::endl;
    
    if (!initRpcSystem()) {
        std::cerr << "Failed to initialize RPC system" << std::endl;
        return 1;
    }

    // Start web tester interface
    initWebserver(picoRpcClient);
    
    runEventLoop();
    
    // Cleanup
    rpcManager->deregisterServer<Pico2Main>();
    delete rpcManager;
    
    std::cout << "Exit" << std::endl;
    return 0;
}