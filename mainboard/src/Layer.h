// mainboard/src/Layer.h
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "AxisRule.h"
#include "HotkeyPart.h"

struct Layer {
    std::string id;
    std::string name;

    std::vector<AxisRule>   rules;

    // Rules currently in-progress (state == InProgress).
    std::vector<AxisRule*>  activeRules;

    // Rules that completed and await release.
    // Key: {vidId, axisIndex} of last hotkey part's activation axis.
    std::unordered_map<VidAxisKey, std::vector<AxisRule*>, VidAxisKeyHash> pendingReleaseRules;

    // {vidId, axisIndex} → rules whose getActivationAxes() includes it.
    std::unordered_map<VidAxisKey, std::vector<AxisRule*>, VidAxisKeyHash> activationIndex;

    // Rebuild activationIndex from rules (called after axis index resolution).
    void rebuildActivationIndex();

    // Reset all in-progress hotkey state (called on deactivation).
    void resetActiveRules();
};
