// mainboard/src/LayerManager.h
#pragma once
#include <string>
#include <vector>
#include <functional>
#include "Layer.h"

class LayerManager {
public:
    std::vector<Layer>   allLayers;    // all defined layers, config order
    std::vector<Layer*>  activeStack;  // active layers, top = front

    // Optional callbacks called on activate/deactivate
    std::function<void(Layer*)> onActivate;
    std::function<void(Layer*)> onDeactivate;

    // Push to front of activeStack. No-op if already active.
    void activate(const std::string& id);

    // Remove by id (any position). Resets in-progress state. No-op if not active.
    void deactivate(const std::string& id);

    // Find a layer by id. Returns nullptr if not found.
    Layer* findLayer(const std::string& id);

    const std::vector<Layer*>& stack() const { return activeStack; }
};
