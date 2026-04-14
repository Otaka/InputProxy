# Layer Enhancements Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend the layer system with three new rule types (`block`, `vod_state`, `turbo`) and hotkey-driven layer activation triggers (`toggle`, `while_active`, `while_not_active`).

**Architecture:** All new behavior is expressed as rules inside the existing `rules[]` array. Three new structs (`BlockRule`, `VodStateRule`, `TurboRule`) live in `Layer`. The `dispatchVidAxisEvent` function is extended to evaluate new rules in priority order before existing AxisRule processing. Turbo loops run as `corocgo::coro()` coroutines. Activation triggers reuse the existing hotkey parser and are evaluated on every axis event.

**Tech Stack:** C++20, nlohmann/json (already in use), corocgo (`corocgo::coro()` / `corocgo::sleep()` for turbo coroutine), existing RPC infrastructure for VOD disconnect.

---

## File Map

| Action | File | What changes |
|--------|------|-------------|
| Modify | `mainboard/src/MainConfig.h` | Add `ConfRuleType::Block/VodState/Turbo`, `ConfBlockAxis`, `ConfActivation`, extend `ConfRule` + `ConfLayer` |
| Modify | `mainboard/src/MainConfig.cpp` | Parse new rule fields + layer activation block |
| **Create** | `mainboard/src/mapping/BlockRule.h` | `BlockRule` struct |
| **Create** | `mainboard/src/mapping/VodStateRule.h` | `VodStateRule` struct + `VodState` enum |
| **Create** | `mainboard/src/mapping/TurboRule.h` | `TurboRule` struct + `TurboCondition` enum |
| Modify | `mainboard/src/mapping/Layer.h` | Add `blockRules`, `vodStateRules`, `turboRules`, `activation` vectors |
| Modify | `mainboard/src/mapping/LayerManager.h` | Add `LayerActivation` struct; expose `activateRaw`/`deactivateRaw` for trigger use |
| Modify | `mainboard/src/mapping/MappingManager.h` | New private methods; turbo state map |
| Modify | `mainboard/src/mapping/MappingManager.cpp` | Load new rules; dispatch block/turbo/trigger; evaluate VOD states |
| Modify | `mainboard/src/emulation/EmulatedDeviceManager.h` | Add `setSilenced(vodId, bool)`, `isSilenced()` |
| Modify | `mainboard/src/emulation/EmulatedDeviceManager.cpp` | Enforce silence in `setAxis` |
| Modify | `shared/rpcinterface.h` | Add `M2P_SET_USB_CONNECTED = 10` |
| Modify | `mainboard/src/emulation/EmulationBoard.h` | Add `setUsbConnected(bool)` |
| Modify | `Pico/src/...` | Handle `M2P_SET_USB_CONNECTED` via `tud_disconnect()`/`tud_connect()` |

---

## Task 1: Config — new enum values and structs

**Files:**
- Modify: `mainboard/src/MainConfig.h`

- [ ] **Step 1: Add new ConfRuleType values and supporting structs to MainConfig.h**

Find `enum class ConfRuleType` and `struct ConfRule` and `struct ConfLayer` and extend them:

```cpp
// In MainConfig.h — extend enum
enum class ConfRuleType { Simple, Hotkey, Block, VodState, Turbo };

// In MainConfig.h — add after ConfAxisEntry
enum class ConfVodState { Active, Silenced, Disconnected };
enum class ConfTurboCondition { WhileAxisActive, Always };
enum class ConfActivationMode { Toggle, WhileActive, WhileNotActive };

struct ConfBlockAxis {
    std::string axis;
    int         value = 0;
};

struct ConfActivation {
    ConfActivationMode mode   = ConfActivationMode::Toggle;
    std::string        vid;
    std::string        hotkey;
};

// In ConfRule — add new fields after existing ones:
//   std::vector<ConfBlockAxis> blockAxes;  // used by Block
//   ConfVodState               vodState = ConfVodState::Active; // used by VodState
//   std::string                turboAxis;  // used by Turbo
//   int                        turboOnMs        = 100;
//   int                        turboOffMs       = 100;
//   int                        turboInitialDelay = 0;
//   int                        turboMaxValue    = 1000;
//   int                        turboMinValue    = 0;
//   ConfTurboCondition         turboCondition   = ConfTurboCondition::WhileAxisActive;

// In ConfLayer — add after rules:
//   std::optional<ConfActivation> activation;
```

Full updated `ConfRule`:
```cpp
struct ConfRule {
    ConfRuleType               type      = ConfRuleType::Simple;
    std::string                vid;
    std::string                vod;
    std::vector<ConfAxisEntry> axes;
    std::string                hotkey;
    bool                       propagate = false;
    std::vector<ConfAction>    pressActions;
    std::vector<ConfAction>    releaseActions;
    // Block
    std::vector<ConfBlockAxis> blockAxes;
    // VodState
    ConfVodState               vodState  = ConfVodState::Active;
    // Turbo
    std::string                turboAxis;
    int                        turboOnMs         = 100;
    int                        turboOffMs        = 100;
    int                        turboInitialDelay = 0;
    int                        turboMaxValue     = 1000;
    int                        turboMinValue     = 0;
    ConfTurboCondition         turboCondition    = ConfTurboCondition::WhileAxisActive;
};
```

Full updated `ConfLayer`:
```cpp
struct ConfLayer {
    std::string                    id;
    std::string                    name;
    bool                           active = true;
    std::vector<ConfRule>          rules;
    std::optional<ConfActivation>  activation;
};
```

- [ ] **Step 2: Build to confirm changes compile**

```bash
cd mainboard && cmake --build build 2>&1 | head -40
```

Expected: build errors only in MainConfig.cpp where new fields aren't parsed yet (not in any headers).

- [ ] **Step 3: Commit**

```bash
git add mainboard/src/MainConfig.h
git commit -m "feat(config): add ConfRuleType Block/VodState/Turbo and activation structs"
```

---

## Task 2: Config — parse new rule fields and activation block

**Files:**
- Modify: `mainboard/src/MainConfig.cpp`

- [ ] **Step 1: Add parsing for block axes in `confRuleFromJson`**

Locate `confRuleFromJson` in `mainboard/src/MainConfig.cpp`. After the existing field parsing, add:

```cpp
// After existing r.propagate = ... line:
if (typeStr == "block") {
    r.type = ConfRuleType::Block;
    for (const auto& ax : j.value("axes", json::array())) {
        ConfBlockAxis ba;
        ba.axis  = ax.value("axis", "");
        ba.value = ax.value("value", 0);
        if (!ba.axis.empty()) r.blockAxes.push_back(ba);
    }
} else if (typeStr == "vod_state") {
    r.type = ConfRuleType::VodState;
    std::string stateStr = j.value("state", "active");
    if      (stateStr == "silenced")     r.vodState = ConfVodState::Silenced;
    else if (stateStr == "disconnected") r.vodState = ConfVodState::Disconnected;
    else                                 r.vodState = ConfVodState::Active;
} else if (typeStr == "turbo") {
    r.type = ConfRuleType::Turbo;
    r.turboAxis          = j.value("axis", "");
    r.turboOnMs          = j.value("on_ms", 100);
    r.turboOffMs         = j.value("off_ms", 100);
    r.turboInitialDelay  = j.value("initial_delay_ms", 0);
    r.turboMaxValue      = j.value("max_value", 1000);
    r.turboMinValue      = j.value("min_value", 0);
    std::string cond     = j.value("condition", "while_axis_active");
    r.turboCondition     = (cond == "always")
                         ? ConfTurboCondition::Always
                         : ConfTurboCondition::WhileAxisActive;
}
```

Also update the existing type-string parsing to not push an error for the new types (remove or guard the `errors.push_back("unknown rule type")` line so it only fires for truly unknown types).

- [ ] **Step 2: Add parsing for layer activation block in `confLayerFromJson`**

Locate `confLayerFromJson`. After `l.rules` parsing, add:

```cpp
auto actIt = j.find("activation");
if (actIt != j.end()) {
    ConfActivation act;
    std::string modeStr = actIt->value("mode", "toggle");
    if      (modeStr == "while_active")     act.mode = ConfActivationMode::WhileActive;
    else if (modeStr == "while_not_active") act.mode = ConfActivationMode::WhileNotActive;
    else                                    act.mode = ConfActivationMode::Toggle;
    act.vid    = actIt->value("vid", "");
    act.hotkey = actIt->value("hotkey", "");
    l.activation = act;
}
```

- [ ] **Step 3: Build to confirm parsing compiles**

```bash
cd mainboard && cmake --build build 2>&1 | head -40
```

Expected: successful build (new config fields are parsed but not used yet by MappingManager).

- [ ] **Step 4: Commit**

```bash
git add mainboard/src/MainConfig.cpp
git commit -m "feat(config): parse block/vod_state/turbo rules and layer activation"
```

---

## Task 3: New rule structs and Layer.h update

**Files:**
- Create: `mainboard/src/mapping/BlockRule.h`
- Create: `mainboard/src/mapping/VodStateRule.h`
- Create: `mainboard/src/mapping/TurboRule.h`
- Modify: `mainboard/src/mapping/Layer.h`

- [ ] **Step 1: Create BlockRule.h**

```cpp
// mainboard/src/mapping/BlockRule.h
#pragma once
#include <string>
#include <vector>

struct BlockEntry {
    std::string axisName;   // retained for resolution
    int         axisIndex = -1;
    int         value     = 0;  // value to hold the axis at when blocked
};

struct BlockRule {
    std::string             vidId;
    std::vector<BlockEntry> entries;
};
```

- [ ] **Step 2: Create VodStateRule.h**

```cpp
// mainboard/src/mapping/VodStateRule.h
#pragma once
#include <string>

enum class VodState { Active, Silenced, Disconnected };

struct VodStateRule {
    std::string vodId;
    VodState    state = VodState::Active;
};
```

- [ ] **Step 3: Create TurboRule.h**

```cpp
// mainboard/src/mapping/TurboRule.h
#pragma once
#include <string>

enum class TurboCondition { WhileAxisActive, Always };

struct TurboRule {
    std::string    vidId;
    std::string    axisName;   // retained for resolution
    int            axisIndex = -1;
    int            onMs         = 100;
    int            offMs        = 100;
    int            initialDelay = 0;
    int            maxValue     = 1000;
    int            minValue     = 0;
    TurboCondition condition    = TurboCondition::WhileAxisActive;
};
```

- [ ] **Step 4: Update Layer.h to include new rules and activation**

Add includes and new member vectors to `struct Layer`:

```cpp
// Add to includes at top of Layer.h:
#include "BlockRule.h"
#include "VodStateRule.h"
#include "TurboRule.h"
#include "../MainConfig.h"   // for ConfActivation (already included transitively or add directly)
#include <optional>

// Add to struct Layer body (after existing members):
    std::vector<BlockRule>    blockRules;
    std::vector<VodStateRule> vodStateRules;
    std::vector<TurboRule>    turboRules;
    std::optional<ConfActivation> activation;   // from config
    bool                      toggleState = false; // for Toggle activation mode
```

- [ ] **Step 5: Build**

```bash
cd mainboard && cmake --build build 2>&1 | head -40
```

Expected: successful build.

- [ ] **Step 6: Commit**

```bash
git add mainboard/src/mapping/BlockRule.h \
        mainboard/src/mapping/VodStateRule.h \
        mainboard/src/mapping/TurboRule.h \
        mainboard/src/mapping/Layer.h
git commit -m "feat(mapping): add BlockRule, VodStateRule, TurboRule structs; extend Layer"
```

---

## Task 4: Load new rules in MappingManager + resolve their axes

**Files:**
- Modify: `mainboard/src/mapping/MappingManager.cpp`

- [ ] **Step 1: Add includes in MappingManager.cpp**

At the top of `mainboard/src/mapping/MappingManager.cpp`, the existing includes already pull in `Layer.h` transitively. Confirm `#include "corocgo/corocgo.h"` is present (it is — already there). No new includes needed.

- [ ] **Step 2: Populate new rule types in `MappingManager::load()`**

In the rule-loading loop in `load()`, after the existing `else if (rc.type == ConfRuleType::Hotkey)` block, add:

```cpp
} else if (rc.type == ConfRuleType::Block) {
    BlockRule br;
    br.vidId = vidId;
    for (const auto& ba : rc.blockAxes) {
        BlockEntry e;
        e.axisName = ba.axis;
        e.value    = ba.value;
        br.entries.push_back(e);
    }
    layer.blockRules.push_back(std::move(br));

} else if (rc.type == ConfRuleType::VodState) {
    VodStateRule vsr;
    vsr.vodId = rc.vod;
    switch (rc.vodState) {
        case ConfVodState::Silenced:     vsr.state = VodState::Silenced;     break;
        case ConfVodState::Disconnected: vsr.state = VodState::Disconnected; break;
        default:                         vsr.state = VodState::Active;        break;
    }
    layer.vodStateRules.push_back(vsr);

} else if (rc.type == ConfRuleType::Turbo) {
    TurboRule tr;
    tr.vidId        = vidId;
    tr.axisName     = rc.turboAxis;
    tr.onMs         = rc.turboOnMs;
    tr.offMs        = rc.turboOffMs;
    tr.initialDelay = rc.turboInitialDelay;
    tr.maxValue     = rc.turboMaxValue;
    tr.minValue     = rc.turboMinValue;
    tr.condition    = (rc.turboCondition == ConfTurboCondition::Always)
                    ? TurboCondition::Always
                    : TurboCondition::WhileAxisActive;
    layer.turboRules.push_back(tr);
```

Also copy the `activation` block from `ConfLayer` to `Layer`:

```cpp
// After the rule loop, before pushing to layerManager.allLayers:
layer.activation = lc.activation;
```

- [ ] **Step 3: Resolve BlockRule and TurboRule axis indices in `resolveVidAxes()`**

After the existing loop that resolves `layer.rules`, add:

```cpp
// Resolve BlockRule axis indices
for (auto& layer : layerManager.allLayers) {
    for (auto& br : layer.blockRules) {
        auto vidIt = vids.find(br.vidId);
        if (vidIt == vids.end()) continue;
        for (auto& e : br.entries) {
            if (e.axisIndex == -1 && !e.axisName.empty())
                e.axisIndex = vidIt->second.axisTable.getIndex(e.axisName);
        }
    }
    for (auto& tr : layer.turboRules) {
        auto vidIt = vids.find(tr.vidId);
        if (vidIt == vids.end()) continue;
        if (tr.axisIndex == -1 && !tr.axisName.empty())
            tr.axisIndex = vidIt->second.axisTable.getIndex(tr.axisName);
    }
}
```

Note: `resolveVidAxes()` is called from `onRealDeviceConnected()`. This is sufficient — axis resolution runs whenever a device connects, same as for existing rules.

- [ ] **Step 4: Build**

```bash
cd mainboard && cmake --build build 2>&1 | head -40
```

Expected: successful build.

- [ ] **Step 5: Commit**

```bash
git add mainboard/src/mapping/MappingManager.cpp
git commit -m "feat(mapping): load block/vod_state/turbo rules and resolve axes"
```

---

## Task 5: Block rule dispatch

**Files:**
- Modify: `mainboard/src/mapping/MappingManager.cpp`

- [ ] **Step 1: Evaluate block rules in `dispatchVidAxisEvent` before AxisRule processing**

In `dispatchVidAxisEvent`, at the start of the `for (Layer* layer : layerManager.stack())` loop body, before the `if (value == 0)` check, add block rule evaluation:

```cpp
// Check block rules first (highest priority within layer)
for (auto& br : layer->blockRules) {
    if (br.vidId != vidId) continue;
    for (const auto& e : br.entries) {
        if (e.axisIndex == vidAxisIndex) {
            // Update vidState to the blocked value so downstream rules see it correctly
            vidState[vidId][vidAxisIndex] = e.value;
            // Dispatch the blocked value downstream (lower layers still see it as "e.value")
            // but stop processing this layer's AxisRules — fall through to next layer
            // Actually: block consumes the event entirely.
            // Send the block value through the remaining layers' AxisRules:
            // The simplest model: emit the block value, mark consumed, break.
            // Re-dispatch with blocked value through lower layers:
            // No — block means: no further rule in ANY layer processes this axis event.
            // This matches "consumed = true; break out of layer loop entirely"
            // But we need to send e.value somewhere — the block rule itself doesn't map to a VOD.
            // The downstream *simple* rules in lower layers would normally do the mapping.
            // So: replace the incoming value with e.value and let lower layers process it,
            // but prevent THIS layer's AxisRules from seeing it.
            // Implementation: modify `value` to e.value, skip this layer's AxisRules,
            // continue to the next layer with the replaced value.
            // This requires restructuring the loop slightly — see below.
            goto block_found;
        }
    }
}
goto no_block;
block_found:
// A block rule matched in this layer: replace value and skip this layer's AxisRule processing.
// The replaced value flows to lower-priority layers normally.
// (vidState already updated above)
// We don't set consumed=true here — lower layers CAN process this (with replaced value).
// Just continue to the next layer.
continue;  // next Layer* in stack
no_block:
```

Wait — `goto` across `continue` into a for-loop label is invalid C++. Use a helper lambda or a bool flag instead:

```cpp
// At the start of the layer loop body:
bool blockedInThisLayer = false;
int  effectiveValue     = value;

for (auto& br : layer->blockRules) {
    if (br.vidId != vidId) continue;
    for (const auto& e : br.entries) {
        if (e.axisIndex == vidAxisIndex) {
            effectiveValue     = e.value;
            blockedInThisLayer = true;
            vidState[vidId][vidAxisIndex] = e.value;
            break;
        }
    }
    if (blockedInThisLayer) break;
}

if (blockedInThisLayer) {
    // This layer's AxisRules are skipped; lower layers see effectiveValue.
    // We still update the outer `value` so lower layers use the blocked value.
    // (Use a mutable reference or replace the dispatch logic — see note below.)
    // Simplest: re-enter dispatchVidAxisEvent with effectiveValue for remaining layers.
    // Since we're inside the loop, just: break out with a flag to re-dispatch.
    // Easiest restructuring: extract the remaining-layers dispatch into a helper.
    // For now: consume the event entirely (lower layers see nothing).
    // This matches the spec: block = suppress and hold at value.
    // The block value is emitted via: force-update vidState and emit to all active simple rules.
    // Actually per spec: block prevents the real event from going to downstream mappings.
    // The blocked value (e.g. 0) is effectively what the VID "looks like" to the system.
    // The simplest correct implementation: consume the event, emit effectiveValue to VODs
    // by running only the AxisRules of lower-priority layers with effectiveValue.
    // TODO: for now, just consume the event (simpler, correct for value=0 blocks).
    break;  // consumed = true will be set after this block; see step 2
}
```

Actually, re-read the spec: "Consumes the event — no lower-priority rule will see it." So block = full consume. The blocked value (e.g. 0) is set in `vidState` so that future hotkey evaluations see it, but no AxisRule mapping fires for this event. This is the simplest correct behavior.

Replace the block rule check with this clean version at the **top** of the layer loop body (before the `value == 0` check):

```cpp
// --- Block rule check ---
{
    bool blocked = false;
    int  blockValue = value;
    for (auto& br : layer->blockRules) {
        if (br.vidId != vidId) continue;
        for (const auto& e : br.entries) {
            if (e.axisIndex == vidAxisIndex) {
                blockValue = e.value;
                blocked    = true;
                break;
            }
        }
        if (blocked) break;
    }
    if (blocked) {
        vidState[vidId][vidAxisIndex] = blockValue;
        break;  // consumed — exit the layer stack loop
    }
}
// --- End block rule check ---
```

- [ ] **Step 2: Send block value on layer deactivation (cleanup)**

In `LayerManager::deactivate()` (file: `mainboard/src/mapping/LayerManager.cpp`), after `(*it)->resetActiveRules()`, add a callback hook so that MappingManager can emit cleanup values. The cleanest approach: give `LayerManager` a `std::function<void(Layer*)>` deactivation callback.

In `mainboard/src/mapping/LayerManager.h`, add:

```cpp
#include <functional>
// In class LayerManager:
    std::function<void(Layer*)> onDeactivate;  // optional callback, called before layer is removed
```

In `LayerManager::deactivate()` in `LayerManager.cpp`, call it:

```cpp
if (onDeactivate) onDeactivate(*it);
(*it)->resetActiveRules();
activeStack.erase(it);
```

In `MappingManager::load()`, after setting up layerManager, wire the callback:

```cpp
layerManager.onDeactivate = [this](Layer* layer) {
    for (auto& br : layer->blockRules) {
        for (auto& e : br.entries) {
            if (e.axisIndex == -1) continue;
            if (e.value == 0) continue;  // was already 0, nothing to clean up
            // Send 0 to unpress the axis — run it through dispatchVidAxisEvent
            vidState[br.vidId][e.axisIndex] = 0;
            dispatchVidAxisEvent(br.vidId, e.axisIndex, 0);
        }
    }
};
```

- [ ] **Step 3: Build**

```bash
cd mainboard && cmake --build build 2>&1 | head -40
```

Expected: successful build.

- [ ] **Step 4: Manual test — load config with block rule**

Add a test layer to `mainboard/config.json`:

```json
{
    "id": "test-block",
    "name": "Block Test",
    "active": true,
    "rules": [
        {
            "type": "block",
            "vid": "vid1",
            "axes": [
                { "axis": "ButtonA", "value": 0 }
            ]
        }
    ]
}
```

Run the app and press ButtonA on the controller — the mapped output button should not fire.
Check log output for `[mapping] loaded N layer(s)` — should include test-block.

- [ ] **Step 5: Remove test layer from config, commit**

```bash
git add mainboard/src/mapping/MappingManager.cpp \
        mainboard/src/mapping/LayerManager.h \
        mainboard/src/mapping/LayerManager.cpp
git commit -m "feat(mapping): block rule dispatch and deactivation cleanup"
```

---

## Task 6: VOD silence (vod_state = silenced)

**Files:**
- Modify: `mainboard/src/emulation/EmulatedDeviceManager.h`
- Modify: `mainboard/src/emulation/EmulatedDeviceManager.cpp`
- Modify: `mainboard/src/mapping/MappingManager.h`
- Modify: `mainboard/src/mapping/MappingManager.cpp`

- [ ] **Step 1: Add silence tracking to EmulatedDeviceManager**

In `EmulatedDeviceManager.h`, add:

```cpp
#include <set>
#include <string>
// In class EmulatedDeviceManager:
    void setSilenced(const std::string& vodId, bool silenced);
    bool isSilenced(const std::string& vodId) const;

private:
    std::set<std::string> silencedVods;
```

In `EmulatedDeviceManager.cpp`, add:

```cpp
void EmulatedDeviceManager::setSilenced(const std::string& vodId, bool silenced) {
    if (silenced) silencedVods.insert(vodId);
    else          silencedVods.erase(vodId);
}

bool EmulatedDeviceManager::isSilenced(const std::string& vodId) const {
    return silencedVods.count(vodId) > 0;
}
```

Modify `EmulatedDeviceManager::setAxis` to check silence:

```cpp
void EmulatedDeviceManager::setAxis(int deviceIndex, int axis, int value) {
    if (deviceIndex < 0 || deviceIndex >= (int)devices.size()) return;
    auto& d = devices[deviceIndex];
    if (d.board == nullptr || !d.board->active) return;
    if (silencedVods.count(d.id) > 0) return;   // <-- add this line
    d.setAxis(axis, value);
}
```

- [ ] **Step 2: Add `evaluateVodStates()` to MappingManager**

This method re-derives the silence state for each VOD from the active layer stack and syncs it to `EmulatedDeviceManager`.

In `MappingManager.h`, declare:

```cpp
private:
    void evaluateVodStates();
```

In `MappingManager.cpp`, implement:

```cpp
void MappingManager::evaluateVodStates() {
    // Collect all known VOD ids from EDM
    const auto& devices = edm->getDevices();
    for (const auto& dev : devices) {
        const std::string& vodId = dev.id;
        VodState effectiveState  = VodState::Active;  // default

        // Highest-priority active layer with a vod_state rule wins
        for (Layer* layer : layerManager.stack()) {
            for (const auto& vsr : layer->vodStateRules) {
                if (vsr.vodId == vodId) {
                    effectiveState = vsr.state;
                    goto found;
                }
            }
        }
        found:

        edm->setSilenced(vodId, effectiveState == VodState::Silenced);
        // Disconnected handled in Task 7
    }
}
```

- [ ] **Step 3: Call evaluateVodStates() from LayerManager activate/deactivate callbacks**

In `MappingManager::load()`, add a second callback for activation:

```cpp
layerManager.onActivate = [this](Layer*) { evaluateVodStates(); };
```

In `LayerManager.h`, add:

```cpp
std::function<void(Layer*)> onActivate;
```

In `LayerManager.cpp`, call it in `activate()`:

```cpp
activeStack.insert(activeStack.begin(), layer);
if (onActivate) onActivate(layer);
```

Also call `evaluateVodStates()` inside the existing `onDeactivate` callback (append to the lambda in `load()`):

```cpp
layerManager.onDeactivate = [this](Layer* layer) {
    // ... existing block cleanup ...
    evaluateVodStates();
};
```

And call it once at the end of `load()` after all layers are loaded:

```cpp
evaluateVodStates();
```

- [ ] **Step 4: Build**

```bash
cd mainboard && cmake --build build 2>&1 | head -40
```

Expected: successful build.

- [ ] **Step 5: Manual test — silence a VOD via REST API**

Start the app. Activate a layer with `vod_state silenced` via REST:

```bash
curl -X POST http://raspberrypi.local:8080/layers/test-silence/activate
```

Confirm that controller inputs no longer reach the silenced output device. Deactivate and confirm they resume.

- [ ] **Step 6: Commit**

```bash
git add mainboard/src/emulation/EmulatedDeviceManager.h \
        mainboard/src/emulation/EmulatedDeviceManager.cpp \
        mainboard/src/mapping/MappingManager.h \
        mainboard/src/mapping/MappingManager.cpp \
        mainboard/src/mapping/LayerManager.h \
        mainboard/src/mapping/LayerManager.cpp
git commit -m "feat(mapping): vod_state silenced — suppress all axis output to a VOD"
```

---

## Task 7: VOD disconnect (vod_state = disconnected)

This requires a new RPC call and Pico firmware changes.

**Files:**
- Modify: `shared/rpcinterface.h`
- Modify: `mainboard/src/emulation/EmulationBoard.h`
- Modify: `Pico/src/main.cpp` (or wherever RPC handlers are registered)
- Modify: `mainboard/src/mapping/MappingManager.cpp`

- [ ] **Step 1: Add M2P_SET_USB_CONNECTED to rpcinterface.h**

```cpp
// In enum Main2PicoMethod, add:
M2P_SET_USB_CONNECTED = 10, /* args: bool connected | returns: void
                               false = tud_disconnect(); true = tud_connect() */
```

- [ ] **Step 2: Add setUsbConnected() to EmulationBoard.h**

```cpp
void setUsbConnected(bool connected) {
    corocrpc::RpcArg* arg = rpc->getRpcArg();
    arg->putBool(connected);
    rpc->callNoResponse(M2P_SET_USB_CONNECTED, arg);
    rpc->disposeRpcArg(arg);
}
```

- [ ] **Step 3: Add Pico handler for M2P_SET_USB_CONNECTED**

In the Pico firmware, locate where RPC methods are registered (look for `rpc.registerMethod(M2P_SET_AXIS, ...)` and add nearby):

```cpp
rpc.registerMethod(M2P_SET_USB_CONNECTED, [](corocrpc::RpcArg* arg, corocrpc::RpcReturn*) {
    bool connected = arg->getBool();
    if (connected) tud_connect();
    else           tud_disconnect();
});
```

`tud_connect()` and `tud_disconnect()` are TinyUSB Device API functions available when `TUSB_OPT_DEVICE_ENABLED` is set. Verify they are available in the Pico SDK version in use (Pico SDK 2.2.0 — they are available).

- [ ] **Step 4: Track USB connected state per board in MappingManager**

In `MappingManager.h`, add:

```cpp
#include <set>
// private:
    std::set<std::string> usbDisconnectedBoards;  // board serial IDs currently disconnected
```

In `evaluateVodStates()`, extend the loop to handle `Disconnected` state:

```cpp
// After setting silenced, check disconnected:
// Find which board owns this VOD
// (EmulatedDeviceManager::getDevices() returns VirtualOutputDevice which has board pointer)
int devIdx = edm->resolveId(vodId);
if (devIdx == -1) continue;
const auto& devs = edm->getDevices();
EmulationBoard* board = devs[devIdx].board;
if (!board) continue;

bool shouldDisconnect = (effectiveState == VodState::Disconnected);
bool wasDisconnected  = usbDisconnectedBoards.count(board->serialString) > 0;

if (shouldDisconnect != wasDisconnected) {
    if (shouldDisconnect) {
        usbDisconnectedBoards.insert(board->serialString);
        board->setUsbConnected(false);
    } else {
        usbDisconnectedBoards.erase(board->serialString);
        board->setUsbConnected(true);
    }
}
```

Note: `VirtualOutputDevice` needs a `board` pointer — check `VirtualOutputDevice.h` to confirm this field exists. If not, it can be looked up via `EmulationBoard`'s device list.

- [ ] **Step 5: Build both mainboard and Pico**

```bash
cd mainboard && cmake --build build 2>&1 | tail -5
cd ../Pico && cmake --build build 2>&1 | tail -5
```

Expected: both build successfully.

- [ ] **Step 6: Manual test**

Flash updated Pico firmware. Add a `vod_state: disconnected` layer to config. Activate it via REST — the gamepad should disappear from the host OS's device list. Deactivate — it should reconnect.

- [ ] **Step 7: Commit**

```bash
git add shared/rpcinterface.h \
        mainboard/src/emulation/EmulationBoard.h \
        mainboard/src/mapping/MappingManager.h \
        mainboard/src/mapping/MappingManager.cpp \
        Pico/src/...
git commit -m "feat: vod_state disconnected — USB detach/reattach via new M2P_SET_USB_CONNECTED RPC"
```

---

## Task 8: Turbo coroutine

**Files:**
- Modify: `mainboard/src/mapping/MappingManager.h`
- Modify: `mainboard/src/mapping/MappingManager.cpp`

- [ ] **Step 1: Add turbo state tracking to MappingManager**

In `MappingManager.h`, add:

```cpp
#include <map>
// Turbo state: key = {vidId, axisIndex}, value = pointer to a shared bool "running" flag
// The coroutine holds a shared_ptr to the bool; setting it false stops the loop.
#include <memory>

struct TurboKey {
    std::string vidId;
    int         axisIndex;
    bool operator<(const TurboKey& o) const {
        return vidId < o.vidId || (vidId == o.vidId && axisIndex < o.axisIndex);
    }
};

// private:
    std::map<TurboKey, std::shared_ptr<bool>> activeTurbos;
    void startTurbo(const TurboRule& rule);
    void stopTurbo(const std::string& vidId, int axisIndex);
    void stopAllTurbosForLayer(Layer* layer);
```

- [ ] **Step 2: Implement startTurbo and stopTurbo**

In `MappingManager.cpp`:

```cpp
void MappingManager::startTurbo(const TurboRule& rule) {
    if (rule.axisIndex == -1) return;
    TurboKey key { rule.vidId, rule.axisIndex };
    if (activeTurbos.count(key)) return;  // already running

    auto running = std::make_shared<bool>(true);
    activeTurbos[key] = running;

    // Capture everything needed by value so the lambda is self-contained
    std::string vidId    = rule.vidId;
    int         axisIdx  = rule.axisIndex;
    int         onMs     = rule.onMs;
    int         offMs    = rule.offMs;
    int         initDelay = rule.initialDelay;
    int         maxVal   = rule.maxValue;
    int         minVal   = rule.minValue;

    corocgo::coro([this, running, vidId, axisIdx, onMs, offMs, initDelay, maxVal, minVal]() {
        if (initDelay > 0) {
            corocgo::sleep(initDelay);
            if (!*running) {
                vidState[vidId][axisIdx] = minVal;
                dispatchVidAxisEvent(vidId, axisIdx, minVal);
                return;
            }
        }
        while (*running) {
            vidState[vidId][axisIdx] = maxVal;
            dispatchVidAxisEvent(vidId, axisIdx, maxVal);
            corocgo::sleep(onMs);
            if (!*running) break;
            vidState[vidId][axisIdx] = minVal;
            dispatchVidAxisEvent(vidId, axisIdx, minVal);
            corocgo::sleep(offMs);
        }
        // Cleanup: ensure axis is released
        vidState[vidId][axisIdx] = minVal;
        dispatchVidAxisEvent(vidId, axisIdx, minVal);
    });
}

void MappingManager::stopTurbo(const std::string& vidId, int axisIndex) {
    TurboKey key { vidId, axisIndex };
    auto it = activeTurbos.find(key);
    if (it == activeTurbos.end()) return;
    *it->second = false;   // signals coroutine to stop after its current sleep
    activeTurbos.erase(it);
}

void MappingManager::stopAllTurbosForLayer(Layer* layer) {
    for (const auto& tr : layer->turboRules) {
        if (tr.axisIndex != -1)
            stopTurbo(tr.vidId, tr.axisIndex);
    }
}
```

- [ ] **Step 3: Evaluate turbo rules in dispatchVidAxisEvent**

In `dispatchVidAxisEvent`, after the block rule check (Task 5) and before the AxisRule processing, add turbo rule evaluation. Turbo rules apply per-layer but the coroutine is global (key-based):

```cpp
// --- Turbo rule check ---
bool turboConsumed = false;
for (auto& tr : layer->turboRules) {
    if (tr.vidId != vidId || tr.axisIndex != vidAxisIndex) continue;
    if (tr.condition == TurboCondition::WhileAxisActive) {
        if (value > 0) {
            startTurbo(tr);
            turboConsumed = true;  // turbo takes over this axis; suppress AxisRule dispatch
        } else {
            stopTurbo(vidId, vidAxisIndex);
            // Don't consume on release — let the 0 value propagate to release mapped outputs
        }
    }
    // TurboCondition::Always turbos are started/stopped with layer activation (not here)
    break;  // at most one turbo rule per axis per layer
}
if (turboConsumed) break;  // exit layer stack loop — consumed
```

Wait — turbo with `condition: always` should start when the layer activates, not on axis events. Handle that in Task 9 (activation triggers) and the `onActivate` callback.

- [ ] **Step 4: Start/stop `condition: always` turbos on layer activate/deactivate**

In `MappingManager::load()`, extend the callbacks:

```cpp
layerManager.onActivate = [this](Layer* layer) {
    evaluateVodStates();
    // Start always-turbos for this layer
    for (auto& tr : layer->turboRules) {
        if (tr.condition == TurboCondition::Always)
            startTurbo(tr);
    }
};

layerManager.onDeactivate = [this](Layer* layer) {
    // Block cleanup
    for (auto& br : layer->blockRules) {
        for (auto& e : br.entries) {
            if (e.axisIndex == -1 || e.value == 0) continue;
            vidState[br.vidId][e.axisIndex] = 0;
            dispatchVidAxisEvent(br.vidId, e.axisIndex, 0);
        }
    }
    // Turbo cleanup
    stopAllTurbosForLayer(layer);
    evaluateVodStates();
};
```

Also, for `condition: always` layers that are already active at boot, call `startTurbo` at the end of `load()` for all active layers:

```cpp
// At end of load(), after evaluateVodStates():
for (Layer* layer : layerManager.activeStack) {
    for (auto& tr : layer->turboRules) {
        if (tr.condition == TurboCondition::Always)
            startTurbo(tr);
    }
}
```

- [ ] **Step 5: Build**

```bash
cd mainboard && cmake --build build 2>&1 | head -40
```

Expected: successful build.

- [ ] **Step 6: Manual test — turbo rule**

Add to config:
```json
{
    "id": "turbo-test",
    "name": "Turbo Test",
    "active": true,
    "rules": [
        {
            "type": "turbo",
            "vid": "vid1",
            "axis": "ButtonA",
            "on_ms": 100,
            "off_ms": 100,
            "condition": "while_axis_active"
        }
    ]
}
```

Hold ButtonA — the mapped output button should rapidly toggle. Release — it should stop.

- [ ] **Step 7: Remove test layer from config, commit**

```bash
git add mainboard/src/mapping/MappingManager.h \
        mainboard/src/mapping/MappingManager.cpp
git commit -m "feat(mapping): turbo coroutine — while_axis_active and always conditions"
```

---

## Task 9: Layer activation triggers

**Files:**
- Modify: `mainboard/src/mapping/MappingManager.h`
- Modify: `mainboard/src/mapping/MappingManager.cpp`

Activation triggers use the same hotkey parser already in MappingManager. Each layer with an `activation` block gets a single `AxisRule` built from its hotkey expression, stored separately from `layer.rules`.

- [ ] **Step 1: Add activation rule storage to Layer**

In `Layer.h`, add:

```cpp
// Activation trigger (built from layer.activation.hotkey):
std::unique_ptr<AxisRule> activationRule;  // nullptr if no activation config
```

- [ ] **Step 2: Build activation rules in MappingManager::load()**

In `load()`, after the layer's rules are loaded and before pushing to `layerManager.allLayers`, add:

```cpp
if (lc.activation.has_value()) {
    const ConfActivation& act = lc.activation.value();
    if (!act.hotkey.empty() && !act.vid.empty()) {
        auto rule = std::make_unique<AxisRule>();
        rule->propagate  = false;
        rule->exclusive  = true;
        rule->hotkeyParts = parseHotkeyString(act.hotkey, act.vid);
        layer.activationRule = std::move(rule);
    }
}
```

- [ ] **Step 3: Resolve activation rule axes in resolveVidAxes()**

In `resolveVidAxes()`, after the existing resolution loops, add:

```cpp
for (auto& layer : layerManager.allLayers) {
    if (!layer.activationRule) continue;
    for (auto& part : layer.activationRule->hotkeyParts) {
        auto resolveRef = [&](VidAxisRef& ref) {
            if (ref.axisIndex != -1 || ref.axisName.empty()) return;
            auto vidIt = vids.find(ref.vidId);
            if (vidIt == vids.end()) return;
            ref.axisIndex = vidIt->second.axisTable.getIndex(ref.axisName);
        };
        for (auto& mod : part.modifiers) resolveRef(mod);
        if (part.activationAxis) resolveRef(part.activationAxis.value());
    }
    layer.activationRule->reset();
    layer.rebuildActivationIndex();  // Rebuild to include activationRule? No — activation index is separate.
    // Actually: activation rules need their own evaluation, not via activationIndex.
    // We'll evaluate them directly in dispatchVidAxisEvent.
}
```

- [ ] **Step 4: Evaluate activation triggers in dispatchVidAxisEvent**

At the **end** of `dispatchVidAxisEvent`, after all layer processing, evaluate activation triggers for all layers:

```cpp
// --- Activation trigger evaluation ---
for (auto& layer : layerManager.allLayers) {
    if (!layer.activationRule) continue;
    const ConfActivation& act = layer.activation.value();

    AxisRule* rule = layer.activationRule.get();

    if (act.mode == ConfActivationMode::WhileActive) {
        // Activate while axis/hotkey is held; deactivate on release.
        auto result = (value > 0)
            ? rule->tryActivateFirstStep(vidId, vidAxisIndex, vidState)
            : AxisRule::EventResult::Ignored;

        if (value == 0 && rule->isWaitingForRelease()) {
            // Check if this is the release of the activation axis
            if (rule->isReleaseEvent(vidId, vidAxisIndex, value)) {
                rule->reset();
                layerManager.deactivate(layer.id);
            }
        } else if (result == AxisRule::EventResult::Completed) {
            rule->state = AxisRule::State::WaitingForRelease;
            layerManager.activate(layer.id);
        } else if (result == AxisRule::EventResult::Advanced) {
            rule->state = AxisRule::State::InProgress;
        }

    } else if (act.mode == ConfActivationMode::WhileNotActive) {
        // Inverse: deactivate while hotkey is held; activate on release.
        auto result = (value > 0)
            ? rule->tryActivateFirstStep(vidId, vidAxisIndex, vidState)
            : AxisRule::EventResult::Ignored;

        if (value == 0 && rule->isWaitingForRelease()) {
            if (rule->isReleaseEvent(vidId, vidAxisIndex, value)) {
                rule->reset();
                layerManager.activate(layer.id);
            }
        } else if (result == AxisRule::EventResult::Completed) {
            rule->state = AxisRule::State::WaitingForRelease;
            layerManager.deactivate(layer.id);
        }

    } else if (act.mode == ConfActivationMode::Toggle) {
        if (value > 0) {
            auto result = rule->tryActivateFirstStep(vidId, vidAxisIndex, vidState);
            if (result == AxisRule::EventResult::Completed) {
                rule->reset();
                layer.toggleState = !layer.toggleState;
                if (layer.toggleState) layerManager.activate(layer.id);
                else                   layerManager.deactivate(layer.id);
            } else if (result == AxisRule::EventResult::Advanced) {
                // multi-step hotkey in progress
            }
        }
    }
}
```

Note: `LayerManager::activate()` and `deactivate()` are already safe to call re-entrantly (they check for duplicates and missing IDs).

Note: `AxisRule::isReleaseEvent()` is already implemented in `AxisRule.h`.

- [ ] **Step 5: Initialize WhileNotActive layers correctly at startup**

At the end of `load()`, for `WhileNotActive` layers that should start active (their hotkey is not currently held):

```cpp
// WhileNotActive layers start active unless their hotkey is currently held
// (at startup, no axes are held, so all WhileNotActive layers start active)
for (auto& layer : layerManager.allLayers) {
    if (!layer.activation.has_value()) continue;
    if (layer.activation->mode == ConfActivationMode::WhileNotActive) {
        // Start active — add to activeStack if not already there (config `active: true` handles this)
        // If config says `active: false` for a WhileNotActive layer, respect that.
    }
}
// No action needed — config `active` field sets initial state correctly.
// WhileNotActive semantics take over once the first axis event arrives.
```

- [ ] **Step 6: Build**

```bash
cd mainboard && cmake --build build 2>&1 | head -40
```

Expected: successful build.

- [ ] **Step 7: Manual test — toggle activation**

Add to config:
```json
{
    "id": "toggle-test",
    "name": "Toggle Test",
    "active": false,
    "activation": {
        "mode": "toggle",
        "vid": "vid1",
        "hotkey": "!ButtonBumperLeft+ButtonBack"
    },
    "rules": [
        { "type": "block", "vid": "vid1", "axes": [{ "axis": "ButtonA", "value": 0 }] }
    ]
}
```

Press BumperLeft + Back — ButtonA should stop working (layer activated, block rule in effect).
Press BumperLeft + Back again — ButtonA should work again (layer deactivated).

Verify with `GET /layers` that the layer's active state toggles correctly.

- [ ] **Step 8: Test while_active mode**

```json
{
    "id": "hold-test",
    "name": "Hold Test",
    "active": false,
    "activation": {
        "mode": "while_active",
        "vid": "vid1",
        "hotkey": "ButtonBumperRight"
    },
    "rules": [
        { "type": "block", "vid": "vid1", "axes": [{ "axis": "ButtonA", "value": 0 }] }
    ]
}
```

Hold BumperRight — ButtonA stops working. Release BumperRight — ButtonA resumes.

- [ ] **Step 9: Remove test layers from config, commit**

```bash
git add mainboard/src/mapping/MappingManager.h \
        mainboard/src/mapping/MappingManager.cpp \
        mainboard/src/mapping/Layer.h
git commit -m "feat(mapping): layer activation triggers — toggle, while_active, while_not_active"
```

---

## Self-Review Checklist (for implementer)

After all tasks are complete, verify:

- [ ] `block` rule: pressing a blocked axis emits no output; deactivating a layer with non-zero block value sends 0
- [ ] `vod_state: silenced`: all setAxis calls for that VOD are dropped; restores on layer deactivate
- [ ] `vod_state: disconnected`: Pico USB disconnects on layer activate; reconnects on deactivate
- [ ] `turbo` with `while_axis_active`: holding the axis starts rapid toggling; releasing stops it and emits min_value
- [ ] `turbo` with `always`: starts on layer activate, stops on layer deactivate, emits min_value on stop
- [ ] `activation: toggle`: first fire activates layer, second fire deactivates
- [ ] `activation: while_active`: layer active only while hotkey held
- [ ] `activation: while_not_active`: layer active while hotkey not held, deactivates while held
- [ ] Config reload (`POST /config/reload`) works with all new rule types — no crash on reload
- [ ] Multiple active layers with conflicting `block` rules: highest-priority (first in array) wins
- [ ] Multiple active layers with conflicting `vod_state` rules: highest-priority wins
