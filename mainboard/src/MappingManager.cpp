// mainboard/src/MappingManager.cpp
#include "MappingManager.h"
#include "EmulatedDeviceManager.h"
#include "RealDeviceManager.h"
#include "OutputSequenceParser.h"
#include "corocgo/corocgo.h"
#include <iostream>
#include <algorithm>

using namespace corocgo;

static std::string jsonStr(const nlohmann::json& obj, const char* key) {
    auto it = obj.find(key);
    if (it == obj.end()) return "";
    if (!it->is_string()) {
        std::cerr << "[mapping] field '" << key << "' must be a string, got: "
                  << it->dump() << "\n";
        return "";
    }
    return it->get<std::string>();
}

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

static std::vector<std::unique_ptr<Action>> parseActionList(const nlohmann::json& arr) {
    using json = nlohmann::json;
    std::vector<std::unique_ptr<Action>> actions;
    if (!arr.is_array()) return actions;

    for (const auto& a : arr) {
        std::string type = a.value("type", "");
        if (type == "emit_axis") {
            auto act       = std::make_unique<EmitAxisAction>();
            act->vodId     = a.value("vod", "");
            act->axisName  = a.value("axis", "");
            act->axisIndex = -1;
            actions.push_back(std::move(act));
        } else if (type == "output_sequence") {
            auto act       = std::make_unique<OutputSequenceAction>();
            act->vodId     = a.value("vod", "");
            auto parsed    = parseOutputSequence(a.value("sequence", ""));
            act->steps     = std::move(parsed.steps);
            act->axisNames = std::move(parsed.axisNames);
            actions.push_back(std::move(act));
        } else if (type == "sleep") {
            auto act    = std::make_unique<SleepAction>();
            act->timeMs = a.value("time", 0);
            actions.push_back(std::move(act));
        } else {
            std::cerr << "[mapping] unknown action type '" << type << "', skipping\n";
        }
    }
    return actions;
}

// ---------------------------------------------------------------------------
// loadFromConfig
// ---------------------------------------------------------------------------

void MappingManager::loadFromConfig(const nlohmann::json& root, EmulatedDeviceManager* edm_) {
    using json = nlohmann::json;
    edm = edm_;

    for (const auto& v : root.value("virtual_input_devices", json::array())) {
        std::string id   = jsonStr(v, "id");
        std::string name = jsonStr(v, "name");
        if (id.empty()) { std::cerr << "[mapping] VID missing id\n"; continue; }
        VirtualInputDevice vid;
        vid.id   = id;
        vid.name = name;
        vids.emplace(id, std::move(vid));
    }

    for (const auto& rd : root.value("real_devices", json::array())) {
        std::string id         = jsonStr(rd, "id");
        std::string assignedTo = jsonStr(rd, "assignedTo");
        if (id.empty() || assignedTo.empty()) continue;
        if (vids.find(assignedTo) == vids.end()) {
            std::cerr << "[mapping] device '" << id << "' assigned to unknown VID '" << assignedTo << "'\n";
            continue;
        }
        deviceAssignments[id] = assignedTo;
    }

    for (const auto& lj : root.value("layers", json::array())) {
        Layer layer;
        layer.id   = jsonStr(lj, "id");
        layer.name = jsonStr(lj, "name");
        if (layer.id.empty()) { std::cerr << "[mapping] layer missing id\n"; continue; }

        for (const auto& rj : lj.value("rules", json::array())) {
            std::string type  = jsonStr(rj, "type");
            std::string vidId = jsonStr(rj, "vid");

            if (type == "simple") {
                std::string vodId = jsonStr(rj, "vod");
                for (const auto& axj : rj.value("axes", json::array())) {
                    std::string from = jsonStr(axj, "from");
                    std::string to   = jsonStr(axj, "to");
                    if (from.empty() || to.empty()) continue;

                    AxisRule rule;
                    rule.propagate = true;
                    rule.exclusive = false;

                    HotkeyPart part;
                    VidAxisRef ref;
                    ref.vidId     = vidId;
                    ref.axisName  = from;
                    ref.axisIndex = -1;
                    part.activationAxis = ref;
                    part.involvedVids   = { vidId };
                    rule.hotkeyParts.push_back(std::move(part));

                    auto press = std::make_unique<EmitAxisAction>();
                    press->vodId    = vodId;
                    press->axisName = to;
                    rule.pressActions.push_back(std::move(press));

                    auto release = std::make_unique<EmitAxisAction>();
                    release->vodId    = vodId;
                    release->axisName = to;
                    rule.releaseActions.push_back(std::move(release));

                    layer.rules.push_back(std::move(rule));
                }
            } else if (type == "hotkey") {
                std::string hotkeyStr = jsonStr(rj, "hotkey");
                bool propagate = rj.value("propagate", false);

                AxisRule rule;
                rule.propagate   = propagate;
                rule.hotkeyParts = parseHotkeyString(hotkeyStr, vidId);

                auto pressIt   = rj.find("press_action");
                auto releaseIt = rj.find("release_action");
                if (pressIt != rj.end())
                    rule.pressActions   = parseActionList(*pressIt);
                if (releaseIt != rj.end())
                    rule.releaseActions = parseActionList(*releaseIt);

                layer.rules.push_back(std::move(rule));
            } else {
                std::cerr << "[mapping] unknown rule type '" << type << "'\n";
            }
        }

        bool activeAtBoot = lj.value("active", true);
        layerManager.allLayers.push_back(std::move(layer));
        if (activeAtBoot)
            layerManager.activeStack.push_back(&layerManager.allLayers.back());
    }

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
                std::cout<<"Action Emit axis index="<<ea->axisIndex<<" value="<<value<<std::endl;
                edm->setAxis(devIdx, ea->axisIndex, value);
            }
        } else if (auto* osa = dynamic_cast<OutputSequenceAction*>(act)) {
            int devIdx = edm->resolveId(osa->vodId);
            for (const auto& step : osa->steps) {
                if (step.type == SequenceStep::Type::SetAxis) {
                    if (devIdx != -1 && step.axisIndex != -1){
                        std::cout<<"Action sequence index="<<step.axisIndex<<" value="<<step.value<<std::endl;
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

        if (value == 0) {
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
                            layer->pendingReleaseRules[key].push_back(rule);
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
                                layer->pendingReleaseRules[rKey].push_back(rule);
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
