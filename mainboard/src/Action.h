#pragma once
#include <string>
#include <vector>
#include <memory>
#include "SequenceStep.h"

struct Action {
    virtual ~Action() = default;
};

struct EmitAxisAction : Action {
    std::string vodId;
    int         axisIndex = -1;  // resolved in phase 3
    std::string axisName;        // retained for resolution
    int         value     =  0;
};

struct OutputSequenceAction : Action {
    std::string               vodId;
    std::vector<SequenceStep> steps;
    std::vector<std::string>  axisNames; // parallel to steps; empty for Wait steps
};

struct SleepAction : Action {
    int timeMs = 0;
};
