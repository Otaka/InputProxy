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

### 3. Pico — CdcProxyCoroutine (new)

**File:** `Pico/src/CdcProxyCoroutine.h/.cpp`

A single coroutine that runs for the lifetime of the Pico:

- Reads raw bytes from USB CDC → feeds into a dedicated `StreamFramer` (CDC framer)
- On channel 2 frame received from CDC framer → reframes onto UART `StreamFramer` channel 2
- On channel 2 frame received from UART framer → reframes onto CDC framer → writes to USB CDC
- Channel 0 frames on UART are ignored (handled by existing RPC coroutine)

The UART `StreamFramer` reference is passed in at construction — `CdcProxyCoroutine` taps into it alongside the existing RPC handler.

**Channel assignment:**
- Channel 0: existing Pico↔RPI4 RPC (M2P/P2M, unchanged)
- Channel 2: PC↔RPI4 proxy traffic

---

### 4. RPI4 — ControlService (new, extracted from RestApi)

**File:** `mainboard/src/rest/ControlService.h/.cpp`

Contains all business logic currently in `RestApi` handler lambdas:
- Layer management: `getLayerList()`, `getActiveLayers()`, `activateLayer(id)`, `deactivateLayer(id)`
- Device queries: `getRealDeviceList()`, `getRealDeviceDetail(id)`, `getEmulationBoardList()`, etc.
- Board commands: `rebootBoard(id)`, `pingBoard(id, val)`, `setAxis(boardId, device, axis, value)`, etc.
- Config: `reloadConfig()`

`RestApi` is refactored to call `ControlService` methods instead of containing logic directly. Behavior is identical.

---

### 5. RPI4 — CdcApi (new)

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

**Dispatch:** parses `method` + `path`, routes to the appropriate `ControlService` method, serializes result to JSON, sends response frame back on **the same `StreamFramer` instance** that delivered the request. This guarantees the response returns to the correct Pico (and thus the correct PC).

**Ping support:** `{"method":"PING","path":"/ping","body":""}` → `{"status":200,"body":"pong"}` — used by Qt app for device identification.

---

### 6. Qt Desktop App — ProxySerialPort (new)

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
