// mainboard/src/OutputSequenceParser.cpp
#include "OutputSequenceParser.h"
#include <sstream>
#include <iostream>
#include <unordered_map>

static constexpr int DEFAULT_GAP_MS = 50;

ParsedSequence parseOutputSequence(const std::string& sequence) {
    ParsedSequence result;
    if (sequence.empty()) return result;

    // Split by whitespace
    std::vector<std::string> tokens;
    std::istringstream ss(sequence);
    std::string tok;
    while (ss >> tok) tokens.push_back(tok);

    // Parse tokens into raw actions + explicit waits
    struct RawToken {
        enum class Kind { Action, Wait } kind;
        std::string axisName;
        int value  = 0;
        int timeMs = 0;
    };
    std::vector<RawToken> raw;
    for (const auto& t : tokens) {
        if (t.empty()) continue;
        if (t[0] == '#') {
            int ms = 0;
            try { ms = std::stoi(t.substr(1)); }
            catch (...) { std::cerr << "[seq] invalid wait token: " << t << "\n"; continue; }
            RawToken rt;
            rt.kind   = RawToken::Kind::Wait;
            rt.timeMs = ms;
            raw.push_back(rt);
        } else if (t[0] == '!') {
            RawToken rt;
            rt.kind     = RawToken::Kind::Action;
            rt.axisName = t.substr(1);
            rt.value    = 1000;
            raw.push_back(rt);
        } else if (t[0] == '^') {
            RawToken rt;
            rt.kind     = RawToken::Kind::Action;
            rt.axisName = t.substr(1);
            rt.value    = 0;
            raw.push_back(rt);
        } else {
            auto eq = t.find('=');
            if (eq == std::string::npos) {
                std::cerr << "[seq] unrecognized token: " << t << "\n";
                continue;
            }
            RawToken rt;
            rt.kind     = RawToken::Kind::Action;
            rt.axisName = t.substr(0, eq);
            try { rt.value = std::stoi(t.substr(eq + 1)); }
            catch (...) {
                std::cerr << "[seq] invalid value in token: " << t << "\n";
                continue;
            }
            if (rt.value < 0) rt.value = 0;
            if (rt.value > 1000) rt.value = 1000;
            raw.push_back(rt);
        }
    }

    // Build final steps: insert gap between consecutive action tokens.
    int pendingWait = -1;
    bool lastWasAction = false;
    std::unordered_map<std::string, int> lastValues;

    auto flushGap = [&]() {
        if (!lastWasAction) return;
        int gap = (pendingWait >= 0) ? pendingWait : DEFAULT_GAP_MS;
        SequenceStep ws;
        ws.type   = SequenceStep::Type::Wait;
        ws.timeMs = gap;
        result.steps.push_back(ws);
        result.axisNames.push_back("");
        pendingWait   = -1;
        lastWasAction = false;
    };

    for (const auto& rt : raw) {
        if (rt.kind == RawToken::Kind::Wait) {
            if (lastWasAction) {
                pendingWait = rt.timeMs;
            }
        } else {
            flushGap();
            SequenceStep step;
            step.type      = SequenceStep::Type::SetAxis;
            step.axisIndex = -1;
            step.value     = rt.value;
            result.steps.push_back(step);
            result.axisNames.push_back(rt.axisName);
            lastValues[rt.axisName] = rt.value;
            lastWasAction = true;
            pendingWait   = -1;
        }
    }

    // Auto-unpress: axes whose last assigned value > 0
    for (const auto& [axisName, val] : lastValues) {
        if (val > 0) {
            SequenceStep step;
            step.type      = SequenceStep::Type::SetAxis;
            step.axisIndex = -1;
            step.value     = 0;
            result.steps.push_back(step);
            result.axisNames.push_back(axisName);
        }
    }

    return result;
}
