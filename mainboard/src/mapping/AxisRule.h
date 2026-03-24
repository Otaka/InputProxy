// mainboard/src/AxisRule.h
#pragma once
#include <vector>
#include <memory>
#include "HotkeyPart.h"
#include "Action.h"
#include "VidStateMap.h"

class AxisRule {
public:
    std::vector<HotkeyPart>               hotkeyParts;
    std::vector<std::unique_ptr<Action>>  pressActions;
    std::vector<std::unique_ptr<Action>>  releaseActions;
    bool                                  propagate  = true;
    bool                                  exclusive  = true;  // false = skip exact-match check

    // Returns axes that must be indexed to trigger initial evaluation of this rule:
    // - Part 0 has activation axis: returns that axis.
    // - Part 0 is modifier-only: returns all modifiers in part 0.
    std::vector<VidAxisRef> getActivationAxes() const;

    // Runtime hotkey state -----------------------------------------------
    enum class State { Idle, InProgress, WaitingForRelease };
    State state       = State::Idle;
    int   currentStep = 0;

    bool isActive()           const { return state == State::InProgress; }
    bool isWaitingForRelease() const { return state == State::WaitingForRelease; }

    enum class EventResult { Ignored, Cancelled, Advanced, Completed };

    // Called while rule is InProgress (in layer.activeRules).
    // Only called for press events (value > 0).
    EventResult processAxisEvent(const std::string& vidId, int axisIndex,
                                 const VidStateMap& vidState);

    // Called from activation index when rule is Idle.
    // Only called for press events (value > 0).
    EventResult tryActivateFirstStep(const std::string& vidId, int axisIndex,
                                     const VidStateMap& vidState);

    // Returns true if this is a release event for the last part's activation axis.
    // Only meaningful when state == WaitingForRelease.
    bool isReleaseEvent(const std::string& vidId, int axisIndex, int value) const;

    void reset() { state = State::Idle; currentStep = 0; }

private:
    bool checkExactMatch(const HotkeyPart& part, const VidStateMap& vidState) const;
    EventResult advance();
};
