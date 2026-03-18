#pragma once
#include <string>
#include <unordered_map>

// vidId → { axisIndex → currentValue (0-1000) }
using VidStateMap = std::unordered_map<std::string, std::unordered_map<int, int>>;
