# SimpleRPC - Single-Header RPC Library

A lightweight, transport-agnostic RPC library for C++17 with compile-time type safety.

## Features

✅ **Single header** - Just include `simplerpc.h`  
✅ **Transport agnostic** - Works with UART, I2C, sockets, or any byte stream  
✅ **Type-safe** - Compile-time method registration via templates  
✅ **Lambda-based providers** - Clean, modern C++ design  
✅ **Async support** - Optional `std::future<T>` via `RPCMANAGER_STD_FUTURE` flag  
✅ **Byte arrays** - Variable-length binary data via `RpcByteArray`  
✅ **String support** - Optional `std::string` via `RPCMANAGER_STD_STRING` flag  
✅ **Binary protocol** - Efficient lightweight packet format  
✅ **Fire-and-forget** - Void methods don't send replies  
✅ **Packet hooks** - Intercept and filter packets for access control and logging  
✅ **Embedded-friendly** - Stack-based buffers (default 10KB)  
✅ **Optional thread-safety** - Define `RPCMANAGER_MUTEX` for mutex protection  

## Quick Example

```cpp
#define RPCMANAGER_STD_FUTURE  // Enable async support
#define RPCMANAGER_STD_STRING  // Enable std::string support
#include "simplerpc.h"

// Define provider interface (shared between client/server)
struct MyRpcProvider {
    std::function<int(int, int)> add;
    std::function<void(int)> log;
    std::function<std::future<int>(int, int)> asyncMultiply;
    std::function<RpcByteArray(RpcByteArray)> processData;
    
    static constexpr uint32_t providerId = 42;
    static constexpr auto methods = std::make_tuple(
        &MyRpcProvider::add,
        &MyRpcProvider::log,
        &MyRpcProvider::asyncMultiply,
        &MyRpcProvider::processData
    );
};

// Server side
RpcManager<> serverRpc;
MyRpcProvider server;
server.add = [](int a, int b) { return a + b; };
server.log = [](int v) { std::cout << v << "\n"; };
server.asyncMultiply = [](int a, int b) -> std::future<int> {
    // Async computation...
};
serverRpc.registerServer(server);

// Client side
RpcManager<> clientRpc;
MyRpcProvider client = clientRpc.createClient<MyRpcProvider>();

int result = client.add(10, 20);           // Synchronous call
client.log(42);                             // Fire-and-forget
std::future<int> f = client.asyncMultiply(7, 8);  // Async call

// Byte array call
char data[] = "binary data";
RpcByteArray input{data, 11};
RpcByteArray output = client.processData(input);
```

## Transport Setup

```cpp
// Connect client to server
clientRpc.sendDataCallback([](const uint8_t* data, size_t len) {
    // Send data over your transport (UART, socket, etc.)
    uart_send(data, len);
});

// Feed received data into RPC manager
clientRpc.receiveData(buffer, bytesReceived);
```

## Supported Types

- **Primitives**: `int`, `float`, `double`, `bool`, `char`, etc.
- **Structs**: Any trivially copyable struct
- **Fixed arrays**: `char text[64]`, `int values[10]`, etc.
- **Byte arrays**: `RpcByteArray{const char* content, uint16_t length}` for variable-length binary data (max 65535 bytes)
- **Strings**: `std::string` (when `RPCMANAGER_STD_STRING` is defined) - implemented on top of RpcByteArray
- **Async**: `std::future<T>` (when `RPCMANAGER_STD_FUTURE` is defined) where T is trivially copyable, `RpcByteArray`, or `std::string`

**Not supported**: `std::vector`, pointers, dynamic containers (except `std::string` with flag)

### RpcByteArray Usage

```cpp
// Passing byte arrays
char buffer[] = {0x01, 0x02, 0x03, 0x04};
RpcByteArray data{buffer, 4};
RpcByteArray result = client.processBytes(data);

// Mixing with other types
int sum = client.processWithBytes(100, data, 200);

// Async byte array return
std::future<RpcByteArray> future = client.asyncGenerateBytes(50);
RpcByteArray asyncResult = future.get();
// Note: Data remains valid as long as RpcByteArray is in scope
```

### std::string Support (Optional)

Enable by defining `RPCMANAGER_STD_STRING` before including the header:

```cpp
#define RPCMANAGER_STD_STRING
#include "simplerpc.h"

// String methods work just like any other type
std::string reversed = client.reverseString("Hello!");

// Mix with other types
int length = client.computeLength("prefix", 42, "suffix");

// Async string returns
std::future<std::string> future = client.asyncGenerateText(100);
std::string result = future.get();
```

**Implementation**: Strings are serialized using the same length-table mechanism as `RpcByteArray` (2-byte length prefix + UTF-8 data).

## Protocol Architecture

SimpleRPC uses a modular protocol architecture with separate layers:

### 1. RPC Protocol Layer (RpcManager)

Binary RPC packet format (15-byte header + payload):
```
┌──────┬────────┬──────┬────────────┬──────────┬───────┬────────┬─────────┐
│Magic │ Length │ CRC  │ ProviderID │ MethodID │ Flags │ CallID │ Payload │
│ 2B   │ 2B     │ 2B   │ 2B         │ 2B       │ 1B    │ 4B     │ var     │
└──────┴────────┴──────┴────────────┴──────────┴───────┴────────┴─────────┘
```

- **Magic**: 0xFEEF (little-endian sync marker)
- **CRC**: CRC16-IBM for data integrity
- **Endianness**: Always little-endian

**Payload Format** (with length table for variable-length support):
```
┌────────────────────┬─────────────────────────────────┐
│   Length Table     │      Argument Data              │
│   (2B * N args)    │      (variable length)          │
└────────────────────┴─────────────────────────────────┘
```
- Length table contains uint16_t size for each argument
- POD types: length = sizeof(T)
- RpcByteArray: length = actual byte count

### 2. Transport Framing Layer (StreamFramer)

For reliable transport over byte streams (UART, pipes, etc.), use the **StreamFramer** class to handle packet framing with CRC validation. StreamFramer is independent of RpcManager and handles transport-level concerns.

**Frame Format** (10-byte header + content):
```
┌───────┬────────┬─────────────┬──────────┬────────────┬─────────┐
│Magic  │Content │Content CRC  │User Data │Header CRC  │ Content │
│0xFEEF │Size    │(CRC16-IBM)  │(reserved)│(CRC16-IBM) │  Data   │
│ 2B    │ 2B     │    2B       │   2B     │    2B      │   var   │
└───────┴────────┴─────────────┴──────────┴────────────┴─────────┘
```

- **Magic**: 0xFEEF (0xEF 0xFE on wire, little-endian)
- **Content Size**: Length of content data (uint16_t, max 10214 bytes)
- **Content CRC**: CRC16-IBM of content data
- **User Data**: Reserved for application use (2 bytes)
- **Header CRC**: CRC16-IBM of first 8 header bytes (validates header integrity)
- **Content**: Actual payload data

**Features**:
- Byte-by-byte or bulk processing
- Automatic sync recovery after bad packets
- CRC validation for header and content
- State machine with optimized bulk copying
- No dynamic memory allocation (10KB fixed buffers)
- Maximum packet size: 10KB

### 3. Using Filters for Framing

The recommended approach is to use **DataFilter** instances to add framing:

```cpp
#include "simplerpc.h"

RpcManager<> serverRpc;
RpcManager<> clientRpc;

// Add framing filters (handles packet assembly/disassembly)
StreamFramerInputFilter serverInputFilter;
StreamFramerOutputFilter serverOutputFilter;
serverRpc.addInputFilter(&serverInputFilter);
serverRpc.addOutputFilter(&serverOutputFilter);

StreamFramerInputFilter clientInputFilter;
StreamFramerOutputFilter clientOutputFilter;
clientRpc.addInputFilter(&clientInputFilter);
clientRpc.addOutputFilter(&clientOutputFilter);

// Now send/receive raw bytes through your transport
clientRpc.setOnSendCallback([](const char* data, int len) {
    uart_send(data, len);  // Send framed packet
});

// Feed raw bytes from transport (filters handle framing)
void onUartReceive(const char* data, int len) {
    serverRpc.processInput(data, len);
}
```

**Benefits of Filter Architecture**:
- Clean separation of concerns (RPC logic vs. transport framing)
- RpcManager stays lightweight and focused on RPC semantics (no CRC, no buffering)
- StreamFramer filters handle incomplete packets, sync recovery, and CRC validation
- Easy to swap or chain multiple filters
- Can disable framing for transports that don't need it (e.g., message-based protocols)

## StreamFramer - Reliable Byte Stream Transport

For unreliable or byte-stream transports (UART, pipes, sockets), use `StreamFramer` to add robust packet framing.

### Direct StreamFramer Usage (Low-Level)

```cpp
#include "simplerpc.h"

StreamFramer framer;

// Set callback for complete packets
framer.setOnPacket([](Packet& pkt) {
    std::cout << "Received packet: " << pkt.length << " bytes\n";
    // Process packet data: pkt.data, pkt.length
});

// Feed incoming bytes (byte-by-byte or in chunks)
void onUartData(const char* buffer, unsigned int length) {
    framer.writeBytes(buffer, length);
    // Callback fires when complete packet assembled
}

// Create outgoing framed packet
const char* message = "Hello, world!";
Packet pkt = framer.createPacket(message, 13);
uart_send(pkt.data, pkt.length);  // Send framed packet
```

### StreamFramer with RpcManager (Recommended)

Use filters for seamless integration:

```cpp
// Server setup
RpcManager<> serverRpc;
StreamFramerInputFilter serverIn;
StreamFramerOutputFilter serverOut;
serverRpc.addInputFilter(&serverIn);
serverRpc.addOutputFilter(&serverOut);

serverRpc.setOnSendCallback([](const char* data, int len) {
    uart_send(data, len);  // Already framed by output filter
});

// Client setup
RpcManager<> clientRpc;
StreamFramerInputFilter clientIn;
StreamFramerOutputFilter clientOut;
clientRpc.addInputFilter(&clientIn);
clientRpc.addOutputFilter(&clientOut);

clientRpc.setOnSendCallback([](const char* data, int len) {
    uart_send(data, len);  // Already framed by output filter
});

// Feed raw bytes from UART
void onUartReceive(const char* data, int len) {
    serverRpc.processInput(data, len);  // Input filter handles deframing
}
```

### StreamFramer Features

- **Byte-by-byte processing**: Feed data as it arrives (1 byte, 10 bytes, or bulk)
- **State machine**: Efficient states for magic search, header read, content read
- **Bulk optimization**: Uses `memcpy` for fast header/content copying when possible
- **CRC validation**: Separate CRC16-IBM for header and content
- **Sync recovery**: Automatically finds next valid packet after corruption
- **Multiple packets**: Can process several packets in single `writeBytes()` call
- **Fixed buffers**: 10KB input + 10KB output buffers (no dynamic allocation)
- **Max packet size**: 10KB total (10-byte header + up to 10214 bytes content)

### When to Use StreamFramer

**Use StreamFramer when**:
- Transport is byte-stream based (UART, TCP, pipes)
- Data can be corrupted or fragmented
- No built-in framing in transport layer
- Need reliable packet boundary detection

**Don't use StreamFramer when**:
- Transport is message-based (UDP, CAN, I2C with length)
- Transport provides reliable framing
- Memory is very constrained (20KB overhead for buffers)

## Compile & Run

```bash
# Compile
clang++ -std=c++17 -o test_rpc main.cpp

# Run
./test_rpc
```

## Configuration

```cpp
// Custom buffer size (default 10KB)
RpcManager<4096> rpc;  // 4KB buffers

// Thread-safe (define before including header)
#define RPCMANAGER_MUTEX
#include "simplerpc.h"

// Enable async/future support
#define RPCMANAGER_STD_FUTURE
#include "simplerpc.h"

// Enable std::string support
#define RPCMANAGER_STD_STRING
#include "simplerpc.h"

// Combine multiple flags
#define RPCMANAGER_MUTEX
#define RPCMANAGER_STD_FUTURE
#define RPCMANAGER_STD_STRING
#include "simplerpc.h"
```

## Error Handling

```cpp
rpc.onError([](int code, const char* msg) {
    std::cerr << "RPC Error " << code << ": " << msg << "\n";
});
```

## Multiple Providers

```cpp
// Register with custom provider ID
serverRpc.registerServer(provider, 100);
MyProvider client = clientRpc.createClient<MyProvider>(100);
```

## Deregistering Providers

You can dynamically deregister providers to stop handling incoming RPC calls:

```cpp
// Register a provider
MyRpcProvider provider;
provider.add = [](int a, int b) { return a + b; };
serverRpc.registerServer(provider);

// Later, deregister it using default provider ID
serverRpc.deregisterServer<MyRpcProvider>();

// Or deregister with custom provider ID
serverRpc.deregisterServer<MyRpcProvider>(100);
```

After deregistering:
- All method handlers for that provider are removed
- Incoming RPC calls to that provider will trigger an error: "No handler for provider/method"
- The provider can be re-registered later if needed

**Use cases**:
- Temporarily disabling functionality
- Dynamic service lifecycle management
- Cleanup before shutdown

## Timeout Configuration

SimpleRPC supports three levels of timeout configuration with priority: **per-call > per-method > global**.

### 1. Global Timeout (Default for All Methods)

```cpp
// Set global timeout for all RPC calls (milliseconds)
clientRpc.setDefaultTimeout(5000);  // 5 second timeout

// 0 = infinite timeout (default)
clientRpc.setDefaultTimeout(0);
```

### 2. Per-Method Timeout

```cpp
// Set timeout for specific method (overrides global)
clientRpc.setMethodTimeout<MyProvider>(0, 1000);  // Method index 0 gets 1s timeout
clientRpc.setMethodTimeout<MyProvider>(1, 3000);  // Method index 1 gets 3s timeout

// Clear method timeout (reverts to global)
clientRpc.clearMethodTimeout<MyProvider>(0);

// With custom provider ID
clientRpc.setMethodTimeout<MyProvider>(providerId, methodIndex, 2000);
```

### 3. Per-Call Timeout (Highest Priority)

```cpp
// Use RpcCallContext for individual call timeout
RpcCallContext ctx = RpcCallContext::withTimeout(500);  // 500ms for this call only
internal::CallContextGuard guard(ctx);
int result = client.slowMethod(100);  // This call uses 500ms timeout
// Guard automatically restores previous context when scope exits
```

### Timeout Behavior

- When a timeout occurs:
  - Error callback is invoked with code `8` and message `"RPC call timeout"`
  - The pending call is cleaned up
  - Method returns default-constructed value (e.g., `0` for `int`, empty for structs)
- Timeouts only apply to **synchronous** calls (methods with non-void return types)
- **Void methods** (fire-and-forget) don't wait for replies and ignore timeouts
- **Async methods** (`std::future<T>`) return immediately; caller manages timeout via `wait_for()`

### Complete Timeout Example

```cpp
// Setup
clientRpc.setDefaultTimeout(5000);  // Global: 5 seconds
clientRpc.setMethodTimeout<MyProvider>(1, 2000);  // Method 1: 2 seconds

// Call 1: Uses global timeout (5s)
int r1 = client.fastMethod(10);

// Call 2: Uses per-method timeout (2s)
int r2 = client.slowMethod(20);

// Call 3: Uses per-call timeout (1s), overrides all others
{
    RpcCallContext ctx = RpcCallContext::withTimeout(1000);
    internal::CallContextGuard guard(ctx);
    int r3 = client.slowMethod(30);  // Uses 1s, not 2s
}

// Call 4: Back to per-method timeout (2s)
int r4 = client.slowMethod(40);
```

See `timeout_example.cpp` for a complete working example.

## Packet Hooks - Intercept and Filter RPC Packets

SimpleRPC provides **packet hooks** for intercepting and filtering RPC packets before they are sent or after they are received. This enables advanced use cases like access control, logging, packet filtering, and custom security policies.

### Overview

Two types of hooks are available:

1. **Send Packet Hook** (`onSendPacketHook`) - Intercepts packets before transmission
2. **Receive Packet Hook** (`onReceivePacketHook`) - Intercepts packets after reception

Each hook receives a read-only `RpcPacket*` pointer and returns a boolean:
- `true` = allow packet to proceed
- `false` = block packet (silently drop)

### Hook Signature

```cpp
using PacketHookCallback = std::function<bool(const RpcPacket* packet)>;
```

### RpcPacket Structure

The hook receives access to the packet header fields:

```cpp
struct RpcPacket {
    uint16_t magic;       // 0xFEEF (little-endian)
    uint16_t length;      // Total packet length including header (little-endian)
    uint16_t providerId;  // Little-endian
    uint16_t methodId;    // Little-endian
    uint8_t  flags;       // RPC_FLAG_REPLY (0x01), RPC_FLAG_ASYNC (0x02)
    uint32_t callId;      // Little-endian
    // + payload data (variable length)
};
```

**Important**: Use helper functions to read multi-byte fields correctly:
```cpp
uint16_t length = read_u16_le(reinterpret_cast<const uint8_t*>(&packet->length));
uint16_t providerId = read_u16_le(reinterpret_cast<const uint8_t*>(&packet->providerId));
uint16_t methodId = read_u16_le(reinterpret_cast<const uint8_t*>(&packet->methodId));
uint32_t callId = read_u32_le(reinterpret_cast<const uint8_t*>(&packet->callId));
uint8_t flags = packet->flags;
```

The `length` field is especially useful for routing scenarios where you need to forward or copy the entire packet to another destination without parsing it.

### 1. Send Packet Hook

Intercepts outgoing packets **before** they are sent over the transport:

```cpp
// Set send hook
clientRpc.onSendPacketHook([](const RpcPacket* packet) -> bool {
    // Extract packet fields
    uint16_t methodId = read_u16_le(reinterpret_cast<const uint8_t*>(&packet->methodId));
    uint8_t flags = packet->flags;
    
    // Block all calls to method 0
    if (methodId == 0) {
        std::cout << "Blocking outgoing call to method 0\n";
        return false;  // Block packet
    }
    
    // Allow all other packets
    return true;
});

// This call will be blocked by the hook
int result = client.add(5, 3);  // Won't be sent over transport

// Clear the hook
clientRpc.onSendPacketHook(nullptr);
```

**Use cases**:
- Rate limiting or throttling
- Blocking specific methods at runtime
- Logging/debugging outgoing calls
- Security policies (e.g., block privileged methods)

### 2. Receive Packet Hook

Intercepts incoming packets **after** reception but **before** processing:

```cpp
// Set receive hook
serverRpc.onReceivePacketHook([](const RpcPacket* packet) -> bool {
    // Extract packet fields
    uint16_t methodId = read_u16_le(reinterpret_cast<const uint8_t*>(&packet->methodId));
    uint8_t flags = packet->flags;
    bool isReply = (flags & RPC_FLAG_REPLY) != 0;
    
    // Block incoming requests (not replies) to method 0
    if (methodId == 0 && !isReply) {
        std::cout << "Blocking incoming request to method 0\n";
        return false;  // Block packet
    }
    
    // Allow all other packets
    return true;
});

// Incoming calls to method 0 will be silently dropped
// Client will timeout waiting for a response

// Clear the hook
serverRpc.onReceivePacketHook(nullptr);
```

**Use cases**:
- Access control (reject unauthorized requests)
- Firewall-style packet filtering
- Logging/auditing incoming requests
- Rate limiting per-method or per-client

### 3. Combined Hooks - Block Replies

You can use hooks on both client and server sides. For example, block reply packets on the server:

```cpp
// Server blocks reply packets for specific methods
serverRpc.onSendPacketHook([](const RpcPacket* packet) -> bool {
    uint8_t flags = packet->flags;
    uint16_t methodId = read_u16_le(reinterpret_cast<const uint8_t*>(&packet->methodId));
    bool isReply = (flags & RPC_FLAG_REPLY) != 0;
    
    // Block replies to method 0
    if (isReply && methodId == 0) {
        std::cout << "Blocking reply for method 0\n";
        return false;
    }
    
    return true;
});

// Server processes the call but doesn't send the reply
int result = client.add(10, 20);  // Client will timeout
```

### Hook Behavior

- **Blocking a packet**: Returns `false` - packet is silently dropped, no error callback
- **Allowing a packet**: Returns `true` - packet proceeds normally
- **No hook set**: All packets allowed by default
- **Thread safety**: If `RPCMANAGER_MUTEX` is defined, hooks are called within the mutex lock
- **Fire-and-forget methods**: Send hooks still apply; receive hooks can block void method calls

### Complete Example

```cpp
// Setup
RpcManager<> serverRpc;
RpcManager<> clientRpc;

// Server: Block all incoming requests to method 0
serverRpc.onReceivePacketHook([](const RpcPacket* packet) -> bool {
    uint16_t methodId = read_u16_le(reinterpret_cast<const uint8_t*>(&packet->methodId));
    uint8_t flags = packet->flags;
    
    if (methodId == 0 && !(flags & RPC_FLAG_REPLY)) {
        return false;  // Block method 0 requests
    }
    return true;
});

// Client: Log all outgoing packets
clientRpc.onSendPacketHook([](const RpcPacket* packet) -> bool {
    uint16_t providerId = read_u16_le(reinterpret_cast<const uint8_t*>(&packet->providerId));
    uint16_t methodId = read_u16_le(reinterpret_cast<const uint8_t*>(&packet->methodId));
    uint32_t callId = read_u32_le(reinterpret_cast<const uint8_t*>(&packet->callId));
    
    std::cout << "Sending: provider=" << providerId 
              << ", method=" << methodId 
              << ", callId=" << callId << "\n";
    return true;  // Allow all
});

// Use RPC normally
MyRpcProvider client = clientRpc.createClient<MyRpcProvider>();
int result = client.add(10, 20);  // Logged + blocked by server
```

### Flags Reference

```cpp
#define RPC_FLAG_REPLY 0x01  // Packet is a reply (not a request)
#define RPC_FLAG_ASYNC 0x02  // Async method (returns future)

// Check if packet is a reply
bool isReply = (packet->flags & RPC_FLAG_REPLY) != 0;

// Check if packet is an async call
bool isAsync = (packet->flags & RPC_FLAG_ASYNC) != 0;
```

See `packetHookTest()` in `rpc_example_main.cpp` for complete working examples.

## Implementation Notes

- **Modular architecture**: RpcManager handles RPC semantics only, StreamFramer handles transport framing
- **No CRC in RpcManager**: Keeps RPC layer lightweight; CRC validation is StreamFramer's responsibility
- **Filter-based framing**: Use `StreamFramerInputFilter` and `StreamFramerOutputFilter` for reliable byte-stream transport
- **Packet reassembly**: StreamFramer handles partial/fragmented packets automatically via state machine
- **Sync recovery**: StreamFramer searches for magic number after bad packets with CRC validation
- **Async semantics**: Server blocks on `future.get()`, client returns immediately
- **Void methods**: No reply packet sent (fire-and-forget)
- **Stack-based**: No heap allocations in critical paths (except maps/futures and filter buffers)
- **Two-layer protocol**: Inner RPC protocol (13B header, no CRC) + optional outer framing layer (10B header with dual CRC)
