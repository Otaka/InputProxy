// mainboard/src/AxisRule.cpp
#include "AxisRule.h"
#include <algorithm>

std::vector<VidAxisRef> AxisRule::getActivationAxes() const {
    if (hotkeyParts.empty()) return {};
    const HotkeyPart& first = hotkeyParts[0];
    if (first.activationAxis.has_value()) {
        return { first.activationAxis.value() };
    }
    return first.modifiers;
}

bool AxisRule::checkExactMatch(const HotkeyPart& part, const VidStateMap& vidState) const {
    for (const auto& vidId : part.involvedVids) {
        auto vsIt = vidState.find(vidId);
        if (vsIt == vidState.end()) continue;
        for (const auto& [axisIdx, val] : vsIt->second) {
            if (val == 0) continue;
            bool allowed = false;
            if (part.activationAxis && part.activationAxis->vidId == vidId
                && part.activationAxis->axisIndex == axisIdx) {
                allowed = true;
            }
            for (const auto& mod : part.modifiers) {
                if (mod.vidId == vidId && mod.axisIndex == axisIdx) {
                    allowed = true;
                    break;
                }
            }
            if (!allowed) return false;
        }
    }
    for (const auto& mod : part.modifiers) {
        auto vsIt = vidState.find(mod.vidId);
        if (vsIt == vidState.end()) return false;
        auto axIt = vsIt->second.find(mod.axisIndex);
        if (axIt == vsIt->second.end() || axIt->second == 0) return false;
    }
    return true;
}

AxisRule::EventResult AxisRule::advance() {
    currentStep++;
    if (currentStep >= static_cast<int>(hotkeyParts.size())) {
        state = State::WaitingForRelease;
        currentStep = 0;
        return EventResult::Completed;
    }
    return EventResult::Advanced;
}

AxisRule::EventResult AxisRule::tryActivateFirstStep(const std::string& vidId,
                                                      int axisIndex,
                                                      const VidStateMap& vidState) {
    if (hotkeyParts.empty()) return EventResult::Ignored;
    const HotkeyPart& part = hotkeyParts[0];

    if (part.activationAxis.has_value()) {
        if (part.activationAxis->vidId != vidId ||
            part.activationAxis->axisIndex != axisIndex)
            return EventResult::Ignored;
        if (exclusive && !checkExactMatch(part, vidState)) return EventResult::Ignored;
    } else {
        bool isOurModifier = false;
        for (const auto& mod : part.modifiers) {
            if (mod.vidId == vidId && mod.axisIndex == axisIndex) {
                isOurModifier = true;
                break;
            }
        }
        if (!isOurModifier) return EventResult::Ignored;
        for (const auto& mod : part.modifiers) {
            auto vsIt = vidState.find(mod.vidId);
            if (vsIt == vidState.end()) return EventResult::Ignored;
            auto axIt = vsIt->second.find(mod.axisIndex);
            if (axIt == vsIt->second.end() || axIt->second == 0) return EventResult::Ignored;
        }
    }

    state = State::InProgress;
    currentStep = 0;
    return advance();
}

AxisRule::EventResult AxisRule::processAxisEvent(const std::string& vidId,
                                                  int axisIndex,
                                                  const VidStateMap& vidState) {
    if (hotkeyParts.empty() || state != State::InProgress) return EventResult::Ignored;
    const HotkeyPart& part = hotkeyParts[currentStep];

    for (const auto& mod : part.modifiers) {
        if (mod.vidId == vidId && mod.axisIndex == axisIndex)
            return EventResult::Ignored;
    }

    if (part.activationAxis.has_value() &&
        part.activationAxis->vidId == vidId &&
        part.activationAxis->axisIndex == axisIndex) {
        if (exclusive && !checkExactMatch(part, vidState)) {
            reset();
            return EventResult::Cancelled;
        }
        return advance();
    }

    bool inPartVid = std::find(part.involvedVids.begin(), part.involvedVids.end(), vidId)
                     != part.involvedVids.end();
    if (!inPartVid) return EventResult::Ignored;

    reset();
    return EventResult::Cancelled;
}

bool AxisRule::isReleaseEvent(const std::string& vidId, int axisIndex, int value) const {
    if (value != 0) return false;
    if (hotkeyParts.empty()) return false;
    const HotkeyPart& last = hotkeyParts.back();
    if (!last.activationAxis.has_value()) return false;
    return last.activationAxis->vidId == vidId &&
           last.activationAxis->axisIndex == axisIndex;
}
