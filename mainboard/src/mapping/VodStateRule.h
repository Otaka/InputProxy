// mainboard/src/mapping/VodStateRule.h
#pragma once
#include <string>

enum class VodState { Active, Silenced, Disconnected };

struct VodStateRule {
    std::string vodId;
    VodState    state = VodState::Active;
};
