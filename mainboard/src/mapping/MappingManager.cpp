// mainboard/src/MappingManager.cpp
#include "MappingManager.h"
#include "EmulatedDeviceManager.h"
#include "EmulationBoard.h"
#include "RealDeviceManager.h"
#include "OutputSequenceParser.h"
#include "corocgo/corocgo.h"
#include <iostream>
#include <algorithm>

using namespace corocgo;

// ---------------------------------------------------------------------------
// Config parsing helpers
// ---------------------------------------------------------------------------

static std::vector<HotkeyPart> parseHotkeyString(const std::string& hotkey,
                                                   const std::string& defaultVidId) {
    std::vector<HotkeyPart> parts;
    std::vector<std::string> partStrs;
    std::string remaining = hotkey;
    while (true) {
        auto pos = remaining.find("->");
        if (pos == std::string::npos) {
            partStrs.push_back(remaining);
            break;
        }
        partStrs.push_back(remaining.substr(0, pos));
        remaining = remaining.substr(pos + 2);
    }

    for (const auto& partStr : partStrs) {
        HotkeyPart part;
        std::vector<std::string> axisTokens;
        std::string rem = partStr;
        while (true) {
            auto pos = rem.find('+');
            if (pos == std::string::npos) {
                axisTokens.push_back(rem);
                break;
            }
            axisTokens.push_back(rem.substr(0, pos));
            rem = rem.substr(pos + 1);
        }

        for (auto t : axisTokens) {
            while (!t.empty() && std::isspace((unsigned char)t.front())) t.erase(t.begin());
            while (!t.empty() && std::isspace((unsigned char)t.back()))  t.pop_back();
            if (t.empty()) continue;

            bool isModifier = (t[0] == '!');
            if (isModifier) t = t.substr(1);

            std::string vidId    = defaultVidId;
            std::string axisName = t;
            auto colonPos = t.find(':');
            if (colonPos != std::string::npos) {
                vidId    = t.substr(0, colonPos);
                axisName = t.substr(colonPos + 1);
            }

            VidAxisRef ref;
            ref.vidId     = vidId;
            ref.axisName  = axisName;
            ref.axisIndex = -1;

            if (isModifier) {
                part.modifiers.push_back(ref);
            } else {
                if (part.activationAxis.has_value()) {
                    std::cerr << "[mapping] hotkey part has more than one activation axis, ignoring extra\n";
                } else {
                    part.activationAxis = ref;
                }
            }
        }

        auto addVid = [&](const std::string& vid) {
            if (std::find(part.involvedVids.begin(), part.involvedVids.end(), vid)
                == part.involvedVids.end())
                part.involvedVids.push_back(vid);
        };
        for (const auto& m : part.modifiers) addVid(m.vidId);
        if (part.activationAxis) addVid(part.activationAxis->vidId);

        parts.push_back(std::move(part));
    }
    return parts;
}

static std::vector<std::unique_ptr<Action>> buildActions(const std::vector<ConfAction>& actions) {
    std::vector<std::unique_ptr<Action>> result;
    for (const auto& a : actions) {
        if (a.type == ConfActionType::EmitAxis) {
            auto act       = std::make_unique<EmitAxisAction>();
            act->vodId     = a.vod;
            act->axisName  = a.axis;
            act->axisIndex = -1;
            result.push_back(std::move(act));
        } else if (a.type == ConfActionType::OutputSequence) {
            auto act       = std::make_unique<OutputSequenceAction>();
            act->vodId     = a.vod;
            auto parsed    = parseOutputSequence(a.sequence);
            act->steps     = std::move(parsed.steps);
            act->axisNames = std::move(parsed.axisNames);
            result.push_back(std::move(act));
        } else if (a.type == ConfActionType::Sleep) {
            auto act    = std::make_unique<SleepAction>();
            act->timeMs = a.timeMs;
            result.push_back(std::move(act));
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// loadFromConfig
// ---------------------------------------------------------------------------

void MappingManager::clear() {
    // Stop all turbos before clearing
    for (auto& [key, running] : activeTurbos) *running = false;
    activeTurbos.clear();
    usbDisconnectedBoards.clear();
    vids.clear();
    realDeviceMappings.clear();
    deviceAssignments.clear();
    vidState.clear();
    layerManager.allLayers.clear();
    layerManager.activeStack.clear();
}

void MappingManager::load(const ConfRoot& config, EmulatedDeviceManager* edm_) {
    edm = edm_;

    for (const auto& v : config.vids) {
        if (v.id.empty()) { std::cerr << "[mapping] VID missing id\n"; continue; }
        VirtualInputDevice vid;
        vid.id   = v.id;
        vid.name = v.name;
        vids.emplace(v.id, std::move(vid));
    }

    for (const auto& rd : config.realDevices) {
        if (rd.id.empty() || rd.assignedTo.empty()) continue;
        if (vids.find(rd.assignedTo) == vids.end()) {
            std::cerr << "[mapping] device '" << rd.id
                      << "' assigned to unknown VID '" << rd.assignedTo << "'\n";
            continue;
        }
        deviceAssignments[rd.id] = rd.assignedTo;
    }

    for (const auto& lc : config.layers) {
        Layer layer;
        layer.id   = lc.id;
        layer.name = lc.name;
        if (layer.id.empty()) { std::cerr << "[mapping] layer missing id\n"; continue; }

        for (const auto& rc : lc.rules) {
            std::string vidId = rc.vid;

            if (rc.type == ConfRuleType::Simple) {
                std::string vodId = rc.vod;
                for (const auto& ax : rc.axes) {
                    if (ax.from.empty() || ax.to.empty()) continue;

                    AxisRule rule;
                    rule.propagate = true;
                    rule.exclusive = false;

                    HotkeyPart part;
                    VidAxisRef ref;
                    ref.vidId     = vidId;
                    ref.axisName  = ax.from;
                    ref.axisIndex = -1;
                    part.activationAxis = ref;
                    part.involvedVids   = { vidId };
                    rule.hotkeyParts.push_back(std::move(part));

                    auto press = std::make_unique<EmitAxisAction>();
                    press->vodId    = vodId;
                    press->axisName = ax.to;
                    rule.pressActions.push_back(std::move(press));

                    auto release = std::make_unique<EmitAxisAction>();
                    release->vodId    = vodId;
                    release->axisName = ax.to;
                    rule.releaseActions.push_back(std::move(release));

                    layer.rules.push_back(std::move(rule));
                }
            } else if (rc.type == ConfRuleType::Hotkey) {
                AxisRule rule;
                rule.propagate   = rc.propagate;
                rule.hotkeyParts = parseHotkeyString(rc.hotkey, vidId);
                rule.pressActions   = buildActions(rc.pressActions);
                rule.releaseActions = buildActions(rc.releaseActions);
                layer.rules.push_back(std::move(rule));

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

            } else {
                std::cerr << "[mapping] unknown rule type\n";
            }
        }

        // Copy activation config and build activation rule
        layer.activation = lc.activation;
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

        bool activeAtBoot = lc.active;
        layerManager.allLayers.push_back(std::move(layer));
        if (activeAtBoot)
            layerManager.activeStack.push_back(&layerManager.allLayers.back());
    }

    // Wire layer callbacks
    layerManager.onActivate = [this](Layer* layer) {
        evaluateVodStates();
        for (auto& tr : layer->turboRules) {
            if (tr.condition == TurboCondition::Always)
                startTurbo(tr);
        }
    };

    layerManager.onDeactivate = [this](Layer* layer) {
        for (auto& br : layer->blockRules) {
            for (auto& e : br.entries) {
                if (e.axisIndex == -1 || e.value == 0) continue;
                vidState[br.vidId][e.axisIndex] = 0;
                dispatchVidAxisEvent(br.vidId, e.axisIndex, 0);
            }
        }
        stopAllTurbosForLayer(layer);
        evaluateVodStates();
    };

    // Start always-turbos for layers active at boot
    for (Layer* layer : layerManager.activeStack) {
        for (auto& tr : layer->turboRules) {
            if (tr.condition == TurboCondition::Always)
                startTurbo(tr);
        }
    }

    evaluateVodStates();

    std::cout << "[mapping] loaded " << layerManager.allLayers.size() << " layer(s)\n";
}

// ---------------------------------------------------------------------------
// Resolution
// ---------------------------------------------------------------------------

void MappingManager::resolveVidAxes() {
    for (auto& layer : layerManager.allLayers) {
        for (auto& rule : layer.rules) {
            for (auto& part : rule.hotkeyParts) {
                auto resolveRef = [&](VidAxisRef& ref) {
                    if (ref.axisIndex != -1 || ref.axisName.empty()) return;
                    auto vidIt = vids.find(ref.vidId);
                    if (vidIt == vids.end()) return;
                    ref.axisIndex = vidIt->second.axisTable.getIndex(ref.axisName);
                };
                for (auto& mod : part.modifiers) resolveRef(mod);
                if (part.activationAxis) resolveRef(part.activationAxis.value());
            }
        }
        layer.rebuildActivationIndex();

        // Resolve BlockRule axis indices
        for (auto& br : layer.blockRules) {
            auto vidIt = vids.find(br.vidId);
            if (vidIt == vids.end()) continue;
            for (auto& e : br.entries) {
                if (e.axisIndex == -1 && !e.axisName.empty())
                    e.axisIndex = vidIt->second.axisTable.getIndex(e.axisName);
            }
        }

        // Resolve TurboRule axis indices
        for (auto& tr : layer.turboRules) {
            auto vidIt = vids.find(tr.vidId);
            if (vidIt == vids.end()) continue;
            if (tr.axisIndex == -1 && !tr.axisName.empty())
                tr.axisIndex = vidIt->second.axisTable.getIndex(tr.axisName);
        }

        // Resolve activation rule axis indices
        if (layer.activationRule) {
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
        }
    }
}

void MappingManager::resolveVodAxes() {
    for (auto& layer : layerManager.allLayers) {
        for (auto& rule : layer.rules) {
            auto resolveAction = [&](Action* act) {
                if (auto* ea = dynamic_cast<EmitAxisAction*>(act)) {
                    if (ea->axisIndex != -1 || ea->axisName.empty()) return;
                    int devIdx = edm->resolveId(ea->vodId);
                    if (devIdx == -1) return;
                    ea->axisIndex = edm->getDevices()[devIdx].axisTable.getIndex(ea->axisName);
                } else if (auto* osa = dynamic_cast<OutputSequenceAction*>(act)) {
                    int devIdx = edm->resolveId(osa->vodId);
                    if (devIdx == -1) return;
                    for (size_t i = 0; i < osa->steps.size(); ++i) {
                        if (osa->steps[i].type == SequenceStep::Type::SetAxis &&
                            osa->steps[i].axisIndex == -1 &&
                            i < osa->axisNames.size() && !osa->axisNames[i].empty()) {
                            osa->steps[i].axisIndex =
                                edm->getDevices()[devIdx].axisTable.getIndex(osa->axisNames[i]);
                        }
                    }
                }
            };
            for (auto& a : rule.pressActions)  resolveAction(a.get());
            for (auto& a : rule.releaseActions) resolveAction(a.get());
        }
    }
}

void MappingManager::onBoardRegistered() {
    resolveVodAxes();
}

void MappingManager::onRealDeviceConnected(const std::string& deviceIdStr,
                                            const RealDevice& device) {
    auto assignIt = deviceAssignments.find(deviceIdStr);
    if (assignIt == deviceAssignments.end()) return;

    const std::string& vidId = assignIt->second;
    VirtualInputDevice& vid  = vids.at(vidId);

    for (const auto& entry : device.axes.getEntries()) {
        if (!vid.axisTable.hasName(entry.name))
            vid.axisTable.addEntry(entry.name, entry.index);
    }

    RealDeviceToVidMapping mapping;
    mapping.vid    = &vid;
    mapping.active = true;
    for (const auto& entry : device.axes.getEntries()) {
        int vidAxisIndex = vid.axisTable.getIndex(entry.name);
        if (vidAxisIndex != -1)
            mapping.realToVidAxisIndex[entry.index] = vidAxisIndex;
    }
    realDeviceMappings[deviceIdStr] = std::move(mapping);

    resolveVidAxes();
    std::cout << "[mapping] device '" << deviceIdStr << "' → VID '" << vidId << "'\n";
}

void MappingManager::onRealDeviceDisconnected(const std::string& deviceIdStr) {
    auto it = realDeviceMappings.find(deviceIdStr);
    if (it != realDeviceMappings.end()) {
        it->second.active = false;
        std::cout << "[mapping] device '" << deviceIdStr << "' disconnected\n";
    }
}

// ---------------------------------------------------------------------------
// Dispatch
// ---------------------------------------------------------------------------

void MappingManager::executeActions(std::vector<std::unique_ptr<Action>>& actions, int value) {
    for (auto& a : actions) {
        Action* act = a.get();
        if (auto* ea = dynamic_cast<EmitAxisAction*>(act)) {
            int devIdx = edm->resolveId(ea->vodId);
            if (devIdx != -1 && ea->axisIndex != -1){
                //td::cout<<"Action Emit axis index="<<ea->axisIndex<<" value="<<value<<std::endl;
                edm->setAxis(devIdx, ea->axisIndex, value);
            }
        } else if (auto* osa = dynamic_cast<OutputSequenceAction*>(act)) {
            int devIdx = edm->resolveId(osa->vodId);
            for (const auto& step : osa->steps) {
                if (step.type == SequenceStep::Type::SetAxis) {
                    if (devIdx != -1 && step.axisIndex != -1){
                      // std::cout<<"Action sequence index="<<step.axisIndex<<" value="<<step.value<<std::endl;
                        edm->setAxis(devIdx, step.axisIndex, step.value);
                    }
                } else {
                    sleep(step.timeMs);
                }
            }
        } else if (auto* sa = dynamic_cast<SleepAction*>(act)) {
            sleep(sa->timeMs);
        }
    }
}

void MappingManager::dispatchVidAxisEvent(const std::string& vidId,
                                           int vidAxisIndex, int value) {
    for (Layer* layer : layerManager.stack()) {
        bool consumed = false;

        // --- Block rule check ---
        {
            bool blocked   = false;
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

        if (value == 0) {
            // Stop turbo on release (for WhileAxisActive turbos)
            for (auto& tr : layer->turboRules) {
                if (tr.vidId == vidId && tr.axisIndex == vidAxisIndex &&
                    tr.condition == TurboCondition::WhileAxisActive) {
                    stopTurbo(vidId, vidAxisIndex);
                    break;
                }
            }

            VidAxisKey key { vidId, vidAxisIndex };
            auto it = layer->pendingReleaseRules.find(key);
            if (it != layer->pendingReleaseRules.end()) {
                for (auto* rule : it->second) {
                    executeActions(rule->releaseActions, 0);
                    rule->reset();
                }
                layer->pendingReleaseRules.erase(it);
            }
            continue;
        }

        // --- Turbo rule check (press only) ---
        {
            bool turboConsumed = false;
            for (auto& tr : layer->turboRules) {
                if (tr.vidId != vidId || tr.axisIndex != vidAxisIndex) continue;
                if (tr.condition == TurboCondition::WhileAxisActive) {
                    startTurbo(tr);
                    turboConsumed = true;
                }
                break;  // at most one turbo rule per axis per layer
            }
            if (turboConsumed) break;
        }

        // Press: process active rules first
        {
            std::vector<AxisRule*> toRemove;
            for (auto* rule : layer->activeRules) {
                auto result = rule->processAxisEvent(vidId, vidAxisIndex, vidState);
                if (result == AxisRule::EventResult::Cancelled) {
                    toRemove.push_back(rule);
                } else if (result == AxisRule::EventResult::Completed) {
                    toRemove.push_back(rule);
                    executeActions(rule->pressActions, value);
                    if (!rule->releaseActions.empty()) {
                        const auto& lastPart = rule->hotkeyParts.back();
                        if (lastPart.activationAxis.has_value()) {
                            VidAxisKey key { lastPart.activationAxis->vidId,
                                             lastPart.activationAxis->axisIndex };
                            auto& pl = layer->pendingReleaseRules[key];
                            if (std::find(pl.begin(), pl.end(), rule) == pl.end())
                                pl.push_back(rule);
                            rule->state = AxisRule::State::WaitingForRelease;
                        }
                    }
                    if (!rule->propagate) consumed = true;
                }
            }
            for (auto* r : toRemove) {
                layer->activeRules.erase(
                    std::remove(layer->activeRules.begin(), layer->activeRules.end(), r),
                    layer->activeRules.end());
            }
        }

        if (!consumed) {
            VidAxisKey key { vidId, vidAxisIndex };
            auto it = layer->activationIndex.find(key);
            if (it != layer->activationIndex.end()) {
                for (auto* rule : it->second) {
                    if (std::find(layer->activeRules.begin(), layer->activeRules.end(), rule)
                        != layer->activeRules.end()) continue;

                    auto result = rule->tryActivateFirstStep(vidId, vidAxisIndex, vidState);
                    if (result == AxisRule::EventResult::Advanced) {
                        layer->activeRules.push_back(rule);
                        if (!rule->propagate) consumed = true;
                    } else if (result == AxisRule::EventResult::Completed) {
                        executeActions(rule->pressActions, value);
                        if (!rule->releaseActions.empty()) {
                            const auto& lastPart = rule->hotkeyParts.back();
                            if (lastPart.activationAxis.has_value()) {
                                VidAxisKey rKey { lastPart.activationAxis->vidId,
                                                  lastPart.activationAxis->axisIndex };
                                auto& pl = layer->pendingReleaseRules[rKey];
                                if (std::find(pl.begin(), pl.end(), rule) == pl.end())
                                    pl.push_back(rule);
                                rule->state = AxisRule::State::WaitingForRelease;
                            }
                        }
                        if (!rule->propagate) consumed = true;
                    }
                }
            }
        }

        if (consumed) break;
    }

    // --- Activation trigger evaluation (all layers, regardless of active state) ---
    for (auto& layer : layerManager.allLayers) {
        if (!layer.activationRule || !layer.activation.has_value()) continue;
        const ConfActivation& act = layer.activation.value();
        AxisRule* rule = layer.activationRule.get();

        if (act.mode == ConfActivationMode::WhileActive) {
            if (value > 0) {
                auto result = rule->tryActivateFirstStep(vidId, vidAxisIndex, vidState);
                if (result == AxisRule::EventResult::Completed) {
                    rule->state = AxisRule::State::WaitingForRelease;
                    layerManager.activate(layer.id);
                } else if (result == AxisRule::EventResult::Advanced) {
                    rule->state = AxisRule::State::InProgress;
                }
            } else {
                if (rule->state == AxisRule::State::WaitingForRelease) {
                    // Check if the released axis is the last activation axis
                    const auto& lastPart = rule->hotkeyParts.back();
                    if (lastPart.activationAxis.has_value() &&
                        lastPart.activationAxis->vidId == vidId &&
                        lastPart.activationAxis->axisIndex == vidAxisIndex) {
                        rule->reset();
                        layerManager.deactivate(layer.id);
                    }
                }
            }
        } else if (act.mode == ConfActivationMode::WhileNotActive) {
            if (value > 0) {
                auto result = rule->tryActivateFirstStep(vidId, vidAxisIndex, vidState);
                if (result == AxisRule::EventResult::Completed) {
                    rule->state = AxisRule::State::WaitingForRelease;
                    layerManager.deactivate(layer.id);
                } else if (result == AxisRule::EventResult::Advanced) {
                    rule->state = AxisRule::State::InProgress;
                }
            } else {
                if (rule->state == AxisRule::State::WaitingForRelease) {
                    const auto& lastPart = rule->hotkeyParts.back();
                    if (lastPart.activationAxis.has_value() &&
                        lastPart.activationAxis->vidId == vidId &&
                        lastPart.activationAxis->axisIndex == vidAxisIndex) {
                        rule->reset();
                        layerManager.activate(layer.id);
                    }
                }
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
                    rule->state = AxisRule::State::InProgress;
                }
            }
        }
    }
}

void MappingManager::evaluateVodStates() {
    const auto& devices = edm->getDevices();
    for (const auto& dev : devices) {
        const std::string& vodId = dev.id;
        VodState effectiveState  = VodState::Active;

        for (Layer* layer : layerManager.stack()) {
            bool found = false;
            for (const auto& vsr : layer->vodStateRules) {
                if (vsr.vodId == vodId) {
                    effectiveState = vsr.state;
                    found = true;
                    break;
                }
            }
            if (found) break;
        }

        edm->setSilenced(vodId, effectiveState == VodState::Silenced);

        // Handle disconnect state
        if (dev.board == nullptr) continue;
        bool shouldDisconnect = (effectiveState == VodState::Disconnected);
        bool wasDisconnected  = usbDisconnectedBoards.count(dev.board->serialString) > 0;
        if (shouldDisconnect != wasDisconnected) {
            if (shouldDisconnect) {
                usbDisconnectedBoards.insert(dev.board->serialString);
                dev.board->setUsbConnected(false);
            } else {
                usbDisconnectedBoards.erase(dev.board->serialString);
                dev.board->setUsbConnected(true);
            }
        }
    }
}

void MappingManager::startTurbo(const TurboRule& rule) {
    if (rule.axisIndex == -1) return;
    TurboKey key { rule.vidId, rule.axisIndex };
    if (activeTurbos.count(key)) return;  // already running

    auto running = std::make_shared<bool>(true);
    activeTurbos[key] = running;

    std::string vidId     = rule.vidId;
    int         axisIdx   = rule.axisIndex;
    int         onMs      = rule.onMs;
    int         offMs     = rule.offMs;
    int         initDelay = rule.initialDelay;
    int         maxVal    = rule.maxValue;
    int         minVal    = rule.minValue;

    coro([this, running, vidId, axisIdx, onMs, offMs, initDelay, maxVal, minVal]() {
        if (initDelay > 0) {
            sleep(initDelay);
            if (!*running) {
                vidState[vidId][axisIdx] = minVal;
                dispatchVidAxisEvent(vidId, axisIdx, minVal);
                return;
            }
        }
        while (*running) {
            vidState[vidId][axisIdx] = maxVal;
            dispatchVidAxisEvent(vidId, axisIdx, maxVal);
            sleep(onMs);
            if (!*running) break;
            vidState[vidId][axisIdx] = minVal;
            dispatchVidAxisEvent(vidId, axisIdx, minVal);
            sleep(offMs);
        }
        vidState[vidId][axisIdx] = minVal;
        dispatchVidAxisEvent(vidId, axisIdx, minVal);
    });
}

void MappingManager::stopTurbo(const std::string& vidId, int axisIndex) {
    TurboKey key { vidId, axisIndex };
    auto it = activeTurbos.find(key);
    if (it == activeTurbos.end()) return;
    *it->second = false;
    activeTurbos.erase(it);
}

void MappingManager::stopAllTurbosForLayer(Layer* layer) {
    for (const auto& tr : layer->turboRules) {
        if (tr.axisIndex != -1)
            stopTurbo(tr.vidId, tr.axisIndex);
    }
}

void MappingManager::axisEvent(const std::string& deviceIdStr, int axisIndex, int value) {
    auto it = realDeviceMappings.find(deviceIdStr);
    if (it == realDeviceMappings.end() || !it->second.active) return;

    RealDeviceToVidMapping& mapping = it->second;
    auto axisIt = mapping.realToVidAxisIndex.find(axisIndex);
    if (axisIt == mapping.realToVidAxisIndex.end()) return;

    const std::string& vidId   = mapping.vid->id;
    int                vidAxis = axisIt->second;

    vidState[vidId][vidAxis] = value;

    dispatchVidAxisEvent(vidId, vidAxis, value);
}
