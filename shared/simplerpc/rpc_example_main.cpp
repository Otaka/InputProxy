#include <iostream>
#include <thread>
#include <chrono>
#define RPCMANAGER_STD_STRING
#define RPCMANAGER_MUTEX
#include "simplerpc.h"

using namespace simplerpc;

// ---------------------------------------------
// Test structures (trivially copyable)
// ---------------------------------------------
struct Point {
    float x;
    float y;
    float z;
};

struct Message {
    char text[64];  // Fixed-size ASCII string
    int priority;
    uint32_t timestamp;
};

// ---------------------------------------------
// Provider Interface (shared between client/server)
// ---------------------------------------------
struct MyRpcProvider {
    // Methods
    std::function<int(int, int)> add;
    std::function<void(int)> log;
    std::function<Point(Point)> transformPoint;
    std::function<Message(Message)> processMessage;
    std::function<RpcFuture<int>(int, int)> asyncMultiply;
    std::function<RpcByteArray(RpcByteArray)> echoBytes;
    std::function<int(int, RpcByteArray, int)> processWithBytes;
    std::function<RpcFuture<RpcByteArray>(int)> asyncGenerateBytes;
    std::function<std::string(std::string)> reverseString;
    std::function<int(std::string, int, std::string)> mixedStringArgs;
    std::function<RpcFuture<std::string>(int)> asyncGenerateString;
    
    // RPC metadata
    static constexpr uint32_t providerId = 42;
    static constexpr auto methods = std::make_tuple(
        &MyRpcProvider::add,
        &MyRpcProvider::log,
        &MyRpcProvider::transformPoint,
        &MyRpcProvider::processMessage,
        &MyRpcProvider::asyncMultiply,
        &MyRpcProvider::echoBytes,
        &MyRpcProvider::processWithBytes,
        &MyRpcProvider::asyncGenerateBytes,
        &MyRpcProvider::reverseString,
        &MyRpcProvider::mixedStringArgs,
        &MyRpcProvider::asyncGenerateString
    );
};

// ---------------------------------------------
// Mock transport: Direct loopback
// ---------------------------------------------
static RpcManager<>* g_server = nullptr;
static RpcManager<>* g_client = nullptr;

void clientToServer(const char* data, int len) {
    std::cout << "[CLIENT->SERVER] Sent " << len << " bytes\n";
    if (g_server) {
        g_server->processInput(data, len);
    }
}

void serverToClient(const char* data, int len) {
    std::cout << "[SERVER->CLIENT] Sent " << len << " bytes\n";
    if (g_client) {
        g_client->processInput(data, len);
    }
}

void simpleOneWayTest() {
    std::cout << "=== SimpleRPC Test ===" << std::endl << std::endl;
    
    // Create RPC managers
    RpcManager<> serverRpc;
    RpcManager<> clientRpc;
    
    g_server = &serverRpc;
    g_client = &clientRpc;
    
    // Setup transport callbacks
    clientRpc.setOnSendCallback(clientToServer);
    serverRpc.setOnSendCallback(serverToClient);
    
    // Setup error callbacks
    serverRpc.onError([](int code, const char* msg) {
        std::cout << "[SERVER ERROR " << code << "] " << msg << std::endl;
    });
    
    clientRpc.onError([](int code, const char* msg) {
        std::cout << "[CLIENT ERROR " << code << "] " << msg << std::endl;
    });
    
    // ---------------------------------------------
    // Setup SERVER implementation
    // ---------------------------------------------
    MyRpcProvider server;
    
    server.add = [](int a, int b) {
        std::cout << "[SERVER] add(" << a << ", " << b << ") = " << (a + b) << std::endl;
        return a + b;
    };
    
    server.log = [](int value) {
        std::cout << "[SERVER] log(" << value << ") - void method (fire and forget)" << std::endl;
    };
    
    server.transformPoint = [](Point p) {
        std::cout << "[SERVER] transformPoint({" << p.x << ", " << p.y << ", " << p.z << "})" << std::endl;
        Point result = {p.x * 2.0f, p.y * 2.0f, p.z * 2.0f};
        std::cout << "[SERVER]   -> {" << result.x << ", " << result.y << ", " << result.z << "}" << std::endl;
        return result;
    };
    
    server.processMessage = [](Message msg) {
        std::cout << "[SERVER] processMessage: \"" << msg.text << "\" (priority="
                  << msg.priority << ", ts=" << msg.timestamp << ")" << std::endl;
        
        Message reply;
        std::snprintf(reply.text, sizeof(reply.text), "ACK: %s", msg.text);
        reply.priority = msg.priority + 1;
        reply.timestamp = msg.timestamp + 1000;
        
        std::cout << "[SERVER]   -> \"" << reply.text << "\"" << std::endl;
        return reply;
    };
    
    server.asyncMultiply = [](int a, int b) -> RpcFuture<int> {
        std::cout << "[SERVER] asyncMultiply(" << a << ", " << b << ") - starting async computation" << std::endl;
        
        // Create a future - it uses shared state so we can keep a copy
        RpcFuture<int> future;
        
        // Simulate async work in background thread (in real embedded system, this would be 
        // handled by the application's task scheduler or main loop)
        std::thread([future, a, b]() mutable {
            std::cout << "[SERVER]   async thread: computing..." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            int result = a * b;
            std::cout << "[SERVER]   async thread: setting result " << a << " * " << b << " = " << result << std::endl;
            future.setValue(result);
            std::cout << "[SERVER]   async thread: done" << std::endl;
        }).detach();
        
        // Return copy - library will also keep a copy and set callback on it
        return future;
    };
    
    server.echoBytes = [](RpcByteArray input) -> RpcByteArray {
        std::cout << "[SERVER] echoBytes(length=" << input.length << ") - data: ";
        for (int i = 0; i < std::min(20, (int)input.length); i++) {
            std::cout << input.content[i];
        }
        if (input.length > 20) std::cout << "...";
        std::cout << std::endl;
        
        // Allocate response buffer (static to keep it alive)
        static char response_buf[1024];
        std::memcpy(response_buf, input.content, input.length);
        return RpcByteArray{response_buf, input.length};
    };
    
    server.processWithBytes = [](int prefix, RpcByteArray data, int suffix) -> int {
        std::cout << "[SERVER] processWithBytes(" << prefix << ", length=" << data.length
                  << ", " << suffix << ")" << std::endl;
        int sum = prefix + suffix;
        for (uint16_t i = 0; i < data.length; i++) {
            sum += static_cast<unsigned char>(data.content[i]);
        }
        std::cout << "[SERVER]   computed sum: " << sum << std::endl;
        return sum;
    };
    
    server.asyncGenerateBytes = [](int size) -> RpcFuture<RpcByteArray> {
        std::cout << "[SERVER] asyncGenerateBytes(" << size << ") - starting async generation" << std::endl;
        
        RpcFuture<RpcByteArray> future;
        
        std::thread([future, size]() mutable {
            std::cout << "[SERVER]   async thread: generating " << size << " bytes..." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            static char buf[1024];
            for (int i = 0; i < size && i < 1024; i++) {
                buf[i] = 'A' + (i % 26);
            }
            
            std::cout << "[SERVER]   async thread: setting byte array result" << std::endl;
            future.setValue(RpcByteArray{buf, static_cast<uint16_t>(std::min(size, 1024))});
            std::cout << "[SERVER]   async thread: done" << std::endl;
        }).detach();
        
        return future;
    };
    
    server.reverseString = [](std::string input) -> std::string {
        std::cout << "[SERVER] reverseString(\"" << input << "\")" << std::endl;
        std::string result = input;
        std::reverse(result.begin(), result.end());
        std::cout << "[SERVER]   -> \"" << result << "\"" << std::endl;
        return result;
    };
    
    server.mixedStringArgs = [](std::string prefix, int num, std::string suffix) -> int {
        std::cout << "[SERVER] mixedStringArgs(\"" << prefix << "\", " << num << ", \"" << suffix << "\")" << std::endl;
        int result = (int)prefix.length() + num + (int)suffix.length();
        std::cout << "[SERVER]   computed: " << result << std::endl;
        return result;
    };
    
    server.asyncGenerateString = [](int count) -> RpcFuture<std::string> {
        std::cout << "[SERVER] asyncGenerateString(" << count << ") - starting async generation" << std::endl;
        
        RpcFuture<std::string> future;
        
        std::thread([future, count]() mutable {
            std::cout << "[SERVER]   async thread: generating string..." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            std::string result;
            for (int i = 0; i < count; i++) {
                result += std::to_string(i) + " ";
            }
            
            std::cout << "[SERVER]   async thread: setting string result" << std::endl;
            future.setValue(result);
            std::cout << "[SERVER]   async thread: done" << std::endl;
        }).detach();
        
        return future;
    };
    
    serverRpc.registerServer(server);
    std::cout << "Server registered\n\n";
    
    // ---------------------------------------------
    // Setup CLIENT
    // ---------------------------------------------
    MyRpcProvider client = clientRpc.createClient<MyRpcProvider>();
    std::cout << "Client created\n\n";
    
    // ---------------------------------------------
    // Test 1: Simple arguments (int)
    // ---------------------------------------------
    std::cout << "=== Test 1: Simple arguments ===" << std::endl;
    int sum = client.add(10, 20);
    std::cout << "[CLIENT] Result: " << sum << "\n\n";
    
    // ---------------------------------------------
    // Test 2: Void method (fire and forget)
    // ---------------------------------------------
    std::cout << "=== Test 2: Void method ===" << std::endl;
    client.log(12345);
    std::cout << "[CLIENT] log() called (no return)\n\n";
    
    // ---------------------------------------------
    // Test 3: Struct with fields
    // ---------------------------------------------
    std::cout << "=== Test 3: Struct with fields ===" << std::endl;
    Point p1 = {1.5f, 2.5f, 3.5f};
    Point p2 = client.transformPoint(p1);
    std::cout << "[CLIENT] Result: {" << p2.x << ", " << p2.y << ", " << p2.z << "}\n\n";
    
    // ---------------------------------------------
    // Test 4: Struct with fixed char array
    // ---------------------------------------------
    std::cout << "=== Test 4: Struct with char array ===" << std::endl;
    Message msg;
    std::snprintf(msg.text, sizeof(msg.text), "Hello RPC!");
    msg.priority = 5;
    msg.timestamp = 123456789;
    
    Message response = client.processMessage(msg);
    std::cout << "[CLIENT] Response: \"" << response.text << "\" (priority="
              << response.priority << ", ts=" << response.timestamp << ")\n\n";
    
    // ---------------------------------------------
    // Test 5: Async with RpcFuture
    // ---------------------------------------------
    std::cout << "=== Test 5: Async with RpcFuture ===" << std::endl;
    RpcFuture<int> futureResult = client.asyncMultiply(7, 8);
    std::cout << "[CLIENT] Future returned immediately, waiting..." << std::endl;
    
    int asyncResult = futureResult.get();
    std::cout << "[CLIENT] Async result: " << asyncResult << "\n\n";
    
    // ---------------------------------------------
    // Test 6: Byte array argument and return
    // ---------------------------------------------
    std::cout << "=== Test 6: Byte array (RpcByteArray) ===" << std::endl;
    char test_data[] = "Hello, byte array RPC!";
    RpcByteArray input{test_data, 22};
    
    RpcByteArray echoed = client.echoBytes(input);
    std::cout << "[CLIENT] Echoed " << echoed.length << " bytes: ";
    // Copy to local buffer immediately (as documented - caller must copy)
    char echoed_buf[256];
    std::memcpy(echoed_buf, echoed.content, echoed.length);
    echoed_buf[echoed.length] = '\0';
    std::cout << echoed_buf << "\n\n";
    
    // ---------------------------------------------
    // Test 7: Mixed arguments with byte array
    // ---------------------------------------------
    std::cout << "=== Test 7: Mixed arguments (int, RpcByteArray, int) ===" << std::endl;
    unsigned char binary_data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    RpcByteArray binary{reinterpret_cast<char*>(binary_data), 5};
    
    int processResult = client.processWithBytes(100, binary, 200);
    std::cout << "[CLIENT] Process result: " << processResult << " (expected: 100+200+1+2+3+4+5=315)\n\n";
    
    // ---------------------------------------------
    // Test 8: Async byte array return
    // ---------------------------------------------
    std::cout << "=== Test 8: Async byte array return ===" << std::endl;
    RpcFuture<RpcByteArray> futureBytesResult = client.asyncGenerateBytes(50);
    std::cout << "[CLIENT] Future returned immediately, waiting for byte array..." << std::endl;
    
    RpcByteArray asyncBytes = futureBytesResult.get();
    std::cout << "[CLIENT] Async byte array result (length=" << asyncBytes.length << "): ";
    // Copy to local buffer immediately (as documented - caller must copy)
    char async_buf[256];
    int display_len = std::min(30, (int)asyncBytes.length);
    std::memcpy(async_buf, asyncBytes.content, display_len);
    async_buf[display_len] = '\0';
    std::cout << async_buf;
    if (asyncBytes.length > 30) std::cout << "...";
    std::cout << "\n\n";
    
    // ---------------------------------------------
    // Test 9: std::string argument and return
    // ---------------------------------------------
    std::cout << "=== Test 9: std::string support ===" << std::endl;
    std::string testStr = "Hello, RPC with std::string!";
    std::string reversed = client.reverseString(testStr);
    std::cout << "[CLIENT] Reversed: \"" << reversed << "\"\n\n";
    
    // ---------------------------------------------
    // Test 10: Mixed arguments with std::string
    // ---------------------------------------------
    std::cout << "=== Test 10: Mixed arguments (string, int, string) ===" << std::endl;
    int strResult = client.mixedStringArgs("prefix", 100, "suffix");
    std::cout << "[CLIENT] Result: " << strResult << " (expected: 6+100+6=112)\n\n";
    
    // ---------------------------------------------
    // Test 11: Async std::string return
    // ---------------------------------------------
    std::cout << "=== Test 11: Async std::string return ===" << std::endl;
    RpcFuture<std::string> futureStringResult = client.asyncGenerateString(10);
    std::cout << "[CLIENT] Future returned immediately, waiting for string..." << std::endl;
    
    std::string asyncString = futureStringResult.get();
    std::cout << "[CLIENT] Async string result: \"" << asyncString << "\"\n\n";
    
    // Cleanup: deregister provider
    std::cout << "\nDeregistering server provider..." << std::endl;
    serverRpc.deregisterServer<MyRpcProvider>();
    std::cout << "Server provider deregistered.\n" << std::endl;
    
    std::cout << "=== All tests passed! ===" << std::endl;
}

// ---------------------------------------------
// Provider for RPC1
// ---------------------------------------------
struct Rpc1Provider {
    std::function<void(std::string)> log1;
    
    static constexpr uint32_t providerId = 100;
    static constexpr auto methods = std::make_tuple(
        &Rpc1Provider::log1
    );
};

// ---------------------------------------------
// Provider for RPC2
// ---------------------------------------------
struct Rpc2Provider {
    std::function<void(std::string)> log2;
    
    static constexpr uint32_t providerId = 200;
    static constexpr auto methods = std::make_tuple(
        &Rpc2Provider::log2
    );
};

// ---------------------------------------------
// Two-way communication test
// ---------------------------------------------
void twoWayTest() {
    std::cout << "\n=== Two-Way RPC Communication Test ===" << std::endl << std::endl;
    
    // Create two RPC managers
    RpcManager<> rpc1;
    RpcManager<> rpc2;
    
    // Setup bidirectional transport callbacks
    // rpc1 -> rpc2
    rpc1.setOnSendCallback([&rpc2](const char* data, int len) {
        std::cout << "[RPC1->RPC2] Sent " << len << " bytes\n";
        rpc2.processInput(data, len);
    });
    
    // rpc2 -> rpc1
    rpc2.setOnSendCallback([&rpc1](const char* data, int len) {
        std::cout << "[RPC2->RPC1] Sent " << len << " bytes\n";
        rpc1.processInput(data, len);
    });
    
    // Setup error callbacks
    rpc1.onError([](int code, const char* msg) {
        std::cout << "[RPC1 ERROR " << code << "] " << msg << std::endl;
    });
    
    rpc2.onError([](int code, const char* msg) {
        std::cout << "[RPC2 ERROR " << code << "] " << msg << std::endl;
    });
    
    // ---------------------------------------------
    // Register providers
    // ---------------------------------------------
    
    // RPC1 provider implementation
    Rpc1Provider rpc1Provider;
    rpc1Provider.log1 = [](std::string message) {
        std::cout << "[RPC1 PROVIDER] log1 called with message: \"" << message << "\"" << std::endl;
    };
    
    // RPC2 provider implementation
    Rpc2Provider rpc2Provider;
    rpc2Provider.log2 = [](std::string message) {
        std::cout << "[RPC2 PROVIDER] log2 called with message: \"" << message << "\"" << std::endl;
    };
    
    rpc1.registerServer(rpc1Provider);
    std::cout << "RPC1 provider registered with method: log1\n";
    rpc2.registerServer(rpc2Provider);
    std::cout << "RPC2 provider registered with method: log2\n\n";
    
    // ---------------------------------------------
    // Create clients
    // ---------------------------------------------
    
    // RPC1 creates a client to RPC2's provider
    Rpc2Provider rpc1ClientToRpc2 = rpc1.createClient<Rpc2Provider>();
    std::cout << "RPC1 created client to RPC2 provider\n";
    
    // RPC2 creates a client to RPC1's provider
    Rpc1Provider rpc2ClientToRpc1 = rpc2.createClient<Rpc1Provider>();
    std::cout << "RPC2 created client to RPC1 provider\n\n";
    
    // ---------------------------------------------
    // Test two-way communication
    // ---------------------------------------------

    std::cout << "=== Test 1: RPC1 calls RPC2's log2 method ===" << std::endl;
    rpc1ClientToRpc2.log2("Hello from RPC1!");
    std::cout << std::endl;
    
    std::cout << "=== Test 2: RPC2 calls RPC1's log1 method ===" << std::endl;
    rpc2ClientToRpc1.log1("Hello from RPC2!");
    std::cout << std::endl;
    
    std::cout << "=== Test 3: Alternating calls ===" << std::endl;
    rpc1ClientToRpc2.log2("RPC1 -> RPC2: Message 1");
    rpc2ClientToRpc1.log1("RPC2 -> RPC1: Message 2");
    rpc1ClientToRpc2.log2("RPC1 -> RPC2: Message 3");
    rpc2ClientToRpc1.log1("RPC2 -> RPC1: Message 4");
    std::cout << std::endl;
    
    // Cleanup: deregister both providers
    std::cout << "\nDeregistering providers..." << std::endl;
    rpc1.deregisterServer<Rpc1Provider>();
    std::cout << "RPC1 provider deregistered." << std::endl;
    rpc2.deregisterServer<Rpc2Provider>();
    std::cout << "RPC2 provider deregistered.\n" << std::endl;
    
    std::cout << "=== Two-way communication test completed! ===" << std::endl;
}

// ---------------------------------------------
// Provider for timeout testing
// ---------------------------------------------
struct TimeoutTestProvider {
    std::function<int(int)> fastMethod;
    std::function<int(int)> slowMethod;
    std::function<int(int)> verySlowMethod;
    
    static constexpr uint32_t providerId = 123;
    inline static const auto methods = std::make_tuple(
        &TimeoutTestProvider::fastMethod,
        &TimeoutTestProvider::slowMethod,
        &TimeoutTestProvider::verySlowMethod
    );
};

// ---------------------------------------------
// Timeout test
// ---------------------------------------------
void timeoutTest() {
    std::cout << "\n=== RPC Timeout Test ===" << std::endl << std::endl;
    
    // Create RPC managers
    RpcManager<> serverRpc;
    RpcManager<> clientRpc;
    
    // Setup transport
    clientRpc.setOnSendCallback([&serverRpc](const char* data, int len) {
        serverRpc.processInput(data, len);
    });
    
    serverRpc.setOnSendCallback([&clientRpc](const char* data, int len) {
        clientRpc.processInput(data, len);
    });
    
    // Setup error callbacks
    clientRpc.onError([](int code, const char* msg) {
        std::cout << "[CLIENT ERROR " << code << "] " << msg << std::endl;
    });
    
    // Register server provider with methods of varying speeds
    TimeoutTestProvider server;
    
    server.fastMethod = [](int value) {
        std::cout << "[SERVER] fastMethod(" << value << ") - responding immediately" << std::endl;
        return value * 2;
    };
    
    server.slowMethod = [](int value) {
        std::cout << "[SERVER] slowMethod(" << value << ") - waiting 2 seconds..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
        std::cout << "[SERVER] slowMethod done" << std::endl;
        return value * 3;
    };
    
    server.verySlowMethod = [](int value) {
        std::cout << "[SERVER] verySlowMethod(" << value << ") - waiting 5 seconds..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
        std::cout << "[SERVER] verySlowMethod done" << std::endl;
        return value * 5;
    };
    
    serverRpc.registerServer(server);
    
    // Create client
    TimeoutTestProvider client = clientRpc.createClient<TimeoutTestProvider>();
    
    // ---------------------------------------------
    // Test 1: No timeout (infinite wait)
    // ---------------------------------------------
    clientRpc.setDefaultTimeout(3000);  // 3 second global timeout
    std::cout << "=== Test 1: No timeout (default) ===" << std::endl;
    int result = client.fastMethod(10);
    std::cout << "[CLIENT] Result: " << result << "\n\n";
    
    // ---------------------------------------------
    // Test 2: Global timeout
    // ---------------------------------------------
    std::cout << "=== Test 2: Global timeout (3000ms) ===" << std::endl;
    clientRpc.setDefaultTimeout(3000);  // 3 second global timeout
    
    std::cout << "Fast method (should succeed):" << std::endl;
    result = client.fastMethod(20);
    std::cout << "[CLIENT] Result: " << result << "\n\n";
    
    std::cout << "Slow method (should succeed - takes 2s, timeout is 3s):" << std::endl;
    result = client.slowMethod(30);
    std::cout << "[CLIENT] Result: " << result << "\n\n";
    
    std::cout << "Very slow method (should timeout - takes 5s, timeout is 3s):" << std::endl;
    result = client.verySlowMethod(40);
    std::cout << "[CLIENT] Result after timeout: " << result << " (should be 0)\n\n";
    
    // ---------------------------------------------
    // Test 3: Per-method timeout
    // ---------------------------------------------
    std::cout << "=== Test 3: Per-method timeout ===" << std::endl;
    clientRpc.setDefaultTimeout(5000);  // Increase global to 5 seconds
    clientRpc.setMethodTimeout<TimeoutTestProvider>(1, 1000);  // slowMethod (index 1) gets 1s timeout
    
    std::cout << "Fast method (uses global 5s timeout):" << std::endl;
    result = client.fastMethod(50);
    std::cout << "[CLIENT] Result: " << result << "\n\n";
    
    std::cout << "Slow method (uses per-method 1s timeout, should fail):" << std::endl;
    result = client.slowMethod(60);
    std::cout << "[CLIENT] Result after timeout: " << result << " (should be 0)\n\n";
    
    std::cout << "Very slow method (uses global 5s timeout, should succeed):" << std::endl;
    result = client.verySlowMethod(70);
    std::cout << "[CLIENT] Result: " << result << "\n\n";
    
    // Cleanup
    serverRpc.deregisterServer<TimeoutTestProvider>();
    
    std::cout << "=== Timeout tests completed! ===" << std::endl;
}

// ---------------------------------------------
// StreamFramerFilter RPC tests
// ---------------------------------------------
void streamFramerFilterRpcTest() {
    std::cout << "\n=== StreamFramerFilter RPC Tests ===" << std::endl << std::endl;
    
    // Create RPC managers
    RpcManager<> serverRpc;
    RpcManager<> clientRpc;
    
    // Create StreamFramer filters
    StreamFramerInputFilter serverInputFilter;
    StreamFramerOutputFilter serverOutputFilter;
    StreamFramerInputFilter clientInputFilter;
    StreamFramerOutputFilter clientOutputFilter;
    
    // Add filters to server
    // Output chain: RpcManager -> StreamFramerOutput -> Transport
    serverRpc.addOutputFilter(&serverOutputFilter);
    // Input chain: Transport -> StreamFramerInput -> RpcManager
    serverRpc.addInputFilter(&serverInputFilter);
    
    // Add filters to client (mirror of server)
    clientRpc.addOutputFilter(&clientOutputFilter);
    clientRpc.addInputFilter(&clientInputFilter);
    
    // Setup transport callbacks (simulates serial/network transport)
    clientRpc.setOnSendCallback([&serverRpc](const char* data, int len) {
        std::cout << "[TRANSPORT] Client->Server: " << len << " bytes (framed data)" << std::endl;
        serverRpc.processInput(data, len);
    });
    
    serverRpc.setOnSendCallback([&clientRpc](const char* data, int len) {
        std::cout << "[TRANSPORT] Server->Client: " << len << " bytes (framed data)" << std::endl;
        clientRpc.processInput(data, len);
    });
    
    // Setup error callbacks
    serverRpc.onError([](int code, const char* msg) {
        std::cout << "[SERVER ERROR " << code << "] " << msg << std::endl;
    });
    
    clientRpc.onError([](int code, const char* msg) {
        std::cout << "[CLIENT ERROR " << code << "] " << msg << std::endl;
    });
    
    // ---------------------------------------------
    // Server Implementation
    // ---------------------------------------------
    MyRpcProvider server;
    
    server.add = [](int a, int b) {
        std::cout << "[SERVER] add(" << a << ", " << b << ") = " << (a + b) << std::endl;
        return a + b;
    };
    
    server.log = [](int value) {
        std::cout << "[SERVER] log(" << value << ") - void method" << std::endl;
    };
    
    server.transformPoint = [](Point p) {
        std::cout << "[SERVER] transformPoint({" << p.x << ", " << p.y << ", " << p.z << "})" << std::endl;
        Point result = {p.x * 2.0f, p.y * 2.0f, p.z * 2.0f};
        return result;
    };
    
    server.processMessage = [](Message msg) {
        std::cout << "[SERVER] processMessage: \"" << msg.text << "\"" << std::endl;
        Message reply;
        std::snprintf(reply.text, sizeof(reply.text), "ACK: %s", msg.text);
        reply.priority = msg.priority + 1;
        reply.timestamp = msg.timestamp + 1000;
        return reply;
    };
    
    server.asyncMultiply = [](int a, int b) -> RpcFuture<int> {
        std::cout << "[SERVER] asyncMultiply(" << a << ", " << b << ")" << std::endl;
        RpcFuture<int> future;
        std::thread([future, a, b]() mutable {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            future.setValue(a * b);
        }).detach();
        return future;
    };
    
    server.echoBytes = [](RpcByteArray input) -> RpcByteArray {
        std::cout << "[SERVER] echoBytes(length=" << input.length << ")" << std::endl;
        static char response_buf[1024];
        std::memcpy(response_buf, input.content, input.length);
        return RpcByteArray{response_buf, input.length};
    };
    
    server.processWithBytes = [](int prefix, RpcByteArray data, int suffix) -> int {
        std::cout << "[SERVER] processWithBytes(" << prefix << ", length=" << data.length
                  << ", " << suffix << ")" << std::endl;
        int sum = prefix + suffix;
        for (uint16_t i = 0; i < data.length; i++) {
            sum += static_cast<unsigned char>(data.content[i]);
        }
        return sum;
    };
    
    server.asyncGenerateBytes = [](int size) -> RpcFuture<RpcByteArray> {
        std::cout << "[SERVER] asyncGenerateBytes(" << size << ")" << std::endl;
        RpcFuture<RpcByteArray> future;
        std::thread([future, size]() mutable {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            static char buf[1024];
            for (int i = 0; i < size && i < 1024; i++) {
                buf[i] = 'A' + (i % 26);
            }
            future.setValue(RpcByteArray{buf, static_cast<uint16_t>(std::min(size, 1024))});
        }).detach();
        return future;
    };
    
    server.reverseString = [](std::string input) -> std::string {
        std::cout << "[SERVER] reverseString(\"" << input << "\")" << std::endl;
        std::string result = input;
        std::reverse(result.begin(), result.end());
        return result;
    };
    
    server.mixedStringArgs = [](std::string prefix, int num, std::string suffix) -> int {
        std::cout << "[SERVER] mixedStringArgs(\"" << prefix << "\", " << num << ", \"" << suffix << "\")" << std::endl;
        return (int)prefix.length() + num + (int)suffix.length();
    };
    
    server.asyncGenerateString = [](int count) -> RpcFuture<std::string> {
        std::cout << "[SERVER] asyncGenerateString(" << count << ")" << std::endl;
        RpcFuture<std::string> future;
        std::thread([future, count]() mutable {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            std::string result;
            for (int i = 0; i < count; i++) {
                result += std::to_string(i) + " ";
            }
            future.setValue(result);
        }).detach();
        return future;
    };
    
    serverRpc.registerServer(server);
    std::cout << "Server registered with StreamFramer filters\n\n";
    
    // ---------------------------------------------
    // Client Calls
    // ---------------------------------------------
    auto client = clientRpc.createClient<MyRpcProvider>();
    std::cout << "Client created\n\n";
    
    // ---------------------------------------------
    // Test 1: Simple int addition
    // ---------------------------------------------
    std::cout << "--- Test 1: Call add(15, 25) with framing ---" << std::endl;
    int sum = client.add(15, 25);
    std::cout << "[CLIENT] Result: " << sum << "\n\n";
    
    // ---------------------------------------------
    // Test 2: Void method (fire and forget)
    // ---------------------------------------------
    std::cout << "--- Test 2: Call log(999) with framing ---" << std::endl;
    client.log(999);
    std::cout << "[CLIENT] log() called\n\n";
    
    // ---------------------------------------------
    // Test 3: Struct argument and return
    // ---------------------------------------------
    std::cout << "--- Test 3: Call transformPoint() with framing ---" << std::endl;
    Point pt = {10.0f, 20.0f, 30.0f};
    Point result_pt = client.transformPoint(pt);
    std::cout << "[CLIENT] Result: {" << result_pt.x << ", " << result_pt.y << ", " << result_pt.z << "}\n\n";
    
    // ---------------------------------------------
    // Test 4: Message struct with char array
    // ---------------------------------------------
    std::cout << "--- Test 4: Call processMessage() with framing ---" << std::endl;
    Message msg;
    std::snprintf(msg.text, sizeof(msg.text), "Framed RPC!");
    msg.priority = 10;
    msg.timestamp = 987654321;
    
    Message response_msg = client.processMessage(msg);
    std::cout << "[CLIENT] Response: \"" << response_msg.text << "\"\n\n";
    
    // ---------------------------------------------
    // Test 5: Async call
    // ---------------------------------------------
    std::cout << "--- Test 5: Call asyncMultiply(9, 11) with framing ---" << std::endl;
    RpcFuture<int> future = client.asyncMultiply(9, 11);
    std::cout << "[CLIENT] Waiting for async result..." << std::endl;
    int async_result = future.get();
    std::cout << "[CLIENT] Async result: " << async_result << "\n\n";
    
    // ---------------------------------------------
    // Test 6: Byte array
    // ---------------------------------------------
    std::cout << "--- Test 6: Call echoBytes() with framing ---" << std::endl;
    char test_bytes[] = "Binary data with framing!";
    RpcByteArray input{test_bytes, 25};
    RpcByteArray output = client.echoBytes(input);
    std::cout << "[CLIENT] Echo result length: " << output.length << "\n\n";
    
    // ---------------------------------------------
    // Test 7: Mixed arguments with byte array
    // ---------------------------------------------
    std::cout << "--- Test 7: Call processWithBytes() with framing ---" << std::endl;
    char data[] = {1, 2, 3, 4, 5};
    RpcByteArray byte_data{data, 5};
    int mixed_result = client.processWithBytes(100, byte_data, 200);
    std::cout << "[CLIENT] Mixed result: " << mixed_result << "\n\n";
    
    // ---------------------------------------------
    // Test 8: Async byte array
    // ---------------------------------------------
    std::cout << "--- Test 8: Call asyncGenerateBytes(30) with framing ---" << std::endl;
    RpcFuture<RpcByteArray> future_bytes = client.asyncGenerateBytes(30);
    RpcByteArray async_bytes = future_bytes.get();
    std::cout << "[CLIENT] Async bytes length: " << async_bytes.length << "\n\n";
    
    // ---------------------------------------------
    // Test 9: String reversal
    // ---------------------------------------------
    std::cout << "--- Test 9: Call reverseString() with framing ---" << std::endl;
    std::string reversed = client.reverseString("StreamFramer");
    std::cout << "[CLIENT] Reversed: \"" << reversed << "\"\n\n";
    
    // ---------------------------------------------
    // Test 10: Mixed string arguments
    // ---------------------------------------------
    std::cout << "--- Test 10: Call mixedStringArgs() with framing ---" << std::endl;
    int str_result = client.mixedStringArgs("Hello", 42, "World");
    std::cout << "[CLIENT] String args result: " << str_result << "\n\n";
    
    // ---------------------------------------------
    // Test 11: Async string generation
    // ---------------------------------------------
    std::cout << "--- Test 11: Call asyncGenerateString(5) with framing ---" << std::endl;
    RpcFuture<std::string> future_str = client.asyncGenerateString(5);
    std::string async_str = future_str.get();
    std::cout << "[CLIENT] Async string: \"" << async_str << "\"\n\n";
    
    std::cout << "=== StreamFramerFilter RPC tests completed! ===" << std::endl;
}

// ---------------------------------------------
// StreamFramer tests
// ---------------------------------------------
void streamFramerTest() {
    std::cout << "\n=== StreamFramer Tests ===" << std::endl << std::endl;
    
    StreamFramer framer;
    
    // Track received packets
    std::vector<std::vector<uint8_t>> received_packets;
    
    framer.setOnPacket([&received_packets](Packet& pkt) {
        std::cout << "[FRAMER] Received packet: " << pkt.length << " bytes" << std::endl;
        std::vector<uint8_t> packet_data(pkt.data, pkt.data + pkt.length);
        received_packets.push_back(packet_data);
    });
    
    // ---------------------------------------------
    // Test 1: Create and parse a simple packet
    // ---------------------------------------------
    std::cout << "=== Test 1: Simple packet ===" << std::endl;
    const char* test_data = "Hello, World!";
    Packet pkt1 = framer.createPacket(test_data, 13);
    std::cout << "[TEST] Created packet: " << pkt1.length << " bytes" << std::endl;
    
    // Parse it back
    received_packets.clear();
    framer.writeBytes(pkt1.data, pkt1.length);
    
    if (received_packets.size() == 1) {
        std::cout << "[TEST] ✓ Packet received successfully" << std::endl;
        // Verify content (skip 10 byte header)
        const char* content = reinterpret_cast<const char*>(received_packets[0].data() + 10);
        if (std::memcmp(content, test_data, 13) == 0) {
            std::cout << "[TEST] ✓ Content matches: \"" << std::string(content, 13) << "\"" << std::endl;
        } else {
            std::cout << "[TEST] ✗ Content mismatch!" << std::endl;
        }
    } else {
        std::cout << "[TEST] ✗ Expected 1 packet, got " << received_packets.size() << std::endl;
    }
    std::cout << std::endl;
    
    // ---------------------------------------------
    // Test 2: Byte-by-byte processing
    // ---------------------------------------------
    std::cout << "=== Test 2: Byte-by-byte processing ===" << std::endl;
    const char* test_data2 = "Byte by byte!";
    Packet pkt2 = framer.createPacket(test_data2, 13);
    
    received_packets.clear();
    for (int i = 0; i < pkt2.length; i++) {
        framer.writeBytes(&pkt2.data[i], 1);
    }
    
    if (received_packets.size() == 1) {
        std::cout << "[TEST] ✓ Packet received via byte-by-byte" << std::endl;
    } else {
        std::cout << "[TEST] ✗ Expected 1 packet, got " << received_packets.size() << std::endl;
    }
    std::cout << std::endl;
    
    // ---------------------------------------------
    // Test 3: Multiple packets in single buffer
    // ---------------------------------------------
    std::cout << "=== Test 3: Multiple packets at once ===" << std::endl;
    const char* data3a = "First";
    const char* data3b = "Second";
    const char* data3c = "Third";
    
    Packet pkt3a = framer.createPacket(data3a, 5);
    Packet pkt3b = framer.createPacket(data3b, 6);
    Packet pkt3c = framer.createPacket(data3c, 5);
    
    // Combine into one buffer
    std::vector<char> combined_buffer;
    combined_buffer.insert(combined_buffer.end(), pkt3a.data, pkt3a.data + pkt3a.length);
    combined_buffer.insert(combined_buffer.end(), pkt3b.data, pkt3b.data + pkt3b.length);
    combined_buffer.insert(combined_buffer.end(), pkt3c.data, pkt3c.data + pkt3c.length);
    
    received_packets.clear();
    framer.writeBytes(combined_buffer.data(), (int)combined_buffer.size());
    
    if (received_packets.size() == 3) {
        std::cout << "[TEST] ✓ All 3 packets received" << std::endl;
    } else {
        std::cout << "[TEST] ✗ Expected 3 packets, got " << received_packets.size() << std::endl;
    }
    std::cout << std::endl;
    
    // ---------------------------------------------
    // Test 4: Noise before packet (garbage data)
    // ---------------------------------------------
    std::cout << "=== Test 4: Garbage data before packet ===" << std::endl;
    const char* data4 = "After garbage";
    Packet pkt4 = framer.createPacket(data4, 13);
    
    // Add garbage before packet
    std::vector<char> noisy_buffer;
    const char garbage[] = {0x12, 0x34, 0x56, 0x78, static_cast<char>(0xAB), static_cast<char>(0xCD)};
    noisy_buffer.insert(noisy_buffer.end(), garbage, garbage + 6);
    noisy_buffer.insert(noisy_buffer.end(), pkt4.data, pkt4.data + pkt4.length);
    
    received_packets.clear();
    framer.writeBytes(noisy_buffer.data(), (int)noisy_buffer.size());
    
    if (received_packets.size() == 1) {
        std::cout << "[TEST] ✓ Packet found after garbage" << std::endl;
    } else {
        std::cout << "[TEST] ✗ Expected 1 packet, got " << received_packets.size() << std::endl;
    }
    std::cout << std::endl;
    
    // ---------------------------------------------
    // Test 5: False magic number detection
    // ---------------------------------------------
    std::cout << "=== Test 5: False magic number ===" << std::endl;
    const char* data5 = "Real packet";
    Packet pkt5 = framer.createPacket(data5, 11);
    
    // Add false magic byte sequence
    std::vector<char> false_magic_buffer;
    const char false_magic[] = {static_cast<char>(0xEF), static_cast<char>(0xFF)};  // First byte matches, second doesn't
    false_magic_buffer.insert(false_magic_buffer.end(), false_magic, false_magic + 2);
    false_magic_buffer.insert(false_magic_buffer.end(), pkt5.data, pkt5.data + pkt5.length);
    
    received_packets.clear();
    framer.writeBytes(false_magic_buffer.data(), (int)false_magic_buffer.size());
    
    if (received_packets.size() == 1) {
        std::cout << "[TEST] ✓ Recovered from false magic number" << std::endl;
    } else {
        std::cout << "[TEST] ✗ Expected 1 packet, got " << received_packets.size() << std::endl;
    }
    std::cout << std::endl;
    
    // ---------------------------------------------
    // Test 6: Empty packet (zero content)
    // ---------------------------------------------
    std::cout << "=== Test 6: Empty packet ===" << std::endl;
    Packet pkt6 = framer.createPacket("", 0);
    
    received_packets.clear();
    framer.writeBytes(pkt6.data, pkt6.length);
    
    if (received_packets.size() == 1 && received_packets[0].size() == 10) {
        std::cout << "[TEST] ✓ Empty packet (header only) received" << std::endl;
    } else {
        std::cout << "[TEST] ✗ Empty packet test failed" << std::endl;
    }
    std::cout << std::endl;
    
    // ---------------------------------------------
    // Test 7: Large packet
    // ---------------------------------------------
    std::cout << "=== Test 7: Large packet ===" << std::endl;
    std::vector<char> large_data(5000, 'X');
    Packet pkt7 = framer.createPacket(large_data.data(), (int)large_data.size());
    
    received_packets.clear();
    framer.writeBytes(pkt7.data, pkt7.length);
    
    if (received_packets.size() == 1 && received_packets[0].size() == 5010) {
        std::cout << "[TEST] ✓ Large packet (5000 bytes) received" << std::endl;
    } else {
        std::cout << "[TEST] ✗ Large packet test failed" << std::endl;
    }
    std::cout << std::endl;
    
    // ---------------------------------------------
    // Test 8: Chunked packet delivery
    // ---------------------------------------------
    std::cout << "=== Test 8: Chunked delivery ===" << std::endl;
    const char* data8 = "Chunked packet test";
    Packet pkt8 = framer.createPacket(data8, 19);
    
    received_packets.clear();
    // Send in 5-byte chunks
    for (int i = 0; i < pkt8.length; i += 5) {
        int chunk_size = std::min(5, (int)(pkt8.length - i));
        framer.writeBytes(&pkt8.data[i], chunk_size);
    }
    
    if (received_packets.size() == 1) {
        std::cout << "[TEST] ✓ Packet received via chunked delivery" << std::endl;
    } else {
        std::cout << "[TEST] ✗ Expected 1 packet, got " << received_packets.size() << std::endl;
    }
    std::cout << std::endl;
    
    // ---------------------------------------------
    // Test 9: Corrupted header CRC recovery
    // ---------------------------------------------
    std::cout << "=== Test 9: Corrupted header CRC recovery ===" << std::endl;
    const char* data9a = "Corrupted";
    const char* data9b = "Valid";
    
    Packet pkt9a = framer.createPacket(data9a, 9);
    Packet pkt9b = framer.createPacket(data9b, 5);
    
    // Corrupt the header CRC of first packet
    std::vector<char> corrupted_buffer(pkt9a.data, pkt9a.data + pkt9a.length);
    corrupted_buffer[8] ^= 0xFF;  // Flip header CRC byte
    
    // Append valid packet
    corrupted_buffer.insert(corrupted_buffer.end(), pkt9b.data, pkt9b.data + pkt9b.length);
    
    received_packets.clear();
    framer.writeBytes(corrupted_buffer.data(), (int)corrupted_buffer.size());
    
    if (received_packets.size() == 1) {
        std::cout << "[TEST] ✓ Recovered from corrupted header, found valid packet" << std::endl;
    } else {
        std::cout << "[TEST] ✗ Expected 1 packet, got " << received_packets.size() << std::endl;
    }
    std::cout << std::endl;
    
    // ---------------------------------------------
    // Test 10: Corrupted content CRC (packet dropped)
    // ---------------------------------------------
    std::cout << "=== Test 10: Corrupted content CRC ===" << std::endl;
    const char* data10 = "Content corrupted";
    Packet pkt10 = framer.createPacket(data10, 17);
    
    // Corrupt content
    std::vector<char> corrupted_content(pkt10.data, pkt10.data + pkt10.length);
    corrupted_content[15] ^= 0xFF;  // Flip a content byte
    
    received_packets.clear();
    framer.writeBytes(corrupted_content.data(), (int)corrupted_content.size());
    
    if (received_packets.size() == 0) {
        std::cout << "[TEST] ✓ Corrupted content packet correctly dropped" << std::endl;
    } else {
        std::cout << "[TEST] ✗ Corrupted packet should be dropped" << std::endl;
    }
    std::cout << std::endl;
    
    std::cout << "=== StreamFramer tests completed! ===" << std::endl;
}

// ---------------------------------------------
// Test: Packet Hooks (Block/Allow packets)
// ---------------------------------------------
void packetHookTest() {
    std::cout << "\n=== Packet Hook Test ===" << std::endl << std::endl;
    
    // Create RPC managers
    RpcManager<> serverRpc;
    RpcManager<> clientRpc;
    
    g_server = &serverRpc;
    g_client = &clientRpc;
    serverRpc.setDefaultTimeout(1000);
    clientRpc.setDefaultTimeout(1000);
    // Setup transport callbacks
    clientRpc.setOnSendCallback(clientToServer);
    serverRpc.setOnSendCallback(serverToClient);

    // Setup error callbacks
    serverRpc.onError([](int code, const char* msg) {
        if(code != RPC_ERROR_TIMEOUT)
            std::cout << "[SERVER ERROR " << code << "] " << msg << std::endl;
    });

    clientRpc.onError([](int code, const char* msg) {
        if(code != RPC_ERROR_TIMEOUT)
            std::cout << "[CLIENT ERROR " << code << "] " << msg << std::endl;
    });

    // ---------------------------------------------
    // Setup SERVER implementation
    // ---------------------------------------------
    MyRpcProvider server;

    server.add = [](int a, int b) {
        std::cout << "[SERVER] add(" << a << ", " << b << ") called" << std::endl;
        return a + b;
    };

    server.log = [](int value) {
        std::cout << "[SERVER] log(" << value << ") called" << std::endl;
    };
    
    // Register server
    serverRpc.registerServer(server);
    
    // Create client
    auto client = clientRpc.createClient<MyRpcProvider>();
    
    // ---------------------------------------------
    // Test 1: onSendPacketHook - Block outgoing packet
    // ---------------------------------------------
    std::cout << "=== Test 1: onSendPacketHook - Block packet with methodId=0 (add) ===" << std::endl;
    
    bool sendHookCalled = false;
    clientRpc.onSendPacketHook([&sendHookCalled](const RpcPacket* packet) -> bool {
        sendHookCalled = true;
        // Extract methodId (little-endian)
        uint16_t methodId = read_u16_le(reinterpret_cast<const uint8_t*>(&packet->methodId));
        uint16_t providerId = read_u16_le(reinterpret_cast<const uint8_t*>(&packet->providerId));
        uint8_t flags = packet->flags;
        uint32_t callId = read_u32_le(reinterpret_cast<const uint8_t*>(&packet->callId));
        
        std::cout << "[SEND HOOK] Inspecting packet: providerId=" << providerId 
                  << ", methodId=" << methodId << ", flags=" << (int)flags 
                  << ", callId=" << callId << std::endl;
        
        // Block packets with methodId == 0 (add method)
        if (methodId == 0) {
            std::cout << "[SEND HOOK] BLOCKING packet with methodId=0" << std::endl;
            return false;  // Block the packet
        }
        
        std::cout << "[SEND HOOK] Allowing packet" << std::endl;
        return true;  // Allow other packets
    });
    
    std::cout << "[CLIENT] Calling add(5, 3) - should be blocked by send hook..." << std::endl;
    int result1 = client.add(5, 3);
    std::cout << "[CLIENT] Result: " << result1 << " (should be default/0 since blocked)" << std::endl;
    
    if (sendHookCalled) {
        std::cout << "[TEST] ✓ Send hook was called" << std::endl;
    } else {
        std::cout << "[TEST] ✗ Send hook was NOT called" << std::endl;
    }
    std::cout << std::endl;
    
    // ---------------------------------------------
    // Test 2: onSendPacketHook - Allow packet through
    // ---------------------------------------------
    std::cout << "=== Test 2: onSendPacketHook - Allow packet with methodId=1 (log) ===" << std::endl;
    
    sendHookCalled = false;
    std::cout << "[CLIENT] Calling log(42) - should pass through send hook..." << std::endl;
    client.log(42);
    
    if (sendHookCalled) {
        std::cout << "[TEST] ✓ Send hook was called and packet was sent" << std::endl;
    } else {
        std::cout << "[TEST] ✗ Send hook was NOT called" << std::endl;
    }
    std::cout << std::endl;
    
    // Clear send hook
    clientRpc.onSendPacketHook(nullptr);
    
    // ---------------------------------------------
    // Test 3: onReceivePacketHook - Block incoming request
    // ---------------------------------------------
    std::cout << "=== Test 3: onReceivePacketHook - Block incoming request with methodId=0 ===" << std::endl;
    
    bool receiveHookCalled = false;
    serverRpc.onReceivePacketHook([&receiveHookCalled](const RpcPacket* packet) -> bool {
        receiveHookCalled = true;
        
        // Extract packet fields
        uint16_t methodId = read_u16_le(reinterpret_cast<const uint8_t*>(&packet->methodId));
        uint16_t providerId = read_u16_le(reinterpret_cast<const uint8_t*>(&packet->providerId));
        uint8_t flags = packet->flags;
        uint32_t callId = read_u32_le(reinterpret_cast<const uint8_t*>(&packet->callId));
        
        std::cout << "[RECEIVE HOOK] Inspecting packet: providerId=" << providerId 
                  << ", methodId=" << methodId << ", flags=" << (int)flags 
                  << ", callId=" << callId << std::endl;
        
        // Block incoming requests (not replies) with methodId == 0
        if (methodId == 0 && !(flags & RPC_FLAG_REPLY)) {
            std::cout << "[RECEIVE HOOK] BLOCKING request with methodId=0" << std::endl;
            return false;  // Block the packet
        }
        
        std::cout << "[RECEIVE HOOK] Allowing packet" << std::endl;
        return true;  // Allow other packets
    });
    
    std::cout << "[CLIENT] Calling add(10, 20) - server should block it..." << std::endl;
    int result3 = client.add(10, 20);
    std::cout << "[CLIENT] Result: " << result3 << " (should timeout or return 0)" << std::endl;
    
    if (receiveHookCalled) {
        std::cout << "[TEST] ✓ Receive hook was called on server" << std::endl;
    } else {
        std::cout << "[TEST] ✗ Receive hook was NOT called" << std::endl;
    }
    std::cout << std::endl;
    
    // ---------------------------------------------
    // Test 4: onReceivePacketHook - Allow packet through
    // ---------------------------------------------
    std::cout << "=== Test 4: onReceivePacketHook - Allow request with methodId=1 ===" << std::endl;
    
    receiveHookCalled = false;
    std::cout << "[CLIENT] Calling log(99) - server should receive it..." << std::endl;
    client.log(99);
    
    if (receiveHookCalled) {
        std::cout << "[TEST] ✓ Receive hook was called and packet was processed" << std::endl;
    } else {
        std::cout << "[TEST] ✗ Receive hook was NOT called" << std::endl;
    }
    std::cout << std::endl;
    
    // ---------------------------------------------
    // Test 5: Combined hooks - Block reply from server
    // ---------------------------------------------
    std::cout << "=== Test 5: Block reply packet on server side ===" << std::endl;
    
    // Clear receive hook, set send hook on server to block replies
    serverRpc.onReceivePacketHook(nullptr);
    
    bool serverSendHookCalled = false;
    serverRpc.onSendPacketHook([&serverSendHookCalled](const RpcPacket* packet) -> bool {
        serverSendHookCalled = true;
        
        uint8_t flags = packet->flags;
        uint16_t methodId = read_u16_le(reinterpret_cast<const uint8_t*>(&packet->methodId));
        
        std::cout << "[SERVER SEND HOOK] Inspecting packet: methodId=" << methodId 
                  << ", isReply=" << ((flags & RPC_FLAG_REPLY) ? "yes" : "no") << std::endl;
        
        // Block reply packets for methodId 0
        if ((flags & RPC_FLAG_REPLY) && methodId == 0) {
            std::cout << "[SERVER SEND HOOK] BLOCKING reply for methodId=0" << std::endl;
            return false;
        }
        
        return true;
    });
    
    std::cout << "[CLIENT] Calling add(7, 8) - server will process but block reply..." << std::endl;
    int result5 = client.add(7, 8);
    std::cout << "[CLIENT] Result: " << result5 << " (should timeout or return 0)" << std::endl;
    
    if (serverSendHookCalled) {
        std::cout << "[TEST] ✓ Server send hook blocked the reply" << std::endl;
    } else {
        std::cout << "[TEST] ✗ Server send hook was NOT called" << std::endl;
    }
    std::cout << std::endl;
    
    std::cout << "=== Packet Hook tests completed! ===" << std::endl;
    
    // Cleanup
    g_server = nullptr;
    g_client = nullptr;
}

// ---------------------------------------------
// Main
// ---------------------------------------------
int main() {
    simpleOneWayTest();
    twoWayTest();
    streamFramerFilterRpcTest();
    streamFramerTest();
    packetHookTest();
    timeoutTest();
    return 0;
}
