// mainboard/src/OutputSequenceParser.h
#pragma once
#include <string>
#include <vector>
#include "SequenceStep.h"

struct ParsedSequence {
    std::vector<SequenceStep> steps;
    std::vector<std::string>  axisNames; // parallel; empty for Wait steps
};

// Parse a sequence string into steps.
// Axis indices in steps are set to -1 (resolved later via resolveSequence).
// Logs warnings to stderr on malformed tokens.
ParsedSequence parseOutputSequence(const std::string& sequence);
