// mainboard/src/Layer.h
#pragma once
#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <unordered_map>
#include "AxisRule.h"
#include "HotkeyPart.h"
#include "BlockRule.h"
#include "VodStateRule.h"
#include "TurboRule.h"
#include "../MainConfig.h"

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

    // New rule types
    std::vector<BlockRule>    blockRules;
    std::vector<VodStateRule> vodStateRules;
    std::vector<TurboRule>    turboRules;

    // Activation trigger (from config)
    std::optional<ConfActivation>    activation;
    std::unique_ptr<AxisRule>        activationRule;  // built from activation.hotkey
    bool                             toggleState = false;

    // Rebuild activationIndex from rules (called after axis index resolution).
    void rebuildActivationIndex();

    // Reset all in-progress hotkey state (called on deactivation).
    void resetActiveRules();
};
