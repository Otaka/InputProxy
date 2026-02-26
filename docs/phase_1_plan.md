# Phase 1 Implementation Plan: Core Infrastructure

## Overview
Phase 1 establishes the foundational architecture for InputProxy:
- Configuration loading and persistence
- Logical device system for input aggregation
- EmulationHost lifecycle management
- Basic simple_mapping implementation
- Integration of all components

## What Already Exists ✅
- **RealDeviceManager**: USB device detection, evdev integration, axis normalization
- **RPC System**: UART communication with Pico boards, onBoot signal reception
- **EventLoop**: Non-blocking event processing infrastructure
- **Config Structure**: JSON schema defined in [config.json](../mainboard/config.json)

## Implementation Tasks

### 1. Configuration System
**File:** `mainboard/src/ConfigManager.h`, `mainboard/src/ConfigManager.cpp`

**Responsibilities:**
- Load `config.json` on startup
- Parse all configuration sections (devices, mappings, profiles)
- Provide read/write access to configuration
- Save runtime state back to `config.json` on changes
- Validate configuration structure

**Data Structures:**
```cpp
enum class DeviceType {
    KEYBOARD,
    MOUSE,
    GAMEPAD,
    UNKNOWN
};

enum class EmulationMode {
    HID_MODE,
    XINPUT_MODE
};

enum class EmulatedDeviceType {
    HID_KEYBOARD,
    HID_MOUSE,
    HID_GAMEPAD,
    XINPUT_GAMEPAD
};

// Typed config structs for emulated devices (parsed from JSON at load time)
// These mirror rpcinterface.h structs but live on the mainboard side
struct KeyboardEmulatedDeviceConfig {};

struct MouseEmulatedDeviceConfig {};

struct HidGamepadEmulatedDeviceConfig {
    bool hat;
    uint16_t axesMask;                       // FLAG_MASK_GAMEPAD_AXIS_* bitmask
    uint8_t buttons;                         // Number of buttons (1-32)
};

struct Xbox360EmulatedDeviceConfig {};

struct ConfiguredRealDevice {
    std::string id;                          // Auto-generated unique ID
    std::string deviceId;                    // vendor/product/serial/capabilities
    DeviceType deviceType;
    std::vector<std::string> axes;           // List of axis names
    std::map<std::string, std::string> renameAxes; // old -> new axis name mapping
    bool active;                             // Connection status
};

struct LogicalDeviceConfig {
    std::string id;                          // Auto-generated ID
    std::string name;                        // User-provided name
    std::vector<std::string> axes;           // Dynamically expanded axis list
};

struct EmulationHostConfig {
    std::string id;                          // Pico-provided unique ID
    EmulationMode mode;
    bool active;                             // Connection status
};

struct EmulatedDeviceConfig {
    std::string id;                          // Auto-generated ID
    std::string name;                        // User-provided name
    EmulatedDeviceType type;
    std::string host;                        // EmulationHost ID
    union {
        KeyboardEmulatedDeviceConfig keyboard;
        MouseEmulatedDeviceConfig mouse;
        HidGamepadEmulatedDeviceConfig hidGamepad;
        Xbox360EmulatedDeviceConfig xbox360;
    } config;
};

struct RealToLogicalMapping {
    std::string real;                        // RealDevice ID
    std::string logical;                     // LogicalDevice ID
    int realIndexCache = -1;                 // Cached runtime index of real device
    int logicalIndexCache = -1;              // Cached runtime index of logical device
};

struct Profile {
    std::string name;
    bool enabled;
};

struct DeviceAndAxis {
    std::string deviceId;                    // Parsed device string (e.g. "inp_mouse_2")
    std::string axisName;                    // Parsed axis string (e.g. "MOUSE_BUTTON_LEFT")
    int deviceIndexCache = -1;               // Cached runtime device index
    int axisIndexCache = -1;                 // Cached runtime axis index
};

struct SimpleMappingEntry {
    DeviceAndAxis source;                    // Logical device + axis (input side)
    DeviceAndAxis target;                    // Emulated device + axis (output side)
};

struct SimpleMappingConfig {
    std::string id;                          // Unique ID (multiple SimpleMappingConfigs can exist)
    std::string profile;
    Profile*profileCache=NULL;
    std::vector<SimpleMappingEntry> entries;  // Pre-parsed mapping entries (no runtime string parsing)
    bool propagate;
    int priority;
    // Note: turbo and toggle are Phase 3 features - store but don't implement yet
};
```

**Key Methods:**
```cpp
class ConfigManager {
public:
    ConfigManager(const std::string& configPath);

    // Load/save configuration
    bool loadConfig();
    bool saveConfig();

    // Device queries
    ConfiguredRealDevice* findRealDevice(const std::string& id);
    ConfiguredRealDevice* findRealDeviceByDeviceId(const std::string& deviceId);
    LogicalDeviceConfig* findLogicalDevice(const std::string& id);
    EmulatedDeviceConfig* findEmulatedDevice(const std::string& id);
    EmulationHostConfig* findEmulationHost(const std::string& id);

    // Device registration
    std::string registerRealDevice(const std::string& deviceId,
                                   const std::string& deviceType,
                                   const std::vector<std::string>& axes);
    void markRealDeviceActive(const std::string& id, bool active);
    void markEmulationHostActive(const std::string& id, bool active);

    // Mapping queries
    RealToLogicalMapping* findRealToLogicalMapping(const std::string& realId);
    std::vector<SimpleMappingConfig> getEnabledSimpleMappings();

    // Cache management (for future WebUI: call after runtime config changes)
    void resetCaches();  // Resets all *Cache| fields in RealToLogicalMapping and SimpleMappingConfig

    // Ambiguous device IDs
    bool isAmbiguousDeviceId(const std::string& baseId) const;

private:
    std::string configPath;
    nlohmann::json configJson;

    // Parsed configuration
    std::vector<ConfiguredRealDevice> realDevices;
    std::vector<LogicalDeviceConfig> logicalDevices;
    std::vector<EmulationHostConfig> emulationHosts;
    std::vector<EmulatedDeviceConfig> emulatedDevices;
    std::vector<RealToLogicalMapping> realToLogicalMappings;
    std::vector<Profile> profiles;
    std::vector<SimpleMappingConfig> simpleMappings;
    std::vector<std::string> ambiguousDeviceIds;
};
```

**Dependencies:** nlohmann/json library (already used in project)

**Testing:**
- Load sample config.json
- Parse all sections correctly
- Handle missing/malformed JSON gracefully
- Save modified config back to disk
- Verify JSON round-trip preserves data

---

### 2. LogicalDevice Class
**File:** `mainboard/src/LogicalDevice.h`, `mainboard/src/LogicalDevice.cpp`

**Responsibilities:**
- Aggregate inputs from multiple physical devices
- Maintain dynamic axis list
- Route axis events to MappingManager
- Track current axis values (for combination detection in future phases)

**Data Structure:**
```cpp
class LogicalDevice {
public:
    LogicalDevice(const std::string& id, const std::string& name);

    // Configuration
    std::string getId() const { return id; }
    std::string getName() const { return name; }
    const std::vector<std::string>& getAxes() const { return axes; }

    // Axis management
    void addAxis(const std::string& axisName);
    bool hasAxis(const std::string& axisName) const;

    // Event handling
    void setAxis(const std::string& axisName, int value);
    int getAxisValue(const std::string& axisName) const;

    // Callback for forwarding events to MappingManager
    void setOnAxisEventCallback(
        std::function<void(const std::string& logicalDeviceId,
                          const std::string& axisName,
                          int value)> callback
    );

private:
    std::string id;
    std::string name;
    std::vector<std::string> axes;
    std::map<std::string, int> axisValues;  // Current axis states
    std::function<void(const std::string&, const std::string&, int)> onAxisEvent;
};
```

**Key Behaviors:**
- Dynamic axis expansion: when a physical device connects, copy all its axes to the logical device
- No validation of conflicting inputs (last write wins)
- Immediately forward axis changes to registered callback
- Store current values for future hotkey detection (Phase 2)

**Testing:**
- Create logical device with initial axes
- Add new axes dynamically
- Set axis values and verify callback fires
- Multiple physical devices writing to same logical device

---

### 3. EmulationHost Management
**File:** `mainboard/src/EmulationHostManager.h`, `mainboard/src/EmulationHostManager.cpp`

**Responsibilities:**
- Track active Pico boards
- Handle `onBoot` RPC signals
- Send full device state on boot
- Mark hosts as inactive on disconnect
- Route commands to correct host

**Data Structure:**
```cpp
struct EmulationHost {
    std::string id;          // Pico-provided ID
    std::string mode;        // "HID_MODE" or "XINPUT_MODE"
    bool active;             // Connection status
};

class EmulationHostManager {
public:
    EmulationHostManager(ConfigManager& config);

    // Lifecycle
    void handleOnBoot(const std::string& hostId);
    void markInactive(const std::string& hostId);

    // Queries
    EmulationHost* getHost(const std::string& hostId);
    bool isHostActive(const std::string& hostId) const;

    // Command routing (uses int indices matching RPC: setAxis(int device, int axis, int value))
    void sendPlugDevice(const std::string& hostId,
                       int deviceSlot,
                       const EmulatedDeviceConfig& device);
    void sendUnplugDevice(const std::string& hostId,
                         int deviceSlot);
    void sendAxisEvent(const std::string& hostId,
                      int deviceIndex,
                      int axisIndex,
                      int value);

    void setRpcClient(Main2Pico* client) { rpcClient = client; }

private:
    ConfigManager& configManager;
    std::map<std::string, EmulationHost> hosts;
    Main2Pico* rpcClient;  // Injected from main

    void sendFullDeviceState(const std::string& hostId);
};
```

**onBoot Flow:**
1. Receive `onBoot(hostId)` RPC call
2. Look up host in config (create new entry if not exists)
3. Mark host as `active: true`
4. Query all emulated devices assigned to this host
5. Send `plugDevice()` RPC for each device
6. Save updated config

---

### 4. EmulatedDevice Abstraction
**File:** `mainboard/src/EmulatedDevice.h`, `mainboard/src/EmulatedDevice.cpp`

**Responsibilities:**
- Represent virtual USB devices on Pico boards
- Send axis/button events to Pico
- Handle backward events (Phase 3, stub for now)

**Data Structure:**
```cpp
class EmulatedDevice {
public:
    EmulatedDevice(const EmulatedDeviceConfig& config,
                  EmulationHostManager& hostManager);

    // Configuration
    std::string getId() const { return config.id; }
    std::string getName() const { return config.name; }
    std::string getType() const { return config.type; }
    std::string getHostId() const { return config.host; }

    // Forward events (to Pico) - uses cached indices for RPC calls
    void setAxis(int axisIndex, int value);

    // Backward events (from Pico) - stub for Phase 3
    void onBackwardEvent(int axisIndex, int value);

    int getDeviceSlot() const { return deviceSlot; }

private:
    EmulatedDeviceConfig config;
    EmulationHostManager& hostManager;
    int deviceSlot = -1;                     // Cached slot index on the Pico host
};
```

**Key Behaviors:**
- `setAxis()` checks if host is active before sending
- Commands silently dropped if host inactive
- No queuing, no retry logic
- Backward events stubbed (will be implemented in Phase 3)


### 5. Basic MappingManager (Simple Mappings Only)
**File:** `mainboard/src/MappingManager.h`, `mainboard/src/MappingManager.cpp`

**Responsibilities (Phase 1 Scope):**
- Process simple_mapping rules only
- Sort by priority (descending)
- Handle propagate flag
- Forward events to emulated devices

**Data Structure:**
```cpp
struct ParsedSimpleMapping {
    DeviceAndAxis source;                    // Logical device + axis (with index caches)
    DeviceAndAxis target;                    // Emulated device + axis (with index caches)
    int priority;
    bool propagate;
};

class MappingManager {
public:
    MappingManager(ConfigManager& config);

    // Registration
    void registerLogicalDevice(LogicalDevice* device);
    void registerEmulatedDevice(EmulatedDevice* device);

    // Event processing
    void processAxisEvent(const std::string& logicalDeviceId,
                         const std::string& axisName,
                         int value);

    // Configuration reload
    void reloadMappings();

private:
    ConfigManager& configManager;
    std::map<std::string, LogicalDevice*> logicalDevices;
    std::map<std::string, EmulatedDevice*> emulatedDevices;

    // Parsed mappings grouped by source (index-based for fast runtime lookup)
    // Key: "logicalDeviceId:axisName" (string key, but entries contain cached indices)
    // Value: sorted list of mappings (by priority desc)
    std::map<std::string, std::vector<ParsedSimpleMapping>> mappingIndex;

    void rebuildMappingIndex();
    void resetMappingCaches();  // Reset all index caches (for future WebUI config changes)
    std::string makeKey(const std::string& deviceId, const std::string& axis);
};
```

**Mapping Execution Logic:**
```cpp
void MappingManager::processAxisEvent(const std::string& logicalDeviceId,
                                     const std::string& axisName,
                                     int value) {
    std::string key = makeKey(logicalDeviceId, axisName);

    auto it = mappingIndex.find(key);
    if (it == mappingIndex.end()) {
        return; // No mappings for this axis
    }

    // Execute mappings in priority order
    for (const auto& mapping : it->second) {
        // Find target emulated device (use cached index if available, fallback to string lookup)
        auto deviceIt = emulatedDevices.find(mapping.target.deviceId);
        if (deviceIt == emulatedDevices.end()) {
            continue; // Device not found, skip
        }

        // Send event to emulated device using cached axis index
        deviceIt->second->setAxis(mapping.target.axisIndexCache, value);

        // Stop if propagate is false
        if (!mapping.propagate) {
            break;
        }
    }
}
```

**Note:** Match mappings and hotkeys are Phase 2 features - not implemented in Phase 1.
---

### 6. Integration Layer
**File:** `mainboard/src/main.cpp` (modifications)

**Changes Required:**
1. Initialize ConfigManager on startup
2. Create LogicalDevice instances from config
3. Initialize EmulationHostManager
4. Create EmulatedDevice instances from config
5. Initialize MappingManager
6. Wire RealDeviceManager callbacks to LogicalDevices
7. Register onBoot handler for EmulationHost lifecycle

**Updated main() Flow:**
```cpp
int main() {
    // 1. Initialize EventLoop and UART
    uartManager = new UartManager(UART2);

    // 2. Initialize RPC system
    initRpcSystem();

    // 3. Load configuration
    ConfigManager configManager("./config.json");
    if (!configManager.loadConfig()) {
        return 1;
    }

    // 4. Initialize EmulationHostManager
    EmulationHostManager hostManager(configManager);
    hostManager.setRpcClient(&main2PicoRpcClient);

    // Register onBoot handler
    pico2MainRpcServer.onBoot = [&hostManager](std::string deviceId) -> bool {
        hostManager.handleOnBoot(deviceId);
        return true;
    };

    // 5. Create LogicalDevices
    std::map<std::string, LogicalDevice*> logicalDevices;
    for (auto& ldConfig : configManager.getLogicalDevices()) {
        auto* ld = new LogicalDevice(ldConfig.id, ldConfig.name);
        // Add axes from config
        for (const auto& axis : ldConfig.axes) {
            ld->addAxis(axis);
        }
        logicalDevices[ld->getId()] = ld;
    }

    // 6. Create EmulatedDevices
    std::map<std::string, EmulatedDevice*> emulatedDevices;
    for (auto& edConfig : configManager.getEmulatedDevices()) {
        auto* ed = new EmulatedDevice(edConfig, hostManager);
        emulatedDevices[ed->getId()] = ed;
    }

    // 7. Initialize MappingManager
    MappingManager mappingManager(configManager);
    for (auto& [id, ld] : logicalDevices) {
        mappingManager.registerLogicalDevice(ld);
    }
    for (auto& [id, ed] : emulatedDevices) {
        mappingManager.registerEmulatedDevice(ed);
    }

    // 8. Wire LogicalDevice callbacks to MappingManager
    for (auto& [id, ld] : logicalDevices) {
        ld->setOnAxisEventCallback(
            [&mappingManager](const std::string& logicalDeviceId,
                            const std::string& axisName,
                            int value) {
                mappingManager.processAxisEvent(logicalDeviceId, axisName, value);
            }
        );
    }

    // 9. Initialize RealDeviceManager
    RealDeviceManager deviceManager(
        eventLoop,
        configManager.getAmbiguousDeviceIds(),
        // onDeviceConnect
        [&](RealDevice* device) {
            handleRealDeviceConnect(device, configManager, logicalDevices);
        },
        // onDeviceDisconnect
        [&](RealDevice* device) {
            handleRealDeviceDisconnect(device, configManager);
        },
        // onInput
        [&](const std::string& deviceId, int axisIndex, int value) {
            handleRealDeviceInput(deviceId, axisIndex, value,
                                 configManager, deviceManager, logicalDevices);
        }
    );

    // 10. Start web interface
    initWebserver(main2PicoRpcClient);

    // 11. Run event loop
    eventLoop.runLoop();

    // Cleanup
    for (auto& [id, ld] : logicalDevices) delete ld;
    for (auto& [id, ed] : emulatedDevices) delete ed;

    return 0;
}
```

**Helper Functions:**
```cpp
void handleRealDeviceConnect(RealDevice* device,
                            ConfigManager& config,
                            std::map<std::string, LogicalDevice*>& logicalDevices) {
    // 1. Check if device already in config
    auto* configDevice = config.findRealDeviceByDeviceId(device->deviceId);

    if (!configDevice) {
        // 2. Register new device
        std::vector<std::string> axes;
        for (const auto& [name, index] : device->axisName2Index) {
            axes.push_back(name);
        }

        std::string deviceType = detectDeviceType(device); // Implement based on capabilities
        std::string configId = config.registerRealDevice(
            device->deviceId, deviceType, axes);

        config.saveConfig();
        configDevice = config.findRealDevice(configId);
    } else {
        // 3. Reactivate existing device
        config.markRealDeviceActive(configDevice->id, true);
        config.saveConfig();
    }

    // 4. Find logical device mapping
    auto* mapping = config.findRealToLogicalMapping(configDevice->id);
    if (!mapping) {
        std::cerr << "Warning: No logical device mapping for " << configDevice->id << std::endl;
        return;
    }

    // 5. Find logical device
    auto ldIt = logicalDevices.find(mapping->logical);
    if (ldIt == logicalDevices.end()) {
        std::cerr << "Warning: Logical device not found: " << mapping->logical << std::endl;
        return;
    }

    // 6. Copy axes from physical device to logical device
    for (const auto& [axisName, index] : device->axisName2Index) {
        // Apply axis renaming if configured
        std::string finalName = axisName;
        for (const auto& [oldName, newName] : configDevice->renameAxes) {
            if (axisName == oldName) {
                finalName = newName;
                break;
            }
        }

        if (!ldIt->second->hasAxis(finalName)) {
            ldIt->second->addAxis(finalName);
        }
    }

    std::cout << "Real device connected: " << configDevice->id
              << " -> Logical device: " << mapping->logical << std::endl;
}

void handleRealDeviceDisconnect(RealDevice* device, ConfigManager& config) {
    auto* configDevice = config.findRealDeviceByDeviceId(device->deviceId);
    if (configDevice) {
        config.markRealDeviceActive(configDevice->id, false);
        config.saveConfig();
    }
}

void handleRealDeviceInput(const std::string& deviceId, int axisIndex, int value,
                          ConfigManager& config,
                          RealDeviceManager& deviceManager,
                          std::map<std::string, LogicalDevice*>& logicalDevices) {
    // 1. Find device in config
    auto* configDevice = config.findRealDeviceByDeviceId(deviceId);
    if (!configDevice || !configDevice->active) {
        return; // Device not configured or inactive
    }

    // 2. Find logical device mapping
    auto* mapping = config.findRealToLogicalMapping(configDevice->id);
    if (!mapping) {
        return; // No mapping configured
    }

    // 3. Find logical device
    auto ldIt = logicalDevices.find(mapping->logical);
    if (ldIt == logicalDevices.end()) {
        return;
    }

    // 4. Get axis name from physical device
    RealDevice* realDevice = deviceManager.getDevice(deviceId);
    if (!realDevice) {
        return;
    }

    auto axisIt = realDevice->axisIndex2Name.find(axisIndex);
    if (axisIt == realDevice->axisIndex2Name.end()) {
        return;
    }

    std::string axisName = axisIt->second;

    // 5. Apply axis renaming if configured
    for (const auto& [oldName, newName] : configDevice->renameAxes) {
        if (axisName == oldName) {
            axisName = newName;
            break;
        }
    }

    // 6. Forward to logical device (which will trigger mappings)
    ldIt->second->setAxis(axisName, value);
}
```

---

## Implementation Order

### Week 1: Configuration and Data Structures
1. Implement ConfigManager with JSON parsing
2. Create LogicalDevice class with basic functionality

### Week 2: EmulationHost and Devices
1. Implement EmulationHostManager
2. Wire up onBoot RPC handler
3. Create EmulatedDevice abstraction

### Week 3: Mapping System
1. Implement basic MappingManager (simple_mapping only)
2. Parse mapping configurations
3. Build mapping index

### Week 4: Integration
1. Integrate all components in main.cpp
2. Wire RealDeviceManager to LogicalDevices

---

## Success Criteria

Phase 1 is complete when:
- ✅ Configuration loads from config.json on startup
- ✅ Physical devices auto-register on connection
- ✅ Physical devices reconnect to existing config entries
- ✅ Axis events flow from physical devices to logical devices
- ✅ Simple mappings route events to emulated devices correctly
- ✅ Multiple mappings execute in priority order
- ✅ propagate=false stops mapping execution
- ✅ Pico boards boot and receive full device state
- ✅ Emulated devices send axis events via RPC
- ✅ Inactive hosts don't receive commands
- ✅ Configuration persists changes to disk

---

## Out of Scope for Phase 1

The following features are intentionally deferred:
- ❌ Match mappings (Phase 2)
- ❌ Hotkey detection (Phase 2)
- ❌ Turbo mode (Phase 3)
- ❌ Toggle mode (Phase 3)
- ❌ Backward mappings (Phase 3)
- ❌ Profile switching UI (Phase 3)
- ❌ Web UI for device management (Phase 4)
- ❌ Logical device chaining (Phase 4)
- ❌ Lua scripting (Phase 4)

---

## Dependencies

### External Libraries
- **nlohmann/json**: JSON parsing (likely already in project)
- **libevdev**: Linux input device handling (already used by RealDeviceManager)
- **SimpleRPC**: UART communication (already implemented)

### Existing Code
- EventLoop: Non-blocking event processing
- UartManager: Serial communication
- RealDeviceManager: Physical device handling
- RPC infrastructure: Pico communication

---

## Risk Mitigation

### Risk: Config corruption
**Mitigation:**
- Backup config.json before writing
- Validate JSON structure before saving
- Graceful fallback to defaults on parse errors

### Risk: Device ID collisions
**Mitigation:**
- Use ambiguous_device_ids for problematic devices
- Append /dev/input/eventX as fallback
- Log warnings for duplicate IDs

### Risk: Mapping misconfiguration
**Mitigation:**
- Skip invalid mappings with warning logs
- Don't crash on missing device references
- Provide clear error messages

### Risk: RPC failures
**Mitigation:**
- Drop commands silently (no retry/queue)
- Rely on Pico reboot + onBoot recovery
- Mark hosts inactive on communication failure

---

## Performance Considerations

- **Mapping lookup**: O(log n) with sorted index
- **Event propagation**: O(m) where m = number of matching mappings
- **Config loading**: One-time cost at startup

---

## Notes

- Keep simple_mapping implementation minimal - no turbo/toggle in Phase 1
- Focus on stability and correct event routing
- Prepare data structures for Phase 2 features (hotkey state tracking)
- Document assumptions and limitations clearly

### Index Cache Strategy
- All device/axis references use string IDs in config, but cache integer indices at runtime
- Caches are populated during `loadConfig()` / `rebuildMappingIndex()` by looking up string IDs against registered devices
- `ConfigManager::resetCaches()` invalidates all index caches (sets to -1) across:
  - `RealToLogicalMapping::realIndexCache`, `logicalIndexCache`
  - `SimpleMappingConfig` entries: all `DeviceAndAxis::deviceIndexCache`, `axisIndexCache`
- This is needed for future WebUI: when user changes mappings at runtime, call `resetCaches()` then re-populate
- Cache value of -1 means "not yet resolved" — lookup falls back to string-based search
- Matches RPC interface: `Main2Pico::setAxis(int device, int axis, int value)`, `plugDevice(int slot, DeviceConfiguration)`
