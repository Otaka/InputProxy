// mainboard/src/mapping/TurboRule.h
#pragma once
#include <string>

enum class TurboCondition { WhileAxisActive, Always };

struct TurboRule {
    std::string    vidId;
    std::string    axisName;   // retained for resolution
    int            axisIndex   = -1;
    int            onMs        = 100;
    int            offMs       = 100;
    int            initialDelay = 0;
    int            maxValue    = 1000;
    int            minValue    = 0;
    TurboCondition condition   = TurboCondition::WhileAxisActive;
};
