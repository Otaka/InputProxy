// mainboard/src/LayerManager.cpp
#include "LayerManager.h"
#include <algorithm>
#include <iostream>

void Layer::rebuildActivationIndex() {
    activationIndex.clear();
    for (auto& rule : rules) {
        for (const auto& ref : rule.getActivationAxes()) {
            if (ref.axisIndex == -1) continue;
            VidAxisKey key { ref.vidId, ref.axisIndex };
            activationIndex[key].push_back(&rule);
        }
    }
}

void Layer::resetActiveRules() {
    for (auto* rule : activeRules) rule->reset();
    activeRules.clear();
    for (auto& [key, rules] : pendingReleaseRules)
        for (auto* rule : rules) rule->reset();
    pendingReleaseRules.clear();
}

Layer* LayerManager::findLayer(const std::string& id) {
    for (auto& layer : allLayers)
        if (layer.id == id) return &layer;
    return nullptr;
}

void LayerManager::activate(const std::string& id) {
    Layer* layer = findLayer(id);
    if (!layer) {
        std::cerr << "[layers] activate: unknown id '" << id << "'\n";
        return;
    }
    for (auto* l : activeStack)
        if (l->id == id) return;
    activeStack.insert(activeStack.begin(), layer);
    std::cout << "[layers] activated '" << id << "'\n";
}

void LayerManager::deactivate(const std::string& id) {
    auto it = std::find_if(activeStack.begin(), activeStack.end(),
                           [&id](Layer* l) { return l->id == id; });
    if (it == activeStack.end()) return;
    (*it)->resetActiveRules();
    activeStack.erase(it);
    std::cout << "[layers] deactivated '" << id << "'\n";
}
