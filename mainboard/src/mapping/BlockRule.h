// mainboard/src/mapping/BlockRule.h
#pragma once
#include <string>
#include <vector>

struct BlockEntry {
    std::string axisName;   // retained for resolution
    int         axisIndex = -1;
    int         value     = 0;  // value to hold the axis at when blocked
};

struct BlockRule {
    std::string             vidId;
    std::vector<BlockEntry> entries;
};
