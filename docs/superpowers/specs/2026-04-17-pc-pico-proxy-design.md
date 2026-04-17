# PC↔Pico↔RPI4 Transparent Proxy Design

**Date:** 2026-04-17  
**Goal:** Allow the Qt Desktop app on PC to communicate with the RPI4 mainboard REST API via a USB serial connection through any connected Pico, eliminating the WiFi/network dependency.

---

## Overview

Every Pico always exposes a hardcoded CDC USB serial device (fixed VID/PID) regardless of its HID or XInput mode. The Qt Desktop app connects to this serial port and sends JSON-framed requests. The Pico forwards them transparently over UART to the RPI4. The RPI4's new `CdcApi` executes the request via `ControlService` and sends the response back over the same UART to the same Pico, which forwards it back to the PC.

```
Qt Desktop App
  │  QSerialPort (CDC serial, VID=0x1209 PID=0x0001)
  │  StreamFramer-compatible framing (Qt port, no corocgo)
  │  channel 2 frames: {"method","path","body"}
  ▼
Pico (always-on CDC device, hardcoded VID=0x1209 PID=0x0001)
  │  CdcProxyCoroutine: USB CDC ↔ UART, channel 2 only
  │  channel 0 unchanged (existing M2P/P2M RPC)
  ▼
RPI4 UART (per-Pico UartManager)
  │  CdcApi: receives channel 2 frames, calls ControlService
  │  Response sent back on the same UART framer
  ▼
ControlService (extracted from RestApi)
  ▲
RestApi (now delegates to ControlService)
```

---

## Components

### 1. shared/crc16.h (new)

Standalone CRC16-IBM implementation with no dependencies. Used by Pico firmware, RPI4 `CdcApi`, and Qt Desktop app to share the same framing algorithm without pulling in corocgo.

---

### 2. Pico — Always-on CDC USB Device

**File changes:**
- `Pico/src/tusb_config.h`: add `CFG_TUD_CDC 1`
- `Pico/CMakeLists.txt`: add `tinyusb_device` CDC class
- `Pico/src/mainPico.cpp`: initialize CDC device at startup before HID/XInput init

**Fixed USB identity (hardcoded, never from config):**
- VID: `0x1209`
- PID: `0x0001`
- Serial: `"IPPROXY"`
- Product: `"InputProxy Control"`

This device is always present. The HID or XInput device continues to use VID/PID from config as today.

---

### 3. Pico — CdcFramerLite (new)

**File:** `Pico/src/CdcFramerLite.h`

A plain synchronous state machine class (no coroutines, no channels) that parses the same 12-byte StreamFramer wire format incrementally. Accepts one byte at a time via `feed(byte)`, returns `true` when a complete valid frame is ready. Caller reads frame via `data()`/`size()`, then calls `reset()`.

No internal coroutine — zero RAM overhead beyond the parse buffer (~2 KB for the frame). Used instead of a second `StreamFramer` to avoid spawning an extra coroutine.

---

### 4. Pico — CDC proxy coroutine (new, in mainPico.cpp)

**+1 coroutine** added to `mainPico.cpp`. Handles all CDC I/O in a single loop:

```
CDC bytes in  →  CdcFramerLite  →  complete frame  →  reframe on UART channel 2  →  uartManager->sendData()
cdcOutCh      →  tud_cdc_write() + flush
```

```cpp
coro([]() {
    CdcFramerLite parser;
    while (true) {
        // PC → RPI4: read CDC bytes, parse frames, forward to UART channel 2
        if (tud_cdc_available()) {
            uint8_t buf[64];
            uint32_t n = tud_cdc_read(buf, sizeof(buf));
            for (uint32_t i = 0; i < n; i++) {
                if (parser.feed(buf[i])) {
                    FramedPacket fp = framer->createPacket(2, parser.data(), parser.size());
                    uartManager->sendData(fp.data, fp.size);
                    parser.reset();
                }
            }
        }
        // RPI4 → PC: drain pending CDC output frames
        while (cdcOutCh->canReceive()) {
            auto fp = cdcOutCh->tryReceive();
            tud_cdc_write(fp.data, fp.size);
            tud_cdc_write_flush();
        }
        coro_yield();
    }
});
```

The existing UART→framer→rpcInCh bridge coroutine is modified (not replaced) to route by channel:
- Channel 0 → `rpcInCh` (existing, unchanged)
- Channel 2 → `cdcOutCh` (new `Channel<FramedPacket>`)

**Net cost: +1 coroutine, +1 `Channel<FramedPacket> cdcOutCh`, +1 `CdcFramerLite` on the stack.**

**Channel assignment:**
- Channel 0: existing Pico↔RPI4 RPC (M2P/P2M, unchanged)
- Channel 2: PC↔RPI4 proxy traffic

---

### 4. Fragmentation Protocol

REST responses can exceed the `SF_BUFFER_SIZE` (2 KB) StreamFramer frame limit. Rather than increasing the frame size (which wastes Pico RAM globally), large payloads are split into multiple frames using the existing unused `UserData` field in the StreamFramer header.

**UserData field encoding (2 bytes, currently always 0):**
```
high byte: fragment index (0, 1, 2, ...)
low byte:  flags — bit 0 = more_fragments (1 = more coming, 0 = last fragment)
```

**Request envelope** (frame payload, channel 2):
```json
{"id": 1, "method": "GET", "path": "/layers", "body": ""}
```

**Response envelope** (one frame per fragment):
```json
{"id": 1, "status": 200, "body": "...up to ~1900 bytes of JSON..."}
```

For a single-frame response: `UserData = 0x0000` (index 0, no more fragments).  
For a multi-frame response: fragments 0..N-1 have `more_fragments=1`, fragment N has `more_fragments=0`.

**Pico is completely unaware of fragmentation** — it forwards all channel 2 frames verbatim regardless of UserData content. Fragmentation is purely end-to-end between `CdcApi` (RPI4, sends fragments) and `ProxySerialPort` (Qt, reassembles by `id` field).

The `id` field in the JSON envelope allows Qt to match response fragments to the original request, supporting a single in-flight request at a time (no pipelining needed).

---

### 6. RPI4 — ControlService (new, extracted from RestApi)

**File:** `mainboard/src/rest/ControlService.h/.cpp`

Contains all business logic currently in `RestApi` handler lambdas:
- Layer management: `getLayerList()`, `getActiveLayers()`, `activateLayer(id)`, `deactivateLayer(id)`
- Device queries: `getRealDeviceList()`, `getRealDeviceDetail(id)`, `getEmulationBoardList()`, etc.
- Board commands: `rebootBoard(id)`, `pingBoard(id, val)`, `setAxis(boardId, device, axis, value)`, etc.
- Config: `reloadConfig()`

`RestApi` is refactored to call `ControlService` methods instead of containing logic directly. Behavior is identical.

---

### 7. RPI4 — CdcApi (new)

**File:** `mainboard/src/rest/CdcApi.h/.cpp`

One `CdcApi` instance per UART channel, created alongside the existing `UartManager`/`EmulationBoard` setup.

**Request format (channel 2 frame payload):**
```json
{"method": "GET", "path": "/layers", "body": ""}
```

**Response format:**
```json
{"status": 200, "body": "{...escaped json...}"}
```

**Dispatch:** parses `id` + `method` + `path`, routes to the appropriate `ControlService` method, serializes result to JSON. If the response body exceeds ~1900 bytes it is split into fragments; each fragment is sent as a separate channel 2 frame with `UserData` encoding the fragment index and `more_fragments` flag. All frames go back on **the same `StreamFramer` instance** that delivered the request, guaranteeing the response returns to the correct Pico (and thus the correct PC).

**Ping support:** `{"method":"PING","path":"/ping","body":""}` → `{"status":200,"body":"pong"}` — used by Qt app for device identification.

---

### 8. Qt Desktop App — ProxySerialPort (new)

**File:** `Desktop/ProxySerialPort.h/.cpp`

Implements the same 12-byte `StreamFramer` wire format (magic `0xEFEE`, CRC16-IBM) using `QSerialPort` and Qt's event loop. No corocgo dependency.

**Public API:**
```cpp
void sendRequest(QString method, QString path, QString body);
signal void requestComplete(int status, QString body);
signal void connectionStateChanged(ConnectionState state);
bool isReady() const;
```

**Connection lifecycle (state machine):**

| State | Action |
|-------|--------|
| `Disconnected` | Entry point / after any error |
| `Scanning` | Poll `QSerialPortInfo` every 2s for VID=`0x1209` PID=`0x0001` |
| `Connecting` | Open port, send ping, wait up to 3s for valid pong |
| `Ready` | Normal operation |

Any error (read/write failure, port disappears, response timeout) → close port → back to `Scanning` immediately.

Pending in-flight requests at disconnect receive `{"status": -1, "body": "disconnected"}` so callers never hang.

**Reassembly:** Qt accumulates incoming channel 2 frames by `id`, checking `UserData` for `more_fragments`. Once the last fragment arrives, concatenates body chunks and resolves the request.

**Baud rate:** configurable in app settings (default: 115200).

---

## Frame Channel Assignment

| Channel | Purpose |
|---------|---------|
| 0 | Existing Pico↔RPI4 RPC (M2P/P2M) — unchanged |
| 2 | PC↔RPI4 proxy traffic (new) |

---

## Key Constraints

- The Pico CDC device identity is hardcoded and never affected by config changes
- After a config change, Pico reboots — the CDC device disappears and reappears; Qt app reconnects automatically via Scanning state
- Multiple Picos can be connected to one PC; Qt app uses the first found
- Multiple Picos can be connected to one RPI4; each has its own `CdcApi` instance bound to its UART — responses never leak to the wrong Pico
- Pico never inspects channel 2 payload content — it is a pure byte forwarder for that channel
